#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/time.h>

#include <getopt.h>
#include <errno.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ONE_BILLION (1000 * 1000 * 1000)


// when the program starts
struct timespec start_time;
unsigned long start_rdtsc;

// when we end measuring the rdtsc for calibration
struct timespec stop_time;
unsigned long stop_rdtsc;



// should we fill the buffers with data or not
int is_fill = 1;

// should we dump some info other than timing info as we run
int is_verbose = 0;

/**
 * print an error message and then exit the process.
 * @param message the message to print
 */
void exit_with_error(const char* message) {
    perror(message);
    exit(-1);
}

/**
 * closes a file descriptor and checks for errors should one occur
 * @param fd the file descriptor to be closed
 */
void do_close(int fd) {
    int result = close(fd);
    if(result < 0) exit_with_error("close");
}

void usage() {
    printf("\nusage:\n");
    printf("     You must choose to run the program using one of the 4 modes below:\n");
    printf("           -p Pipes, use unix pipes to transfer data from sender to receiver.\n");
    printf("           -s Sockets, use TCP/IP (localhost) sockets to transfer from sender to receiver.\n");
    printf("           -m Shared Memory.  Use shared memory to transfer from sender to receiver.  When \n");
    printf("              testing throughput, you can specify the number of shared memory buffers(-c) to use.\n");
    printf("              Having more buffers allows the sender and receiver to overlap memory accesses,\n");
    printf("              but having two many buffers will cause L3 cache churn.\n");
    printf("           -o Overhead, This is NOT a test of data transfer. Instead a single thread fills\n");
    printf("              buffer_count (-c) buffers of size(-b) and then verifies the contents of the\n");
    printf("              buffers.  It does this for (-t) seconds and then reports the throughput for\n");
    printf("              the operation.  This gives an approximations of the cost for filling buffers.\n");
    printf("              Since both fill and verify happen on a single thread, this is an over estimate\n");
    printf("              of the throughput test case becuase those tests should use two cores.\n");
    printf("           \n");
    printf("     The following options modify the behavior of the tests:\n");
    printf("           -b (buffer_size) The buffer size used during the transfer.  This value must be a\n");
    printf("              multiple of 8.  The default value is a 4096.\n");
    printf("           -c (buffer_count) The shared memory throughput test and the Overhead test can use\n");
    printf("              multiple buffers.  Multiple buffers give the shared memory tests better performance\n");
    printf("              because the sender can be filling buffers while receiver is verifying the buffers.\n");
    printf("              During the memory overhead test, more buffers usually cause the test to run slower\n");
    printf("              because the processor caches get churned.\n");
    printf("           -t (run_time), This how long the program runs to gather timing info.  \n");
    printf("              When measuring latency, the run time is not used during sample collection,\n");
    printf("              but it is still used to see if enough time has elapsed when computing the\n");
    printf("              rdtsc ticks/second.  We this to convert out measurements from elapsed ticks\n");
    printf("              back to seconds.  Even 1 second of elapsed time gets a good estimate of ticks\n");
    printf("              per second (this value should be the same (or very close) to the CPU frequency\n");
    printf("              found if /proc/cpuinfo)  Default run_time is 10 seconds.\n");
    printf("           -l (sample_count), run a latency test instead of a throughput test.  This program\n");
    printf("              defaults to running a throughput test, by using this flag you can run a latency\n");
    printf("              test.  Specify the number of samples you would like to collect.  Note the we report\n");
    printf("              percentiles up tp 99.9999%% so you should probably collect at least 1 million samples.\n");
    printf("           -n No Fill/Verify of memory.  This program will write a monotonically increasing\n");
    printf("              sequence of longs into each buffer before sending it to the receiver.  The \n");
    printf("              receiver will verify the bytes received to ensure that it is receiving the correct\n");
    printf("              data. This operation takes a non-trivia amount of time.  With this flag you can turn\n");
    printf("              off this operation, and the memory transferred is whatever is in the buffers.\n");
    printf("           -v Verbose, Print out a lot more information when running.  When verbose is turned\n");
    printf("              off (the default) the program only prints the results on a single line.\n");
    printf("           \n");
    exit(-1);
}


