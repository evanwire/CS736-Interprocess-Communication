// References:
// https://opensource.com/article/19/4/interprocess-communication-linux-networking

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "socket.h"
#include "measurement.h"

#define PORT 9006
#define MAX_QUEUED_REQUESTS 10

void run_client(int count, int port, size_t read_io_size, size_t write_io_size) {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Failed to create client socket");
        exit(2);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_fd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("Failed to connect to server");
        close(client_fd);
    }

    int tcp_nodelay_value = 1;
    setsockopt(client_fd, SOL_TCP, TCP_NODELAY, (void *) &tcp_nodelay_value, sizeof(tcp_nodelay_value));

    char *read_buffer = malloc(read_io_size);
    char *write_buffer = malloc(write_io_size);

    for (int itr = 0; itr < count; itr++) {
        char expected_char = (char)((itr+65) % 122);
        memset(write_buffer, expected_char, write_io_size);

        if(write(client_fd, write_buffer, write_io_size) < 0) {
            perror("write failed");
            break;
        }

        int offset = 0;
        while(1) {
            int result = read(client_fd, &(read_buffer[offset]), read_io_size - offset);
            if(result == 0) {break;}
            if(result < 0) {perror("read failed"); exit(5);}
            offset += result;
        }
        for (int i = 0; i <= read_io_size-1; i++) {
            assert(read_buffer[i] == expected_char);
        }
    }
    close(client_fd);
    free(read_buffer);
    free(write_buffer);
}

Measurements *run_server(int count, int port, size_t read_io_size, size_t write_io_size) {
    int enable = 1;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create server socket\n");
        exit(2);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    if (bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind server socket\n");
        close(server_fd);
        exit(3);
    }

    int ret = listen(server_fd, MAX_QUEUED_REQUESTS);
    if (ret < 0) {
        perror("Failed to listen on server socket\n");
        close(server_fd);
        exit(4);
    }

    struct sockaddr_in client_address;
    int client_address_len = sizeof(client_address);
    int client_fd = accept(server_fd, (struct sockaddr *) &client_address, (socklen_t*)&client_address_len);

    if (client_fd < 0) {
        perror("Failed accept client connection\n");
        close(server_fd);
        exit(5);
    }

    setsockopt(server_fd, SOL_TCP, TCP_NODELAY, &enable, sizeof(enable));
    setsockopt(client_fd, SOL_TCP, TCP_NODELAY, &enable, sizeof(enable));

    char *read_buffer = malloc(read_io_size);
    char *write_buffer = malloc(write_io_size);

    Measurements *measurements = malloc(sizeof(Measurements));
    init_measurements(measurements);

    for (int itr = 0; itr < count; itr++) {
        record_start(measurements);
        char expected_char = (char)((itr+65) % 122);

        int offset = 0;
        while(1) {
            int result = read(client_fd, &(read_buffer[offset]), read_io_size - offset);
            if(result == 0) {break;}
            if(result < 0) {perror("read failed"); exit(5);}
            offset += result;
        }
        for (int i = 0; i <= read_io_size-1; i++) {
            assert(read_buffer[i] == expected_char);
        }

        memset(write_buffer, expected_char, write_io_size);
        if(write(client_fd, write_buffer, write_io_size) < 0) {
            perror("write failed");
        }

        record_end(measurements);
    }
    close(client_fd);
    close(server_fd);
    free(read_buffer);
    free(write_buffer);

    return measurements;
}

Measurements * run_socket(int count, int port, size_t message_io_size, size_t ack_io_size) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork");
    }

    Measurements *measurements;
    // Parent is server, Child is client
    if (pid != 0) {
        measurements = run_server(count, port, message_io_size, ack_io_size);
        waitpid(pid, NULL, 0);

    } else {
        run_client(count, port, ack_io_size, message_io_size);
        exit(0);
    }

    return measurements;
}

void run_socket_latency(int count, int size) {
    size_t io_size = (size * sizeof(char));

    Measurements *measurements = run_socket(count, PORT, io_size, io_size);
    log_latency_results(measurements);
    free(measurements);
}

void run_socket_bandwidth(int count, int size) {
    size_t message_io_size = (size * sizeof(char));
    size_t ack_size = 1;

    Measurements *measurements = run_socket(count, PORT, message_io_size, ack_size);
    log_throughput_results(measurements, count, size);
    free(measurements);
}
