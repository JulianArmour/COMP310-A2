#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

typedef struct WaitTime {
    unsigned long min_wait_time;
    unsigned long max_wait_time;
    unsigned long sum_wait_time;
    unsigned long count_wait_time;
} WaitTime;

//solves the fairness problem by revoking access from readers when a writer shows up on the queue
static sem_t thread_queue;
//lock for the variable global
static sem_t global_mutex;
//lock for the r_count variable
static sem_t r_count_mutex;
//lock for writer_data and reader_data, when aggregating thread benchmark statistics
static sem_t rw_data_mutex;
//the resource for the readers-writers problem
static int global = 0;
//the number of reader threads currently reading global
static int r_count = 0;
// Only updated when global_mutex is locked in WriterThread()
static WaitTime writer_data = {ULLONG_MAX, 0, 0, 0};
// Only updated when global_mutex and r_count_mutex is are both locked in ReaderThread()
static WaitTime reader_data = {ULLONG_MAX, 0, 0, 0};

// Used to update either global writer_data or reader_data with thread-specific data
void UpdateGlobalReadWriteData(WaitTime *thread_data, WaitTime *global_data) {
  // Update min and max times
  if (thread_data->min_wait_time < global_data->min_wait_time)
    global_data->min_wait_time = thread_data->min_wait_time;
  if (thread_data->max_wait_time > global_data->max_wait_time)
    global_data->max_wait_time = thread_data->max_wait_time;
  // Update running sum of waiting times
  global_data->sum_wait_time += thread_data->sum_wait_time;
  global_data->count_wait_time += thread_data->count_wait_time;
}

// Used to update thread-specific benchmarking data on each iteration
static void UpdateWaitTimeData(struct timespec *start_time, struct timespec *end_time, WaitTime *data) {
  time_t sec_diff = end_time->tv_sec - start_time->tv_sec;
  long ns_diff = end_time->tv_nsec - start_time->tv_nsec;
  long wait_time = (sec_diff * 1000000000) + (ns_diff);
  // Update min and max times
  if (wait_time < data->min_wait_time) data->min_wait_time = wait_time;
  if (wait_time > data->max_wait_time) data->max_wait_time = wait_time;
  // Update running sum of wait times
  data->sum_wait_time += wait_time;
  data->count_wait_time++;
}

static void *WriterThread(void *repeat_count) {
  int loops = *((int *) repeat_count);
  struct timespec start_time;
  struct timespec end_time;
  //data for this thread's waiting time
  WaitTime writer_thread_data = {ULLONG_MAX, 0, 0, 0};

  for (int i = 0; i < loops; i++) {
    clock_gettime(CLOCK_REALTIME, &start_time);
    sem_wait(&thread_queue);//enter thread queue
    sem_wait(&global_mutex);//ENTERING global_mutex CRITICAL SECTION
    sem_post(&thread_queue);//exit thread queue
    global += 10;
    sem_post(&global_mutex);//EXITING global_mutex CRITICAL SECTION
    //update this thread's data with this loop's wait time
    clock_gettime(CLOCK_REALTIME, &end_time);
    UpdateWaitTimeData(&start_time, &end_time, &writer_thread_data);
    //sleep before trying to get access again
    usleep((random() % 101) * 1000);
  }
  //update the global writer_data
  sem_wait(&rw_data_mutex);
  UpdateGlobalReadWriteData(&writer_thread_data, &writer_data);
  sem_post(&rw_data_mutex);
}