/**
 * read the value from the timestamp counter.
 * @return an unsigned long read from the tsc.
 */
unsigned long get_rdtsc() {
    unsigned a, d;
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return ((unsigned long)a) | (((unsigned long)d) << 32);
}

/**
 * gets the realtime clock with nanosecond precision (if the system supports).
 * Getting the time using this call is slower than getting the TSC value.
 * So this is used only over longer durations.
 * @param ts a point to the timespec to be filled.
 */
void get_time(struct timespec* ts) {
    int result = clock_gettime(CLOCK_REALTIME, ts);
    if(result < 0) exit_with_error("clock_gettime");
}

/**
 * computes the difference (in nanos) between two timespec structs
 * @param start the start time
 * @param end  the end time
 * @return the nanos difference between end and start
 */
unsigned long elapsed_nanos(struct timespec* start, struct timespec* end) {
    long result = (end->tv_sec - start->tv_sec) * ONE_BILLION;
    result += end->tv_nsec - start->tv_nsec;
    return result;
}

/**
 * calculates the nano per tick of the timestamp clock.
 * Note to get an accurate measurement, at least run_time
 * number of seconds must have elapsed.  That amount of
 * time has not elapsed this function will sleep until
 * the required time has passed.
 * @param run_time the minimum time that must elapse to measure the time.
 * @return the number of nanos per tick (inverse of the cpu frequency)
 */
double get_nanos_per_tick(long run_time) {
    while (1) {
        get_time(&stop_time);
        stop_rdtsc = get_rdtsc();
        if (elapsed_nanos(&start_time, &stop_time) > run_time * ONE_BILLION) break;
        if (is_verbose) printf("sleeping for a bit -- waiting for rdtsc calibration\n");
        sleep(1);
    }

    if (stop_rdtsc < start_rdtsc) exit_with_error("tsc wrapped, how likely was that?");
    long elapsed_tsc = stop_rdtsc - start_rdtsc;
    double nanos_per_tick = ((double) elapsed_nanos(&start_time, &stop_time)) / ((double) elapsed_tsc);

    if(is_verbose) printf("nano per tick:%f -- cpu frequency: %f MHz\n", nanos_per_tick, 1000.0/nanos_per_tick);
    return nanos_per_tick;
}

int cmp(const void* v1, const void* v2) {
    long val1 = *((long*) v1);
    long val2 = *((long*) v2);

    if(val1 < val2) return -1;
    if(val1 > val2) return +1;
    return 0;
}


void print_tp(int sample_count, long* samples, double npt, double tp) {
    double index_d= (sample_count * tp) / 100;
    int index = (int) index_d;
    if(index >= sample_count) index = sample_count - 1;
    double value = samples[ index];
    if(is_verbose) {
        printf("tp%f index:%d sample_count:%d value:%f, nanos:%f\n", tp, index, sample_count, value, value*npt);
    } else {
        printf(", %f", value*npt);
    }
}

void print_average(int sample_count, long* samples, double npt) {
    double sum = 0;
    for(int i=0; i<sample_count; i++) sum += samples[i];
    double average = sum / sample_count * npt;
    if(is_verbose) {
        printf("sample_count:%d average: %f\n", sample_count, average);
    } else {
        printf("%f", average);
    }
}

void report_latency( int sample_count, long* samples, int run_time) {
    qsort(samples, sample_count, sizeof(long), &cmp);
    double npt = get_nanos_per_tick(run_time);
     if(is_verbose) {
        for(int i=0; i<sample_count; i++) {
            printf("sample: %d, %ld\n", i, samples[i]);
        }
    }
    print_average(sample_count, samples, npt);
    print_tp(sample_count, samples, npt, 0);
    print_tp(sample_count, samples, npt, 10);
    print_tp(sample_count, samples, npt, 50);
    print_tp(sample_count, samples, npt, 90);
    print_tp(sample_count, samples, npt, 99);
    print_tp(sample_count, samples, npt, 99.9);
    print_tp(sample_count, samples, npt, 99.99);
    print_tp(sample_count, samples, npt, 99.999);
    print_tp(sample_count, samples, npt, 100);
    printf("\n");

}


