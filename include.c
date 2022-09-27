#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
//#include <sys/time.h>
#include <time.h>
//#define _POSIX_C_SOURCE 199309L

#include "./include.h"

// TODO: Investigate other time measurement methods
void get_now(struct timespec* ts) {
    int result = clock_gettime(CLOCK_REALTIME, ts);
    if(result < 0) {
        perror("clock_gettime");
        exit(0);
    }
}

// Constructs a Measurements struct
void init_measurements(Measurements* m){
    m->minimum = INT32_MAX;
    m->maximum = 0;
    m->total_sent = 0;
    m->total_t = 0;
    get_now(&m->start_t);
    get_now(&m->exp_start);
}

void record_end(Measurements *m){
    get_now(&m->exp_end);
}

void record(Measurements *m) {
    get_now(&m->end_t);
    long t = (m->end_t.tv_sec - m->start_t.tv_sec) * 1e9;
    t +=  (m->end_t.tv_nsec - m->start_t.tv_nsec);

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
void log_l(Measurements* m, int count, int size){
    double avg_latency = ((double) m->total_t / (1000 * m->total_sent)) / 2.0; // divide by 2 for latency, don't need the return transmission

    printf("Results (Latency): \n");
    printf("Total messages sent: %d\n", m->total_sent);
    printf("Minimum RTT send time: %.3f micro seconds\n", (double) m->minimum / 1000);
    printf("Maximum RTT send time: %.3f micro seconds\n", (double) m->maximum / 1000);
    printf("Average RTT Latency: %.3f micro seconds\n", avg_latency);
}

void log_tp(Measurements* m, int count, int size){
    double min_tp = ((double) size) / (m->maximum / 1e6);
    double max_tp = ((double) size) / (m->minimum / 1e6);
    double avg_tp = ((double) count * size) / (m->total_t / 1e6);

    printf("Results (Throughput): \n");
    printf("Total messages sent: %d\n", m->total_sent);
    printf("Minimum Throughput: %.3f in bytes/millisecond\n", min_tp);
    printf("Maximum Throughput: %.3f in bytes/millisecond\n", max_tp);
    printf("Average Throughput: %.3f in bytes/millisecond\n", avg_tp);
}