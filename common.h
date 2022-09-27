#include <sys/types.h>

#include "measurement.h"

#ifndef CS736_COMMON_H
#define CS736_COMMON_H

void print_progress(int, int);

void fill(char *, char, size_t);

void v_fill(volatile char *, char, size_t);

void verify(const char *, char, size_t);

void v_verify(const volatile char *, char, size_t);

Measurements *run_fd_primary(int, size_t, size_t, int, int);

void run_fd_secondary(int, size_t, size_t, int, int);

#endif //CS736_COMMON_H_H