void report_throughput( long bytes_sent, long elapsed, int runtime) {
    double npt = get_nanos_per_tick(runtime);

    if(is_verbose) printf("get_nanos_per_tick: %f\n", npt);
    double elapsed_seconds = ((double)elapsed) * npt / ONE_BILLION;
    if(is_verbose)  printf("elapsed ticks: %ld seconds:%f\n", elapsed, elapsed_seconds);
    double throughput = ((double)bytes_sent) / elapsed_seconds;
    double gbps = throughput / 1024 /1024 /1024;
    if(is_verbose) {
        printf("bytes_sent: %ld, throughput(b/s):%f  %f(Gb/s)\n", bytes_sent, throughput, gbps);
    } else {
        printf("%f\n", throughput);
    }
}

/**
 * a static long used to fill the buffer with values that will be verified.
 */
static long nextFillValue = 0;

/**
 * fills a buffer with monotonically increasing integers.
 * note the size of the buffer must be divisible by sizeof(long)
 * @param buffer the buffer to fill
 * @param size  the size of the buffer
 */
void fill(void* buffer, int size) {
    int count = size / sizeof(long);
    assert(size == count * sizeof(long));
    long* array = (long*) buffer;
    for(int i=0; i<count; i++) {
        array[i] = nextFillValue++;
    }
}

/**
 * a static buffer for the next value we expect to see in the buffer
 */
static long nextExpectValue = 0;

/**
 * verfies that the buffer contains a sequence of monotonically increasing longs.
 * note the size of the buffer must be divisible by sizeof(long)
 * @param buffer the buffer to fill
 * @param size  the size of the buffer
 */
void verify(void* buffer, int size) {
    int count = size / sizeof(long);
    assert(size == count * sizeof(long));
    long *array = (long *) buffer;
    for (int i = 0; i < count; i++) {
        if(nextExpectValue != array[i]) {
            fprintf(stderr, "buffer did not contain the expected value:%ld, got:%ld\n", nextExpectValue, array[i]);
            exit(-1);
        }
        nextExpectValue++;
    }
}

void write_long(int fd, long value) {
    int result = write(fd, &value, sizeof(long));
    if(result<0) exit_with_error("write_long");
}

long read_long(int fd) {
    long value = 0;
    int result = read(fd, &value, sizeof(long));
    if(result != sizeof(long)) exit_with_error("read_long");
    return value;
}
void open_sockets(int* fds) {
    struct sockaddr_in address;

    // socket create and verification
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) exit_with_error("socket");

    bzero(&address, sizeof(address));

    // assign IP, PORT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(0);

    int result = bind(listen_socket, (struct sockaddr *) &address, sizeof(address));
    if (result != 0) exit_with_error("bind");

    bzero(&address, sizeof(address));
    int len = (socklen_t) sizeof(address);
    getsockname(listen_socket, (struct sockaddr *) &address, (socklen_t*) &len);

    int port = ntohs(address.sin_port);
    if(is_verbose) printf("port:%d\n", port);


    result = listen(listen_socket, 5);
    if (result != 0) exit_with_error("listen");

    // socket create and verification
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) exit_with_error("client socket");

    bzero(&address, sizeof(address));

    // assign IP, PORT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(port);

    // connect the client socket to server socket
    result = connect(client_socket, (struct sockaddr *) &address, sizeof(address));
    if (result != 0) exit_with_error("connect");

    // Accept the data packet from client and verification
    len = sizeof(address);
    int server_socket = accept(listen_socket, (struct sockaddr *) &address, (socklen_t *) &len);
    if (server_socket < 0) exit_with_error("accept");

    do_close(listen_socket);

    int one = 1;
    result = setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (result < 0) exit_with_error("setsockopt server socket");

    result = setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (result < 0) exit_with_error("setsockopt server socket");

    fds[0] = server_socket;
    fds[1] = client_socket;
}


