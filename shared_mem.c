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
#include <sys/stat.h>
#include <sys/wait.h>

#include "include.h"
#include "shared_mem.h"

#define SHM_FILE_PRIMARY_TO_SECONDARY "/736.shm_primary_to_secondary"
#define SHM_FILE_SECONDARY_TO_PRIMARY "/736.shm_secondary_to_primary"


typedef struct SharedMemoryMap {
    char *prim_to_sec;
    char *sec_to_prim;
} SharedMemoryMap;


SharedMemoryMap memory_map_sm_file_descriptors(int fd_prim_to_sec, int fd_sec_to_prim, off_t mmap_size) {
    char *sm_prim_to_sec = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_prim_to_sec, 0);
    if (sm_prim_to_sec == MAP_FAILED) {
        printf("Failed to map prim_to_sec memory");
        exit(4);
    }
    char *sm_sec_to_prim = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_sec_to_prim, 0);
    if (sm_sec_to_prim == MAP_FAILED) {
        printf("Failed to map sec_to_prim memory");
        munmap(sm_prim_to_sec, mmap_size);
        exit(4);
    }
    SharedMemoryMap memoryMap = {.prim_to_sec = sm_prim_to_sec, .sec_to_prim = sm_sec_to_prim};
    return memoryMap;
}

void run_secondary(int count, int size, int fd_prim_to_sec, int fd_sec_to_prim, off_t sm_size) {
    SharedMemoryMap memoryMap = memory_map_sm_file_descriptors(fd_prim_to_sec, fd_sec_to_prim, sm_size);
    char *sm_prim_to_sec = memoryMap.prim_to_sec;
    char *sm_sec_to_prim = memoryMap.sec_to_prim;
    char *buffer = malloc(sm_size);
    off_t last_idx = sm_size - 1;

    for (int itr = 1; itr <= count; itr++) {
        while (sm_prim_to_sec[last_idx] != '0' + itr) {} // Waiting for primary to write
        memcpy(buffer, sm_prim_to_sec, sm_size);
        memset(sm_sec_to_prim, '0' + itr, size);
    }

    munmap(sm_prim_to_sec, sm_size);
    munmap(sm_sec_to_prim, sm_size);
    free(buffer);
}

Measurements *run_primary(int count, int size, int fd_prim_to_sec, int fd_sec_to_prim, off_t sm_size) {
    SharedMemoryMap memoryMap = memory_map_sm_file_descriptors(fd_prim_to_sec, fd_sec_to_prim, sm_size);
    char *sm_prim_to_sec = memoryMap.prim_to_sec;
    char *sm_sec_to_prim = memoryMap.sec_to_prim;
    char *buffer = malloc(sm_size);
    off_t last_idx = sm_size - 1;

    Measurements *measurements = malloc(sizeof(Measurements));
    init_measurements(measurements);

    for (int itr = 1; itr <= count; itr++) {
        memset(sm_prim_to_sec, '0' + itr, size);
        while (sm_sec_to_prim[last_idx] != '0' + itr) {} // Waiting for secondary to write
        memcpy(buffer, sm_sec_to_prim, sm_size);
        record(measurements);
    }

    munmap(sm_prim_to_sec, sm_size);
    munmap(sm_sec_to_prim, sm_size);
    free(buffer);

    return measurements;
}

void set_shared_mem_size(int sm_file_descriptor, off_t shared_mem_size) {
    if (ftruncate(sm_file_descriptor, shared_mem_size) == -1) {
        perror("failed to allocate memory");
        shm_unlink(SHM_FILE_PRIMARY_TO_SECONDARY);
        shm_unlink(SHM_FILE_SECONDARY_TO_PRIMARY);
        exit(3);
    }
}

// We send count messages, size characters(bytes) long
void run_experiment__sm(int count, int size) {
    int fd_prim_to_sec = shm_open(SHM_FILE_PRIMARY_TO_SECONDARY, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd_prim_to_sec < 0) {
        perror("failed to shm_open fd_prim_to_sec");
    }
    int fd_sec_to_prim = shm_open(SHM_FILE_SECONDARY_TO_PRIMARY, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd_sec_to_prim < 0) {
        shm_unlink(SHM_FILE_PRIMARY_TO_SECONDARY);
        perror("failed to shm_open sec_to_prim");
        exit(2);
    }

    off_t shared_mem_size = (sizeof(char) * size);
    set_shared_mem_size(fd_prim_to_sec, shared_mem_size);
    set_shared_mem_size(fd_sec_to_prim, shared_mem_size);

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork");
    }

    Measurements *measurements;
    // Parent is primary, Child is secondary
    if (pid != 0) {
        measurements = run_primary(count, size, fd_prim_to_sec, fd_sec_to_prim, shared_mem_size);
        waitpid(pid, NULL, 0);

    } else {
        run_secondary(count, size, fd_prim_to_sec, fd_sec_to_prim, shared_mem_size);
        exit(0);
    }

    shm_unlink(SHM_FILE_PRIMARY_TO_SECONDARY);
    shm_unlink(SHM_FILE_SECONDARY_TO_PRIMARY);

    log_results(measurements, count, size);
    free(measurements);
}
