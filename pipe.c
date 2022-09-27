#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/wait.h>

#include "pipe.h"
#include "common.h"
#include "measurement.h"


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

    // Parent is primary, Child is secondary
    Measurements *measurements;
    if (pid != 0) {
        measurements = run_fd_primary(count, ack_io_size, message_io_size, pipefd_primary_to_secondary[1],
                                      pipefd_secondary_to_primary[0]);
        waitpid(pid, NULL, 0);

    } else {
        run_fd_secondary(count, message_io_size, ack_io_size, pipefd_primary_to_secondary[0],
                         pipefd_secondary_to_primary[1]);
        exit(0);
    }

    close(pipefd_primary_to_secondary[0]);
    close(pipefd_primary_to_secondary[1]);
    close(pipefd_secondary_to_primary[0]);
    close(pipefd_secondary_to_primary[1]);

    return measurements;
}

void run_pipe_latency(int count, int size) {
    size_t io_size = (size * sizeof(char));

    Measurements *measurements = run_pipe(count, io_size, io_size);
    log_latency_results(measurements);
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

