#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>


#include "./include.h"


// client hears from server, writes same message back
void client_p(int s[2], int r[2], int count, int size, Measurements* m, int mode){
    //close unnessary fd's
    close(s[0]);
    close(r[1]);

    // init msg and buffer
    int return_msg_size = mode ? size : 1;
    char msg[return_msg_size];
    memset(msg, 'A', return_msg_size);
    void* buff = malloc(size * sizeof(char));
   
    for(int i = 0; i < count; i++){
        read(r[0], buff, size * sizeof(char));
        write(s[1], msg, return_msg_size * sizeof(char));
    }

    // Close any other fds and free
    close(s[1]);
    close(r[0]);
    free(buff);
}

// Server sends msg to client, client sends an ack
void server_p(int s[2], int r[2], int count, int size, Measurements* m, int mode){
    // Close unnesesary fd's
    close(s[0]);
    close(r[1]);

    // init msg and buffer
    char msg[size];
    memset(msg, 'A', size);
    int return_msg_size = mode ? size : 1;
    void* buff = malloc(return_msg_size * sizeof(char));
    

    for(int i = 0; i < count; i++){
        write(s[1], msg, size);
        read(r[0], buff, return_msg_size * sizeof(char));
        record(m);
    }
    record_end(m);


    // Close any other fds and free
    close(s[1]);
    close(r[0]);
    free(buff);
}

// We send count messages, size characters(bytes) long
void run_experiment__p(int count, int size, int mode){
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
        server_p(client_send, server_send, count, size, m, mode);
        waitpid(pid, NULL, 0);
    }else{
        client_p(server_send, client_send, count, size, m, mode);
        exit(0);
    }

    mode ? log_l(m, count, size) : log_tp(m, count, size);
    free(m);
}
