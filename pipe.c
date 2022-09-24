#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>


#include "./include.h"


// client hears from server, writes same message back
void client(int s[2], int r[2], int count, int size, Measurements* m){
    //close unnessary fd's
    close(s[0]);
    close(r[1]);

    // init msg and buffer
    char msg[size];
    memset(msg, 'A', size);
    void* buff = malloc(size * sizeof(char));
   
    for(int i = 0; i < count; i++){
        read(r[0], buff, size * sizeof(char));
        write(s[1], msg, size * sizeof(char));
    }

    // Close any other fds and free
    close(s[1]);
    close(r[0]);
    free(buff);
}

// Server sends msg to client, client sends an ack
void server(int s[2], int r[2], int count, int size, Measurements* m){
    // Close unnesesary fd's
    close(s[0]);
    close(r[1]);

    // init msg and buffer
    char msg[size];
    memset(msg, 'A', size);
    void* buff = malloc(size * sizeof(char));
    

    for(int i = 0; i < count; i++){
        write(s[1], msg, size);
        read(r[0], buff, size * sizeof(char));
        record(m);
    }

    // Close any other fds and free
    close(s[1]);
    close(r[0]);
    free(buff);
}

// We send count messages, size characters(bytes) long
void run_experiment__p(int count, int size){
    // Create pipes
    int server_send[2];

    if(pipe(server_send) < 0){
        perror("Pipe creation");
        return;
    }

    int client_send[2];

    if(pipe(client_send) < 0){
        perror("Pipe creation");
        close(server_send[0]);
        close(server_send[1]);
        return;
    }

    // Init measurements
    Measurements* m = malloc(sizeof(Measurements));
    init_measurements(m);

    pid_t pid;
    if((pid = fork()) == -1){
        perror("Fork");
    }

    // Parent is server, child is client
    if(pid != 0){
        server(client_send, server_send, count, size, m);
        waitpid(pid, NULL, 0);
    }else{
        client(server_send, client_send, count, size, m);
        exit(0);
    }

    log_results(m, count, size);
    free(m);
}