struct shm_info_struct {
    int queue_id;
    int* buffer_ids;
    long* queue;
    long** buffers;
    int buffer_count;
    int buffer_size;
};
typedef struct shm_info_struct shm_info_t;

#define HEAD_OFFSET 0
#define TAIL_OFFSET 16
#define DONE_OFFSET 32
#define QUEUE_HEADER_SIZE 64

void init_shm(shm_info_t* info, int buffer_size, int buffer_count) {
    info->buffer_size = buffer_size;
    info->buffer_count = buffer_count;

    // create shm segment to hold the queue pointers head, tail & done
    info->queue_id = shmget(IPC_PRIVATE, sizeof(long) * QUEUE_HEADER_SIZE, IPC_CREAT | 0600);
    if (info->queue_id == -1) exit_with_error("init_shm: queue: shmget");

    // attach to the queue shm and set it all to zero
    long *queue = shmat(info->queue_id, NULL, SHM_RND);
    if (queue == NULL) exit_with_error("init_shm: shmat");
    memset(queue, 0, sizeof(long) * 64);
    queue[DONE_OFFSET] = -1;

    int result = shmdt(queue);
    if (result == -1) exit_with_error("init_shm: shmdt");
    info->queue = NULL;

    // create the array for the buffer id and pointers to the buffers
    info->buffer_ids = (int *) malloc(buffer_count * sizeof(int));
    info->buffers = NULL;

    // create the shared memory segments for the queue;
    for (int i = 0; i < buffer_count; i++) {
        info->buffer_ids[i] = shmget(IPC_PRIVATE, buffer_size, IPC_CREAT | 0600);
        if (info->buffer_ids[i] == -1) exit_with_error("shmget");
    }
}


void shm_attach(shm_info_t* info) {
    info->queue = shmat(info->queue_id, NULL, SHM_RND);
    if (info->queue == NULL) exit_with_error("shm_attach:shmat:queue");

    info->buffers = malloc(info->buffer_count * sizeof(long *));
    if (info->buffers == NULL) exit_with_error("malloc");

    for (int i = 0; i < info->buffer_count; i++) {
        info->buffers[i] = shmat(info->buffer_ids[i], NULL, SHM_RND);
        if (info->buffers[i] == NULL) exit_with_error("shm_attach:shmat:buffer");
    }
}

void shm_detach(shm_info_t* info) {
    int result = shmdt(info->queue);
    if (result == -1) exit_with_error("shm_detach:shmdt:queue");
    info->queue = NULL;

    for (int i = 0; i < info->buffer_count; i++) {
        result = shmdt(info->buffers[i]);
        if (result == -1) exit_with_error("shm_detach:shmdt:buffer");
        info->buffers[i] = NULL;
    }

    free(info->buffers);
    info->buffers = NULL;
}


/**
 * releases a bunch of shared memory segments
 * Note: after detaching the shared memory, the shm_ids array is freed
 * @param buffer_count the number of shared memory segments
 * @param shm_ids an array of the shared memory segment ids
 */
void shm_release(shm_info_t* info) {
    int result = shmctl(info->queue_id, IPC_RMID, NULL);
    if (result == -1) exit_with_error("shmctl");

    for (int i = 0; i < info->buffer_count; i++) {
        result = shmctl(info->buffer_ids[i], IPC_RMID, NULL);
        if (result == -1) exit_with_error("shmctl");
    }
    free(info->buffer_ids);
    info->buffer_ids = NULL;
}



