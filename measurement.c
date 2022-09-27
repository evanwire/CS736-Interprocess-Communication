#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "measurement.h"


void get_time(struct timespec *ts) {
    int result = clock_gettime(CLOCK_REALTIME, ts);
    if (result < 0) {
        perror("Failed to get time");
        exit(10);
    }
}

// Constructs a Measurements struct
void init_measurements(Measurements *m) {
    m->minimum = LONG_MAX;
    m->maximum = 0;
    m->total_sent = 0;
    m->total_t = 0;
}

void record_start(Measurements *m) {
    get_time(&m->time_s);
}

void record_end(Measurements *m) {
    get_time(&m->time_e);
    long elapsed_t = ((m->time_e.tv_sec - m->time_s.tv_sec) * 1e9) + (m->time_e.tv_nsec - m->time_s.tv_nsec);

    if (elapsed_t < m->minimum) {
        m->minimum = elapsed_t;
    }
    if (elapsed_t > m->maximum) {
        m->maximum = elapsed_t;
    }

    // printf("%ld ", elapsed_t);
    m->total_t += elapsed_t;
    m->total_sent++;
}

void log_latency_results(Measurements *m) {
    double avg_latency = ((double) m->total_t / m->total_sent);

    printf("Results (Latency): \n");
    printf("Total messages sent: %d\n", m->total_sent);
    printf("Minimum RTT send time: %ld nano seconds\n", m->minimum);
    printf("Maximum RTT send time: %ld nano seconds\n", m->maximum);
    printf("Average RTT Latency: %.3f nano seconds\n", avg_latency);
}

void log_throughput_results(Measurements *m, int count, int size) {
    double min_tp = ((double) size) / (m->maximum / 1e6);
    double max_tp = ((double) size) / (m->minimum / 1e6);
    double avg_tp = ((double) count * size) / (m->total_t / 1e6);

    printf("Results (Throughput): \n");
    printf("Total messages sent: %d\n", m->total_sent);
    printf("Minimum Throughput: %.3f in bytes/millisecond\n", min_tp);
    printf("Maximum Throughput: %.3f in bytes/millisecond\n", max_tp);
    printf("Average Throughput: %.3f in bytes/millisecond\n", avg_tp);
}