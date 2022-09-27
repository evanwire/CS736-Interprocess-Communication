
#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <stdio.h>


#include "./include.h"

#define STC 0x1234
#define CTS 0x4321

void client(int count, int size, int mode){
    int stc_id = shmget(STC, size, IPC_CREAT | 0666);
    if (stc_id < 0) {
		perror("shmget");
        exit(0);
	}
    char* stc_shm = (char*)shmat(stc_id, NULL, 0);
    if (stc_shm < (char*)0) {
		perror("shmat");
        exit(0);
	}
    int cts_id = shmget(CTS, size, IPC_CREAT | 0666);
    if (cts_id < 0) {
		perror("shmget");
        exit(0);
	}
    char* cts_shm = (char*)shmat(cts_id, NULL, 0);
    if (cts_shm < (char*)0) {
		perror("shmat");
        exit(0);
	}

    void* buff = malloc(size);
    int return_msg_size = mode ? size : 1;

    for(int i = 1; i <= count; i++){
        while (stc_shm[size-1] != '0' + i) {}
        memcpy(buff, stc_shm, size);
        memset(cts_shm, '0' + i, return_msg_size);
    }

    free(buff);
    if (shmdt(cts_shm) == -1) {
        perror("shmdt");
        exit(0);
    }
    if (shmdt(stc_shm) == -1) {
        perror("shmdt");
        exit(0);
    }
}

void server(int count, int size, int mode){
    int stc_id = shmget(STC, size, IPC_CREAT | 0666);
    if (stc_id < 0) {
		perror("shmget");
        exit(0);
	}
    char* stc_shm = (char*)shmat(stc_id, NULL, 0);
    if (stc_shm < (char*)0) {
		perror("shmat");
        exit(0);
	}
    int cts_id = shmget(CTS, size, IPC_CREAT | 0666);
    if (cts_id < 0) {
		perror("shmget");
        exit(0);
	}
    char* cts_shm = (char*)shmat(cts_id, NULL, 0);
    if (cts_shm < (char*)0) {
		perror("shmat");
        exit(0);
	}

    int return_msg_size = mode ? size : 1;
    void* buff = malloc(return_msg_size);
    Measurements* m = malloc(sizeof(Measurements));
    init_measurements(m);
    
    
    for(int i = 1; i <= count; i++){
        memset(stc_shm, '0' + i, size);      
        while (cts_shm[return_msg_size-1] != '0' + i) {}
        memcpy(buff, cts_shm, return_msg_size);
        record(m);
    }
    record_end(m);

    mode ? log_l(m, count, size) : log_tp(m, count, size);
    free(buff);
    free(m);

    if (shmdt(cts_shm) == -1) {
        perror("shmdt");
        exit(0);
    }
        if (shmdt(stc_shm) == -1) {
        perror("shmdt");
        exit(0);
    }
}

void run_experiment__sm(int count, int size, int mode){
    pid_t pid;
    if((pid = fork()) == -1){
        perror("Fork");
    }

    // Parent is server, child is client
    if(pid != 0){
        server(count, size, mode);
        waitpid(pid, NULL, 0);
    }else{
        client(count, size, mode);
        exit(0);
    }
}