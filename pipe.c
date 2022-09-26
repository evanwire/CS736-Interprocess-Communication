#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/wait.h>

#include "pipe.h"
#include "include.h"


void run_pipe_secondary(int count, size_t read_io_size, size_t write_io_size,
                        const int fd_primary_to_secondary[2], const int fd_secondary_to_primary[2]) {

    close(fd_secondary_to_primary[0]);
    int fd_w_secondary = fd_secondary_to_primary[1];
    int fd_r_primary = fd_primary_to_secondary[0];
    close(fd_primary_to_secondary[1]);
    char *read_buffer = malloc(read_io_size);
    char *write_buffer = malloc(write_io_size);

    for (int itr = 0; itr < count; itr++) {
        if(read(fd_r_primary, read_buffer, read_io_size) < 0) {
            perror("read failed\n");
            break;
        }
        memset(write_buffer, ((itr+65) % 122), write_io_size);
        if(write(fd_w_secondary, write_buffer, write_io_size) < 0) {
            perror("write failed\n");
            break;
        }
    }

    close(fd_secondary_to_primary[1]);
    close(fd_primary_to_secondary[0]);

    free(read_buffer);
    free(write_buffer);
}

Measurements *run_pipe_primary(int count, size_t read_io_size, size_t write_io_size,
                               const int fd_primary_to_secondary[2], const int fd_secondary_to_primary[2]) {

    close(fd_primary_to_secondary[0]);
    int fd_w_primary = fd_primary_to_secondary[1];
    int fd_r_secondary = fd_secondary_to_primary[0];
    close(fd_secondary_to_primary[1]);

    Measurements *measurements = malloc(sizeof(Measurements));
    init_measurements(measurements);

    char *read_buffer = malloc(read_io_size);
    char *write_buffer = malloc(write_io_size);

    for (int itr = 0; itr < count; itr++) {
        memset(write_buffer, ((itr+65) % 122), write_io_size);
        record_start(measurements);
        if(write(fd_w_primary, write_buffer, write_io_size) < 0) {
            perror("write failed\n");
            break;
        }
        if(read(fd_r_secondary, read_buffer, read_io_size) < 0) {
            perror("read failed\n");
            break;
        }
        record_end(measurements);
    }

    close(fd_primary_to_secondary[1]);
    close(fd_secondary_to_primary[0]);
    return measurements;
}

Measurements *run_pipe(int count, size_t message_io_size, size_t ack_io_size) {
    int pipefd_primary_to_secondary[2];
    int pipefd_secondary_to_primary[2];
    if (pipe(pipefd_primary_to_secondary) == -1) {
        perror("failed to create pipe primary_to_secondary");
        exit(2);
    }
    if (pipe(pipefd_secondary_to_primary) == -1) {
        perror("failed to create pipe secondary_to_primary");
        close(pipefd_primary_to_secondary[0]);
        close(pipefd_primary_to_secondary[1]);
        exit(2);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork");
    }

    Measurements *measurements;
    // Parent is primary, Child is secondary
    if (pid != 0) {
        measurements = run_pipe_primary(count, ack_io_size, message_io_size, pipefd_primary_to_secondary, pipefd_secondary_to_primary);
        waitpid(pid, NULL, 0);

    } else {
        run_pipe_secondary(count, message_io_size, ack_io_size, pipefd_primary_to_secondary, pipefd_secondary_to_primary);
        exit(0);
    }

    return measurements;
}

void run_pipe_latency(int count, int size) {
    size_t io_size = (size * sizeof(char));

    Measurements *measurements = run_pipe(count, io_size, io_size);
    log_latency_results(measurements, count, size);
    free(measurements);
}

// We send count messages, size characters(bytes) long, with 1 byte acks
void run_pipe_bandwidth(int count, int size) {
    size_t message_io_size = (size * sizeof(char));
    size_t ack_size = 1;

    Measurements *measurements = run_pipe(count, message_io_size, ack_size);
    log_throughput_results(measurements, count, size);
    free(measurements);
}

