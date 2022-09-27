// References:
// https://www.kernel.org/doc/gorman/html/understand/understand015.html
// https://man7.org/linux/man-pages/man3/shm_open.3.html (shm_open man page)
// https://man7.org/linux/man-pages/man2/mmap.2.html (mmap man page)
// https://stackoverflow.com/questions/21311080/linux-shared-memory-shmget-vs-mmap

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "common.h"
#include "shared_mem.h"
#include "measurement.h"


typedef struct SharedMemoryMap {
    char *prim_to_sec;
    char *sec_to_prim;
} SharedMemoryMap;


void run_secondary(int count, size_t read_io_size, size_t write_io_size, SharedMemoryMap sharedMemoryMap) {
    volatile char *sm_prim_to_sec = sharedMemoryMap.prim_to_sec;
    volatile char *sm_sec_to_prim = sharedMemoryMap.sec_to_prim;

    char expected_char;
    size_t last_read_idx = read_io_size - 1;

    for (int itr = 0; itr < count; itr++) {
        expected_char = (char) ((itr + 65) % 122);

        while (sm_prim_to_sec[last_read_idx] != expected_char) {} // Waiting for primary to write
        v_verify(sm_prim_to_sec, expected_char, read_io_size);

        v_fill(sm_sec_to_prim, expected_char, write_io_size);
    }
}

Measurements *run_primary(int count, size_t read_io_size, size_t write_io_size, SharedMemoryMap sharedMemoryMap) {
    volatile char *sm_prim_to_sec = sharedMemoryMap.prim_to_sec;
    volatile char *sm_sec_to_prim = sharedMemoryMap.sec_to_prim;

    Measurements *measurements = malloc(sizeof(Measurements));
    init_measurements(measurements);

    char expected_char;
    size_t last_read_idx = read_io_size - 1;
    for (int itr = 0; itr < count; itr++) {
//        print_progress(itr, count);
        record_start(measurements);

        expected_char = (char) ((itr + 65) % 122);

        v_fill(sm_prim_to_sec, expected_char, write_io_size);

        while (sm_sec_to_prim[last_read_idx] != expected_char) {} // Waiting for secondary to write
        v_verify(sm_sec_to_prim, expected_char, read_io_size);

        record_end(measurements);
    }

//    printf("\n");
    return measurements;
}

SharedMemoryMap memory_map_sm_file_descriptors(off_t message_io_size, off_t ack_io_size) {
    // Map Anonymous eliminates the need for using shm_open or opening a file in /dev/shm
    char *sm_prim_to_sec = mmap(NULL, message_io_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sm_prim_to_sec == MAP_FAILED) {
        printf("Failed to map prim_to_sec memory");
        exit(4);
    }
    char *sm_sec_to_prim = mmap(NULL, ack_io_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sm_sec_to_prim == MAP_FAILED) {
        printf("Failed to map sec_to_prim memory");
        munmap(sm_prim_to_sec, message_io_size);
        exit(4);
    }
    SharedMemoryMap memoryMap = {.prim_to_sec = sm_prim_to_sec, .sec_to_prim = sm_sec_to_prim};
    return memoryMap;
}

Measurements *run_shared_mem(int count, size_t message_io_size, size_t ack_io_size) {
    SharedMemoryMap sharedMemoryMap = memory_map_sm_file_descriptors((off_t) message_io_size, (off_t) ack_io_size);

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork");
    }

    Measurements *measurements;
    // Parent is primary, Child is secondary
    if (pid != 0) {
        measurements = run_primary(count, ack_io_size, message_io_size, sharedMemoryMap);
        waitpid(pid, NULL, 0);

    } else {
        run_secondary(count, message_io_size, ack_io_size, sharedMemoryMap);
        exit(0);
    }

    munmap(sharedMemoryMap.prim_to_sec, message_io_size);
    munmap(sharedMemoryMap.sec_to_prim, ack_io_size);

    return measurements;
}

void run_shared_mem_latency(int count, int size) {
    size_t io_size = (size * sizeof(char));

    Measurements *measurements = run_shared_mem(count, io_size, io_size);
    log_latency_results(measurements);
    free(measurements);
}

void run_shared_mem_bandwidth(int count, int size) {
    size_t message_io_size = (size * sizeof(char));
    size_t ack_size = 1;

    Measurements *measurements = run_shared_mem(count, message_io_size, ack_size);
    log_throughput_results(measurements, count, size);
    free(measurements);
}

