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
    time_t sec_diff = (*end_time).tv_sec - (*start_time).tv_sec;
    long ns_diff = (*end_time).tv_nsec - (*start_time).tv_nsec;
    long wait_time = (sec_diff * 1000000000) + (ns_diff);

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
        //sleep
        usleep((random() % 101) * 1000);
    }
}

static void *reader_thread(void *repeat_count) {
    int loops = *((int *)repeat_count);
    struct timespec start_time;
    struct timespec end_time;

    for (int i = 0; i < loops; i++) {
        clock_gettime(CLOCK_REALTIME, &start_time);
        if (sem_wait(&r_mutex) == -1) exit(EXIT_FAILURE);

        r_count++;
        if (r_count == 1)
            sem_wait(&rw_mutex);

        clock_gettime(CLOCK_REALTIME, &end_time);
        update_wait_data(&start_time, &end_time);

        sem_post(&r_mutex);

        global;//read performed

        sem_wait(&r_mutex);
        r_count--;

        if (r_count == 0) {
            sem_post(&rw_mutex);
        }

        sem_post(&r_mutex);
        //sleep
        usleep((random() % 101) * 1000);
    }
}

int main(int argc, char *argv[]) {
    pthread_t readers[500];
    pthread_t writers[10];
    int reads;
    int writes;

    if (argc != 3) {
        puts("Invalid number of arguments");
        exit(EXIT_FAILURE);
    }
    reads = atoi(argv[1]);
    writes = atoi(argv[2]);

    //initialize mutexes
    if(sem_init(&rw_mutex, 0, 1) == -1) {
        puts("Failed to initialize rw_mutex");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&r_mutex, 0, 1) == -1) {
        puts("Failed to initialize r_mutex");
        exit(EXIT_FAILURE);
    }

    //initialize writers
    for (int i = 0; i < 10; ++i) {
        if (pthread_create(&writers[i], NULL, writer_thread, &writes) != 0) {
            puts("Error creating writer thread");
            exit(EXIT_FAILURE);
        }
    }

    //initialize readers
    for (int i = 0; i < 500; ++i) {
        if (pthread_create(&readers[i], NULL, reader_thread, &reads) != 0) {
            puts("Error creating reader thread");
            exit(EXIT_FAILURE);
        }
    }

    //join writers
    for (int i = 0; i < 10; ++i) {
        if(pthread_join(writers[i], NULL) != 0) {
            puts("Error joining writers");
        }
    }
    //join readers
    for (int i = 0; i < 500; ++i) {
        if(pthread_join(readers[i], NULL) != 0) {
            puts("Error joining readers");
        }
    }

    printf("global: %d\tMin: %ld\tMean: %ld\tMax: %ld",
            global, min_wait_time, sum_wait_time/count_wait_time, max_wait_time);

    return 0;
}