long throughput_sender_fd(int fd, int buffer_size, int run_time) {
    if (is_verbose) printf("in throughput_sender_fd -- buffer_size:%d, run_time:%d\n", buffer_size, run_time);

    void *buffer = malloc(buffer_size);
    if (buffer == NULL) exit_with_error("malloc");

    struct timespec started;
    get_time(&started);
    struct timespec now;

    long end_nanos = run_time;
    end_nanos *= ONE_BILLION;

    long bytes_sent = 0;

    while (1) {
        get_time(&now);
        // it is not important that this time is accurate, we just
        // need a rough idea of how long to run (1 second resolution would be OK)
        if (elapsed_nanos(&started, &now) > end_nanos) {
            do_close(fd); // tell the receiver we are done sending
            break;
        }

        // instead of getting the time with each buffer sent,
        // we will check the time every 1000 buffers sent. This
        // lets us do about the minimum work in the loop.
        for (int i = 0; i < 1000; i++) {
            if (is_fill) fill(buffer, buffer_size);
            int result = write(fd, buffer, buffer_size);
            if (result != buffer_size) exit_with_error("throughput_sender_fd: write");
        }
        bytes_sent += ((long)1000) * buffer_size;
    }

    if(is_verbose) printf("leaving throughput_sender_fd bytes_sent:%ld\n", bytes_sent);

    // find out what time the receiver finished reading the last buffer
    // note this exchange will include the closing of the file descriptor
    // in the total time, should be minimal compared to the data transfer
    free(buffer);
    return bytes_sent;
}


long throughput_sender_shm(shm_info_t* info, int run_time) {

    if(is_verbose)printf("in throughput_sender_shm run_time:%d\n", run_time);

    volatile long* head_ptr = &(info->queue[HEAD_OFFSET]);
    volatile long* tail_ptr = &(info->queue[TAIL_OFFSET]);
    volatile long* done_ptr = &(info->queue[DONE_OFFSET]);
    int buffer_count = info->buffer_count;
    int buffer_size = info->buffer_size;

    long head = *head_ptr;

    struct timespec started;
    get_time(&started);
    struct timespec now;

    long end_nanos = run_time;
    end_nanos *= ONE_BILLION;
    while(1) {
        get_time(&now);
        if(elapsed_nanos(&started, &now) > end_nanos) break;

        long free;
        for (int i = 0; i < 1000; i++) {
            do {
                volatile long tail = *tail_ptr;
                free = buffer_count - (head - tail);
            } while (free == 0);

            for (int j = 0; j < free; j++) {
                if(is_fill) fill(info->buffers[head % buffer_count], buffer_size);
                head++;
                *head_ptr = head;
            }
        }
    }

    *done_ptr = head; // indicate to the receiver that this is all we are sending
    long bytes_sent = head * buffer_size;

    if(is_verbose)printf("leaving throughput_sender_shm bytes_sent:%ld\n", bytes_sent);
    return bytes_sent;
}



long throughput_receiver_fd(int fd, int buffer_size) {
    if(is_verbose) printf("in throughput_receiver_fd -- buffer_size:%d\n", buffer_size);

    long bytes_read = 0;
    char* buffer = (char*) malloc(buffer_size);
    if(buffer == NULL) exit_with_error("throughput_receiver_fd: malloc");

    int offset = 0;
    while(1) {
        int result = read(fd, &(buffer[offset]), buffer_size-offset);
        if(result == 0) break;
        if(result < 0) exit_with_error("read");
        offset += result;
        if(offset != buffer_size) continue;
        bytes_read += offset;
        if(is_fill) verify(buffer, offset);
        offset = 0;
    }

    if(is_verbose) printf("leaving throughput_receiver_fd bytes_read:%ld\n", bytes_read);

    // clean up after ourselves
    free(buffer);
    do_close(fd);
}



void throughput_receiver_shm(shm_info_t* info) {

    if(is_verbose)printf("in throughput_receiver_shm\n");

    volatile long* head_ptr = &(info->queue[HEAD_OFFSET]);
    volatile long* tail_ptr = &(info->queue[TAIL_OFFSET]);
    volatile long* done_ptr = &(info->queue[DONE_OFFSET]);
    int buffer_count = info->buffer_count;
    int buffer_size = info->buffer_size;

    volatile long tail = *tail_ptr;
    long available;

    while(1) {
        // spin until there are buffers available
        while(1) {
            volatile long head = *head_ptr;
            available = head - tail;

            // if there are no buffers available
            // then see if the sender is telling us we are done.
            if(available == 0) {
                volatile long final_count = *done_ptr;
                if(tail == final_count) return;
            }

            // process each buffer (verify the data if requested)
            // and then increment the tail pointer;
            for (int j = 0; j < available; j++) {
                int index = (int) (tail % buffer_count);
                if(is_fill) verify(info->buffers[index], buffer_size);
                tail++;
                *tail_ptr = tail;
            }
        }
    }
}



