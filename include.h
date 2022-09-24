#ifndef INCLUDE_H
#define INCLUDE_H

typedef struct Measurements{
    unsigned long long start;
    unsigned long long minimum;
    unsigned long long maximum;
    unsigned long long total_t;
    unsigned long long total_sent;
} Measurements;

unsigned long long get_now();
void init_measurements(Measurements*);
void record(Measurements*);
void log_results(Measurements*, int, int);

#endif