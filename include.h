#ifndef INCLUDE_H
#define INCLUDE_H
#include <time.h>

typedef struct Measurements{
    long minimum;
    long maximum;
    long long total_t;
    struct timespec start_t;
    struct timespec end_t;
    struct timespec exp_start;
    struct timespec exp_end;
    int total_sent;
} Measurements;

void get_now(struct timespec*);
void init_measurements(Measurements*);
void record(Measurements*);
void record_end(Measurements*);
void log_l(Measurements*, int, int);
void log_tp(Measurements*, int, int);

#endif