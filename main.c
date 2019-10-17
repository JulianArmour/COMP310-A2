#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

static sem_t rw_mutex;
static sem_t r_mutex;
static int r_count = 0;
//only updated when rw_mutex is locked
static int global = 0;
//only updated when rw_mutex is locked in writer_thread()
//or when rw_mutex and r_mutex is are both locked in reader_thread()
static unsigned long min_wait_time = ULLONG_MAX;
static unsigned long max_wait_time = 0;
static unsigned long sum_wait_time = 0;
static unsigned long count_wait_time = 0;

static void update_wait_data(struct timespec *start_time, struct timespec *end_time) {
    time_t sec_diff = ((*end_time).tv_sec - (*start_time).tv_sec) * 1000;
    long ms_diff = ((*end_time).tv_nsec - (*start_time).tv_nsec) / 1000000;
    long wait_time = sec_diff + ms_diff;

    //update min and max times
    if (wait_time < min_wait_time) min_wait_time = wait_time;
    if (wait_time > max_wait_time) max_wait_time = wait_time;
    sum_wait_time += wait_time;
    count_wait_time++;
}

static void *writer_thread(void *repeat_count) {
    int loops = *((int *)repeat_count);
    struct timespec start_time;
    struct timespec end_time;

    for (int i = 0; i < loops; i++) {

        clock_gettime(CLOCK_REALTIME, &start_time);
        if (sem_wait(&rw_mutex) == -1) exit(EXIT_FAILURE);
        clock_gettime(CLOCK_REALTIME, &end_time);

        update_wait_data(&start_time, &end_time);
        global += 10;

        if (sem_post(&rw_mutex) == -1) exit(EXIT_FAILURE);

        //wait before trying to gain access again
        usleep((random() % 101) * 1000);
    }
}

static void *reader_thread(void *repeat_count) {
    int loops = *((int *)repeat_count);
    struct timespec start_time;
    struct timespec end_time;

    if (sem_wait(&r_mutex) == -1) exit(EXIT_FAILURE);

    r_count++;
    if (r_count)
}

int main() {
    printf("long long : %lu \t long: %lu\n", sizeof(unsigned long long), sizeof(long));
    return 0;
}

