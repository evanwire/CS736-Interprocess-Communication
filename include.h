#ifndef INCLUDE_H
#define INCLUDE_H

typedef struct Measurements{
    unsigned long long start;
    unsigned long long minimum;
    unsigned long long maximum;
    unsigned long long total_t;
    int total_sent;
} Measurements;

unsigned long long get_now();
void init_measurements(Measurements*);
void record_start(Measurements*);
void record_end(Measurements *m);
void log_latency_results(Measurements *m, int count, int size);
void log_throughput_results(Measurements *m, int count, int size);

#endif