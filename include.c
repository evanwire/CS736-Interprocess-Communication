#include <time.h>
#include <stdio.h>
#include <stdint.h>

#include "./include.h"

// TODO: Investigate other time measurement methods
unsigned long long get_now() {
    return ((double) clock()) / CLOCKS_PER_SEC * 1e9;
}

// Constructs a Measurements struct
void init_measurements(Measurements *m) {
    m->minimum = INT32_MAX;
    m->maximum = 0;
    m->start = get_now();
    m->total_sent = 0;
    m->total_t = 0;
}

// Records necessary measurements, called after a successful transmit
void record_start(Measurements *m) {
    m->start = get_now();
}

void record_end(Measurements *m) {
    unsigned long long t = get_now() - m->start;

    if (t < m->minimum) {
        m->minimum = t;
    }
    if (t > m->maximum) {
        m->maximum = t;
    }

    m->total_t += t;
    m->total_sent++;
}

// TODO: Check my unit conversions
// TODO: Standard deviation of message send times
// TODO: Do we want send rate? (size * bytes) / second?
void log_latency_results(Measurements *m, int count, int size) {
    double avg_latency = (m->total_t / 1000.0) / (m->total_sent);

    printf("Results (Latency): \n");
    printf("Total messages sent: %d\n", m->total_sent);
    printf("Minimum RTT send time: %.3f nano seconds\n", m->minimum / 1000.0);
    printf("Maximum RTT send time: %.3f nano seconds\n", m->maximum / 1000.0);
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