void latency_sender_fd(int fd, int buffer_size, int sample_count, long* samples, int result_fd) {
    if(is_verbose) printf("in latency_sender_fd -- buffer_size:%d, sample_count:%d\n", buffer_size, sample_count);

    void* buffer = malloc(buffer_size);
    if(buffer == NULL) exit_with_error("malloc");

    int index = 0;

    while(index < sample_count) {
        unsigned long started = get_rdtsc();

        if(is_fill) fill(buffer, buffer_size);
        int result = write(fd, buffer, buffer_size);
        if (result != buffer_size) exit_with_error("latency_sender_fd: write");

        // wait for the receiver to tell us how long it took them to get this buffer
        long received = read_long(result_fd);
        samples[index++] = received - started;
    }

    free(buffer);
    do_close(fd);
    do_close(result_fd);

    long bytes_sent = sample_count * buffer_size;
    if(is_verbose)printf("leaving latency_sender_fd bytes_sent:%ld\n", bytes_sent);

}

long latency_sender_shm(shm_info_t* info, int sample_count, long* samples, int result_fd) {

    if(is_verbose)printf("in latency_sender_shm sample_count:%d\n", sample_count);

    volatile long* head_ptr = &(info->queue[HEAD_OFFSET]);
    volatile long* tail_ptr = &(info->queue[TAIL_OFFSET]);
    volatile long* done_ptr = &(info->queue[DONE_OFFSET]);
    int buffer_size = info->buffer_size;

    long head = *head_ptr;
    int index = 0;

    while(index < sample_count) {

        long started = get_rdtsc();
        if (is_fill) fill(info->buffers[0], buffer_size);
        head++;
        *head_ptr = head;

        // wait for the receiver to move the tail pointer
        while (1) {
            volatile long tail = *tail_ptr;
            if (head == tail) break;
        }

        long end = get_rdtsc();
        long received = read_long(result_fd);
        samples[index++] = end - started;
    }

    *done_ptr = head; // indicate to the receiver that this is all we are sending

    long bytes_sent = head * buffer_size;
    if(is_verbose)printf("leaving latency_sender_shm bytes_sent:%ld\n", bytes_sent);

    return  bytes_sent;
}




void latency_receiver_fd(int fd, int buffer_size, int result_fd) {
    if(is_verbose) printf("in latency_receiver_fd-- buffer_size:%d\n", buffer_size);

    char* buffer = (char*) malloc(buffer_size);
    if(buffer == NULL) exit_with_error("malloc");

    long bytes_received = 0;
    int offset = 0;
    while(1) {
        int result = read(fd, &(buffer[offset]), buffer_size-offset);
        if(result == 0) break;
        if(result < 0) exit_with_error("read");
        offset += result;
        if(offset  != buffer_size) continue;
        if(is_fill) verify(buffer, offset);
        long end_tsc = get_rdtsc();
        write_long(result_fd, end_tsc);
        offset = 0;
        bytes_received += offset;
    }

    free(buffer);
    do_close(fd);
    if(is_verbose) printf("leaving latency_receiver_fd-- bytes_received:%ld\n", bytes_received);
}


