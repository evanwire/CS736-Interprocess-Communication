// References:
// https://opensource.com/article/19/4/interprocess-communication-linux-networking

#include <stdio.h>
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
#include "common.h"
#include "measurement.h"

#define PORT 9006
#define MAX_QUEUED_REQUESTS 10


Measurements *run_socket(int count, int port, size_t message_io_size, size_t ack_io_size) {
    int enable = 1;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create server socket");
        exit(2);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    if (bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind server socket");
        close(server_fd);
        exit(3);
    }

    int ret = listen(server_fd, MAX_QUEUED_REQUESTS);
    if (ret < 0) {
        perror("Failed to listen on server socket");
        close(server_fd);
        exit(4);
    }


    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Failed to create client socket");
        exit(2);
    }
    struct sockaddr_in c_server_address;
    memset(&server_address, 0, sizeof(server_address));
    c_server_address.sin_family = AF_INET;
    c_server_address.sin_port = htons(port);
    c_server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_fd, (struct sockaddr *) &c_server_address, sizeof(c_server_address)) < 0) {
        perror("Failed to connect to server");
        close(client_fd);
    }

    struct sockaddr_in client_address;
    int client_address_len = sizeof(client_address);
    int s_client_fd = accept(server_fd, (struct sockaddr *) &client_address, (socklen_t *) &client_address_len);

    if (s_client_fd < 0) {
        perror("Failed accept client connection");
        close(server_fd);
        close(client_fd);
        exit(5);
    }

    setsockopt(server_fd, SOL_TCP, TCP_NODELAY, &enable, sizeof(enable));
    setsockopt(s_client_fd, SOL_TCP, TCP_NODELAY, &enable, sizeof(enable));
    setsockopt(client_fd, SOL_TCP, TCP_NODELAY, &enable, sizeof(enable));

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork");
    }

    // Parent is server, Child is client
    Measurements *measurements;
    if (pid != 0) {
        measurements = run_fd_primary(count, ack_io_size, message_io_size, s_client_fd, s_client_fd);
        waitpid(pid, NULL, 0);

    } else {
        run_fd_secondary(count, message_io_size, ack_io_size, client_fd, client_fd);
        exit(0);
    }

    close(server_fd);
    close(s_client_fd);
    close(client_fd);

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
