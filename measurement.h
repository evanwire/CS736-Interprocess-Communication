#include <time.h>

#ifndef INCLUDE_H
#define INCLUDE_H

typedef struct Measurements{
    long minimum;
    long maximum;
    long long total_t;
    struct timespec time_s;
    struct timespec time_e;
    int total_sent;
} Measurements;

void get_time(struct timespec*);
void init_measurements(Measurements*);
void record_start(Measurements*);
void record_end(Measurements *m);
void log_latency_results(Measurements *m);
void log_throughput_results(Measurements *m, int count, int size);

#endif