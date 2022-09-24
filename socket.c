#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>



#include "./include.h"

#define PORT 6000


int server_fd;
int client_fd;

Measurements* m;



void signal_handler(int signal_number) {
    close(server_fd);
    exit(0);
}

void close_fds(){
    close(server_fd);
    close(client_fd);
}

void client_s(int count, int size){
    int sock = 0;
    struct sockaddr_in server_addr;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("create socket");
        close_fds();
        exit(0);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if((client_fd = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0){
        perror("connect");
        close_fds();
        exit(0);
    }

    char msg[size];
    memset(msg, 'A', size);
    void* buff = malloc(size * sizeof(char));

    for(int i = 0; i < count; i++){
        read(sock, buff, size);
        if(write(sock, msg, size) == -1){
            perror("send");
            close_fds();
            exit(0);
        }
    }
    free(buff);
}

void server_s(int count, int size){
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket creation");
        close_fds();
        exit(0);
    }

    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0){
        perror("bind");
        close_fds();
        exit(0);
    }
    if(listen(server_fd, 5) != 0){
        perror("listen");
        close_fds();
        exit(0);
    }
     if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        close_fds();
        exit(1);
    }
    struct sockaddr_in client_addr;
    int addrlen = sizeof(client_addr);

    int connection_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen);
    if (connection_fd < 0) {
        perror("accept");
        close_fds();
        exit(0);
    }

    // Begin transmit
    char msg[size];
    memset(msg, 'A', size);
    void* buff = malloc(size * sizeof(char));
    m = malloc(sizeof(Measurements));
    init_measurements(m); // Do this here so socket setup doesn't affect time
    for(int i = 0; i < count; i++){
        if(write(connection_fd, msg, size) == -1){
            perror("send");
            close_fds();
            exit(0);
        }
        read(connection_fd, buff, size);
        record(m);
    }
    free(buff);

}

void run_experiment__s(int count, int size){

    pid_t pid;
    if((pid = fork()) == -1){
        perror("Fork");
    }

    // Parent is server, child is client
    if(pid != 0){
        server_s(count, size);
        waitpid(pid, NULL, 0);
    }else{
        client_s(count, size);
        exit(0);
    }

    log_results(m, count, size);
    free(m);
    close_fds();

}