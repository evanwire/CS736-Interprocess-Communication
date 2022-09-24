#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "./include.h"

// TODO: Investigate other time measurement methods
unsigned long long get_now(){
    return ((double)clock()) / CLOCKS_PER_SEC * 1e9;
}

// Constructs a Measurements struct
void init_measurements(Measurements* m){
    m->minimum = INT32_MAX;
    m->maximum = 0;
    m->start = get_now();
}

// Records necessary measurements, called after a successful transmit
void record(Measurements* m){
    unsigned long long t = get_now() - m->start;

    if(t < m->minimum){
        m->minimum = t;
    }
    if(t > m->maximum){
        m->maximum = t;
    }

    m->total += t;
}

// TODO: Check my unit conversions
// TODO: Standard deviation of message send times
// TODO: Do we want send rate? (size * bytes) / second?
void log_results(Measurements* m, int count, int size){
    double tp = (count * size) / (m->total / 1e6); // Milliseconds
    double avg_latency = (m->minimum + m->maximum) / 4; //divide by 2 for average, and 2 for latency which is 1/2 rtt

    printf("Results: \n");
    printf("Average Latency: %.3f nano seconds\n", avg_latency / 1000.0);
    printf("Minimum send time: %.3f nano seconds\n", m->minimum / 1000.0);
    printf("Maximum send time: %.3f nano seconds\n", m->maximum / 1000.0);
    printf("Throughput: %.3f\n in bytes/millisecond", tp);

}