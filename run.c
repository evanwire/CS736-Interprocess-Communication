#include<stdlib.h>
#include<stdio.h>
#include "./pipe.h"
#include "./socket.h"
#include "./shared_mem.h"

int main(int argc, char* argv[]){
    //Parse args
    // if(argc != 3){
    //     perror("./a.out COUNT SIZE");
    //     exit(1);
    // }

    // int count = atoi(argv[1]);
    // int size = atoi(argv[2]);

    // if(size < 0 || count < 0){
    //     perror("Provide 2 positive values");
    //     exit(1);
    // }



    // // Run each experiment with these params
    // printf("-------Pipes-------\n");
    // run_experiment__p(count, size, 0); // 0 is tp
    // run_experiment__p(count, size, 1); // 1 is latency
    // printf("\n-------Sockets-------\n");
    // run_experiment__s(count, size, 0);
    // run_experiment__s(count, size, 1);
    // printf("\n-------Shm-------\n");
    run_experiment__sm(1000, 256, 0);
    //run_experiment__sm(count, size, 1);
}