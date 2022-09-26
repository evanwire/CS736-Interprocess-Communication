#include<stdlib.h>
#include<stdio.h>
#include "./pipe.h"
#include "./socket.h"
#include "./shared_mem.h"

int main(int argc, char* argv[]){
    //Parse args
    if(argc != 3){
        perror("Requires two parameters: count and size");
        exit(1);
    }

    int count = atoi(argv[1]);
    int size = atoi(argv[2]);

    if(count < 0 || size < 0){
        perror("Provide 2 positive values");
        exit(1);
    }


    // Run each experiment with these params
    run_experiment__s(count, size);
    run_experiment__p(count, size);
    run_experiment__sm(count, size);
}