void latency_receiver_shm(shm_info_t* info, int result_fd) {

    if(is_verbose)printf("in latency_receiver_shm\n");

    volatile long* head_ptr = &(info->queue[HEAD_OFFSET]);
    volatile long* tail_ptr = &(info->queue[TAIL_OFFSET]);
    volatile long* done_ptr = &(info->queue[DONE_OFFSET]);

    int buffer_size = info->buffer_size;

    long tail = *tail_ptr;
    long available;

    while (1) {
        // spin until there are buffers available
        while (1) {
            volatile long head = *head_ptr;
            available = head - tail;
            if (available == 0) {
                volatile long final_count = *done_ptr;
                if (tail == final_count) {
                    long bytes_received = tail * buffer_size;
                    if(is_verbose)printf("leaving latency_receiver_shm -- bytes_received:%ld\n", bytes_received);
                    return;
                }
                continue;
            }

            if (is_fill) verify(info->buffers[0], buffer_size);
            tail++;
            *tail_ptr = tail;
            write_long(result_fd, get_rdtsc());
        }
    }
}

void measure_fd_latency(int* fds, int buffer_size, int sample_count, int run_time, int* result_fds) {

    if(is_verbose) printf("in measure_fd_latency buffer_size:%d, sample_count:%d run_time;%d\n", buffer_size,  sample_count, run_time);

    int pid = fork();
    if(pid<0) exit_with_error("fork");
    if(pid == 0 ) {
        do_close(fds[1]);
        do_close(result_fds[0]);
        latency_receiver_fd(fds[0], buffer_size, result_fds[1]);
     } else {
        do_close(fds[0]);
        do_close(result_fds[1]);
        long* samples = malloc(sample_count * sizeof(long));

        latency_sender_fd(fds[1], buffer_size, sample_count, samples, result_fds[0]);
        report_latency(sample_count, samples, run_time);
        free(samples);
    }
}


void measure_shm_latency(int buffer_size, int buffer_count, int run_time, int sample_count, int* result_fds) {

    if(is_verbose) printf("in measure_shm_latency buffer_size:%d, buffer_count:%d, run_time:%d, sample_count:%d\n",
                          buffer_size, buffer_count, run_time, sample_count);

    shm_info_t info;
    init_shm(&info, buffer_size, buffer_count);

    int pid = fork();
    if(pid<0) exit_with_error("fork");
    if(pid == 0 ) {
        shm_attach(&info);
        latency_receiver_shm(&info, result_fds[1]);
        shm_detach(&info);
     } else {
        shm_attach(&info);
        long* samples = malloc(sample_count * sizeof(long));
        latency_sender_shm(&info, sample_count, samples, result_fds[0]);
        shm_detach(&info);
        shm_release(&info);

        report_latency(sample_count, samples, run_time);
        free(samples);
    }
}



long measure_fd_throughput(int fds[], int buffer_size, int run_time, int* result_fds) {

    if(is_verbose) printf("in measure_fd_throughput buffer_size:%d, run_time:%d\n", buffer_size, run_time);

    int result = fork();
    if(result<0) exit_with_error("fork");
    if(result == 0 ) {
        do_close(fds[1]);
        do_close(result_fds[0]);
        throughput_receiver_fd(fds[0], buffer_size);
        write_long(result_fds[1], get_rdtsc());
    } else {
        do_close(fds[0]);
        do_close(result_fds[1]);
        long start = get_rdtsc();
        long bytes_sent = throughput_sender_fd(fds[1], buffer_size, run_time);
        long end = read_long(result_fds[0]);

        report_throughput(bytes_sent, (end - start), run_time);
    }
}


void measure_shm_throughput(int buffer_size, int buffer_count, int run_time, int* result_fds) {

    if(is_verbose) printf("in measure_shm_throughput buffer_size:%d, buffer_count:%d, run_time:%d\n", buffer_size, buffer_count, run_time);

    // set up the shared memory buffers
    shm_info_t info;
    init_shm(&info, buffer_size, buffer_count);

    int pid = fork();
    if(pid<0) exit_with_error("fork");
    if(pid == 0 ) {
        shm_attach(&info);
        throughput_receiver_shm(&info);
        write_long(result_fds[1], get_rdtsc());
        shm_detach(&info);
    } else {
        shm_attach(&info);

        long start = get_rdtsc();
        long bytes_sent = throughput_sender_shm(&info, run_time);
        long end = read_long(result_fds[0]);

        shm_detach(&info);
        shm_release(&info); // we release shm in the parent

        report_throughput(bytes_sent, (end-start), run_time);
    }
}