static void *ReaderThread(void *repeat_count) {
  int loops = *((int *) repeat_count);
  struct timespec start_time;
  struct timespec end_time;
  //data for this thread's waiting time
  WaitTime reader_thread_data = {ULLONG_MAX, 0, 0, 0};

  for (int i = 0; i < loops; i++) {
    clock_gettime(CLOCK_REALTIME, &start_time);
    sem_wait(&thread_queue);//enter thread queue
    sem_wait(&r_count_mutex);//ENTERING r_count CRITICAL SECTION
    sem_post(&thread_queue);//exit thread queue
    r_count++;
    if (r_count == 1)
      sem_wait(&global_mutex);//writer done, lock-out writers
    sem_post(&r_count_mutex);//EXITING r_count CRITICAL SECTION
    global;//read performed
    clock_gettime(CLOCK_REALTIME, &end_time);
    sem_wait(&r_count_mutex);//ENTERING r_count CRITICAL SECTION
    r_count--;
    if (r_count == 0) {
      sem_post(&global_mutex);//readers done, open lock for next writer
    }
    sem_post(&r_count_mutex);//EXITING r_count CRITICAL SECTION
    //update this thread's data with this loop's wait time
    UpdateWaitTimeData(&start_time, &end_time, &reader_thread_data);
    //sleep before trying to get access again
    usleep((random() % 101) * 1000);
  }
  //update the global reader_data
  sem_wait(&rw_data_mutex);
  UpdateGlobalReadWriteData(&reader_thread_data, &reader_data);
  sem_post(&rw_data_mutex);
}

//the following resources was consulting when trying to find a solution to the starvation problem:
//https://rfc1149.net/blog/2011/01/07/the-third-readers-writers-problem/
int main(int argc, char *argv[]) {
  pthread_t readers[500];
  pthread_t writers[10];
  int read_loops;
  int write_loops;

  if (argc != 3) {
    puts("Invalid number of arguments");
    exit(EXIT_FAILURE);
  }
  write_loops = atoi(argv[1]);
  read_loops = atoi(argv[2]);

  //initialize semaphores
  if (sem_init(&global_mutex, 0, 1) == -1) {
    puts("Failed to initialize global_mutex");
    exit(EXIT_FAILURE);
  }
  if (sem_init(&r_count_mutex, 0, 1) == -1) {
    puts("Failed to initialize r_count_mutex");
    exit(EXIT_FAILURE);
  }
  if (sem_init(&thread_queue, 0, 1) == -1) {
    puts("Failed to initialize thread_queue");
    exit(EXIT_FAILURE);
  }
  if (sem_init(&rw_data_mutex, 0, 1) == -1) {
    puts("Failed to initialize rw_data_mutex");
    exit(EXIT_FAILURE);
  }

  //initialize readers
  for (int i = 0; i < 500; ++i) {
    if (pthread_create(&readers[i], NULL, ReaderThread, &read_loops) != 0) {
      puts("Error creating reader thread");
      exit(EXIT_FAILURE);
    }
  }
  //initialize writers
  for (int i = 0; i < 10; ++i) {
    if (pthread_create(&writers[i], NULL, WriterThread, &write_loops) != 0) {
      puts("Error creating writer thread");
      exit(EXIT_FAILURE);
    }
  }

  //join readers
    for (int i = 0; i < 500; ++i) {
      if (pthread_join(readers[i], NULL) != 0) {
        puts("Error joining readers");
        exit(EXIT_FAILURE);
      }
    }
  //join writers
  for (int i = 0; i < 10; ++i) {
    if (pthread_join(writers[i], NULL) != 0) {
      puts("Error joining writers");
      exit(EXIT_FAILURE);
    }
  }

  //destroy semaphores
  sem_destroy(&global_mutex);
  sem_destroy(&r_count_mutex);
  sem_destroy(&thread_queue);
  sem_destroy(&rw_data_mutex);

  //program output
  printf("global: %d\n", global);
  printf("Readers: Min: %ld ns, Mean: %ld ns, Max: %ld ns\n",
         reader_data.min_wait_time, reader_data.sum_wait_time / reader_data.count_wait_time, reader_data.max_wait_time);
  printf("Writers: Min: %ld ns, Mean: %ld ns, Max: %ld ns\n",
         writer_data.min_wait_time, writer_data.sum_wait_time / writer_data.count_wait_time, writer_data.max_wait_time);
  return 0;
}
