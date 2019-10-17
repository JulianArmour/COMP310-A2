#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

static sem_t rw_mutex;
static sem_t r_mutex;
static int global = 0;

int main() {
    printf("Hello, World!\n");
    return 0;
}

