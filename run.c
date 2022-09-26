#include <stdlib.h>
#include <stdio.h>

#include "pipe.h"
#include "socket.h"
#include "shared_mem.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        perror("Requires two parameters: count and size");
        exit(1);
    }

    int count = atoi(argv[1]);
    int size = atoi(argv[2]);

    if (count < 0 || size < 0) {
        perror("Provide 2 positive values");
        exit(1);
    }

    // Run each experiment with these params
    run_pipe_latency(count, size);
    run_pipe_bandwidth(count, size);
    run_socket_latency(count, size);
    run_socket_bandwidth(count, size);
    run_shared_mem_latency(count, size);
    run_shared_mem_bandwidth(count, size);
    return 0;
}