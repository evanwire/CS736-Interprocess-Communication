#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "common.h"
#include "measurement.h"

void print_progress(int itr, int count) {
    if(itr > 0 && itr % 1000==0) {
        printf("\r %d of %d (%d percent).", itr, count, (100*itr)/count);
        fflush(stdout);
    }
}

void fill(char *buffer, char value, size_t size) {
    for (int itr = 0; itr < size; itr++) {
        buffer[itr] = value;
    }
}

void v_fill(volatile char *buffer, char value, size_t size) {
    for (int itr = 0; itr < size; itr++) {
        buffer[itr] = value;
    }
}

void verify(const char *buffer, char expected_value, size_t size) {
    for (int itr = 0; itr < size; itr++) {
        assert(buffer[itr] == expected_value);
    }
}

void v_verify(const volatile char *buffer, char expected_value, size_t size) {
    for (int itr = 0; itr < size; itr++) {
        assert(buffer[itr] == expected_value);
    }
}

void run_fd_secondary(int count, size_t read_io_size, size_t write_io_size, int fd_r_primary, int fd_w_secondary) {
    char *read_buffer = malloc(read_io_size);
    char *write_buffer = malloc(write_io_size);

    for (int itr = 0; itr < count; itr++) {
        char expected_char = (char) ((itr + 65) % 122);
        int offset = 0;
        while (1) {
            int result = read(fd_r_primary, &(read_buffer[offset]), read_io_size - offset);
            if (result == 0) { break; }
            if (result < 0) {
                perror("read failed");
                exit(5);
            }
            offset += result;
        }
        verify(read_buffer, expected_char, read_io_size);

        fill(write_buffer, expected_char, write_io_size);
        if (write(fd_w_secondary, write_buffer, write_io_size) < 0) {
            perror("write failed\n");
            break;
        }
    }

    free(read_buffer);
    free(write_buffer);
}

Measurements *run_fd_primary(int count, size_t read_io_size, size_t write_io_size, int fd_w_primary, int fd_r_secondary) {
    Measurements *measurements = malloc(sizeof(Measurements));
    init_measurements(measurements);

    char *read_buffer = malloc(read_io_size);
    char *write_buffer = malloc(write_io_size);

    for (int itr = 0; itr < count; itr++) {
//        print_progress(itr, count);
        record_start(measurements);
        char expected_char = (char) ((itr + 65) % 122);
        fill(write_buffer, expected_char, write_io_size);

        if (write(fd_w_primary, write_buffer, write_io_size) < 0) {
            perror("write failed\n");
            break;
        }
        int offset = 0;
        while (1) {
            int result = read(fd_r_secondary, &(read_buffer[offset]), read_io_size - offset);
            if (result == 0) { break; }
            if (result < 0) {
                perror("read failed");
                exit(5);
            }
            offset += result;
        }
        verify(read_buffer, expected_char, read_io_size);

        record_end(measurements);
    }
//    printf("\n");

    return measurements;
}
