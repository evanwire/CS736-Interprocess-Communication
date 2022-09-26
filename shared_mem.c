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
#include <assert.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "include.h"
#include "shared_mem.h"

#define SHM_FILE_PRIMARY_TO_SECONDARY "/736.shm_primary_to_secondary"
#define SHM_FILE_SECONDARY_TO_PRIMARY "/736.shm_secondary_to_primary"


typedef struct SharedMemoryMap {
    char *prim_to_sec;
    char *sec_to_prim;
} SharedMemoryMap;

void fill_shared_mem(volatile char *buffer, size_t size, char value) {
    for (int i = 0; i < size; i++) {
        buffer[i] = value;
    }
}

void run_secondary(int count, size_t read_io_size, size_t write_io_size, SharedMemoryMap sharedMemoryMap) {
    volatile char *sm_prim_to_sec = sharedMemoryMap.prim_to_sec;
    volatile char *sm_sec_to_prim = sharedMemoryMap.sec_to_prim;

    char expected_char;
    size_t last_read_idx = read_io_size - 1;

    for (int itr = 0; itr < count; itr++) {
        expected_char = (char) ((itr + 65) % 122);
        while (sm_prim_to_sec[last_read_idx] != expected_char) {} // Waiting for primary to write
        for (int i = 0; i <= last_read_idx; i++) {
            assert(sm_prim_to_sec[i] == expected_char);
        }
        fill_shared_mem(sm_sec_to_prim, write_io_size, expected_char);
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
        record_start(measurements);
        expected_char = (char) ((itr + 65) % 122);
        fill_shared_mem(sm_prim_to_sec, write_io_size, expected_char);
        while (sm_sec_to_prim[last_read_idx] != expected_char) {} // Waiting for secondary to write
        for (int i = 0; i <= last_read_idx; i++) {
            assert(sm_sec_to_prim[i] == expected_char);
        }
        record_end(measurements);
    }

    return measurements;
}

//void set_shared_mem_size(int sm_file_descriptor, off_t shared_mem_size) {
//    if (ftruncate(sm_file_descriptor, shared_mem_size) == -1) {
//        perror("failed to allocate memory");
//        shm_unlink(SHM_FILE_PRIMARY_TO_SECONDARY);
//        shm_unlink(SHM_FILE_SECONDARY_TO_PRIMARY);
//        exit(3);
//    }
//}

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
//    int fd_prim_to_sec = shm_open(SHM_FILE_PRIMARY_TO_SECONDARY, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
//    if (fd_prim_to_sec < 0) {
//        perror("failed to shm_open fd_prim_to_sec");
//    }
//    int fd_sec_to_prim = shm_open(SHM_FILE_SECONDARY_TO_PRIMARY, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
//    if (fd_sec_to_prim < 0) {
//        shm_unlink(SHM_FILE_PRIMARY_TO_SECONDARY);
//        perror("failed to shm_open sec_to_prim");
//        exit(2);
//    }

//    off_t shared_mem_size = (sizeof(char) * size);
//    set_shared_mem_size(fd_prim_to_sec, (off_t) message_io_size);
//    set_shared_mem_size(fd_sec_to_prim, (off_t) ack_io_size);
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
//    shm_unlink(SHM_FILE_PRIMARY_TO_SECONDARY);
//    shm_unlink(SHM_FILE_SECONDARY_TO_PRIMARY);

    return measurements;
}

void run_shared_mem_latency(int count, int size) {
    size_t io_size = (size * sizeof(char));

    Measurements *measurements = run_shared_mem(count, io_size, io_size);
    log_latency_results(measurements, count, size);
    free(measurements);
}

void run_shared_mem_bandwidth(int count, int size) {
    size_t message_io_size = (size * sizeof(char));
    size_t ack_size = 1;

    Measurements *measurements = run_shared_mem(count, message_io_size, ack_size);
    log_throughput_results(measurements, count, size);
    free(measurements);
}