void measure_memory_overhead(int buffer_size, int block_count, int run_time) {
    if (is_verbose) printf("in throughput_sender_fd -- buffer_size:%d, run_time:%d\n", buffer_size, run_time);

    int loops_without_checking_time = 100;

    void **buffers = malloc(block_count * sizeof(void *));
    if (buffers == NULL) exit_with_error("malloc");

    for (int i = 0; i < block_count; i++) {
        buffers[i] = malloc(buffer_size);
        if (buffers[i] == NULL) exit_with_error("malloc");
    }

    struct timespec started;
    get_time(&started);
    struct timespec now;

    long end_nanos = run_time;
    end_nanos *= ONE_BILLION;

    long bytes_touched = 0;
    long start  = get_rdtsc();

    while (1) {
        get_time(&now);
        if (elapsed_nanos(&started, &now) > end_nanos) break;

        for (int j = 0; j < loops_without_checking_time; j++) {
            for (int i = 0; i < block_count; i++) {
                fill(buffers[i], buffer_size);
            }

            for (int i = 0; i < block_count; i++) {
                verify(buffers[i], buffer_size);
            }

            bytes_touched += ((long)buffer_size) * block_count;
        }

    }
    long end = get_rdtsc();
    report_throughput(bytes_touched, (end - start), run_time);
}

int main(int argc, char** argv) {

    get_time(&start_time);
    start_rdtsc = get_rdtsc();

    int is_pipe = 0;
    int is_shm = 0;
    int is_socket = 0;
    int is_overhead = 0;

    // how much data do we transfer at a time?
    int buffer_size = 4096;

    // the number of buffers used when doing shm.
    int buffer_count = 1;

    // when measuring throughput, how many seconds should we run for?
    int run_time = 10;

    // when measuring latency, how many samples should we collect
    int sample_count = -1;

    int c;
    while ((c = getopt(argc, argv, "psmonvb:c:t:l:")) != -1) {
        switch (c) {
            case 'p':
                is_pipe = 1;
                break;
            case 's':
                is_socket = 1;
                break;
            case 'm':
                is_shm = 1;
                break;
            case 'o':
                is_overhead = 1;
                break;
            case 'n':
                is_fill = 0;
                break;
            case 'v':
                is_verbose = 1;
                break;
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 'c':
                buffer_count = atoi(optarg);
                break;
            case 't':
                run_time = atoi(optarg);
                break;
            case
            'l':
                sample_count = atoi(optarg);
                break;
            default:
                usage();
        }
    }

    int count = is_pipe + is_socket + is_shm+ is_overhead;
    if (count != 1) {
        fprintf(stderr, "you must choose one of -p(pipe) -s(socket) -m(shared memory) -o(overhead)\n");
        usage();
    }

    if (run_time < 1) {
        fprintf(stderr, "you must specify a run time(-t) in seconds\n");
        usage();
    }

    if(buffer_size % 8 != 0) {
        fprintf(stderr, "The buffer_size(%d) must be divisible by 8.\n", buffer_size);
        usage();
    }

    int result_fds[2];
    int result = pipe(result_fds);
    if (result < 0) exit_with_error("pipe");

    if(is_overhead) {
        measure_memory_overhead(buffer_size, buffer_count, run_time);
    }
    else if (is_shm) {
        if (sample_count < 1) {
            measure_shm_throughput(buffer_size, buffer_count, run_time, result_fds);
        } else {
            measure_shm_latency(buffer_size, buffer_count, run_time, sample_count, result_fds);
        }
    } else {
        int fds[2];
        if(is_pipe) {
            // create the pipes that will be used to transfer the blocks
            result = pipe(fds);
            if (result < 0) exit_with_error("pipe");
        } else {
            // open_sockets will deal with any errors
            open_sockets(fds);
        }

        if (sample_count < 1) {
            measure_fd_throughput(fds, buffer_size, run_time, result_fds);
        } else {
            measure_fd_latency(fds, buffer_size, sample_count, run_time, result_fds);
        }
    }
}
