#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <time.h>

#define SHM_NAME "/srsv_lab1"
#define MAX_INPUTS 32
#define NAME_SIZE 32

typedef struct {
    char name[NAME_SIZE];
    int priority;
    long first;
    long period;
    long computation;
    long deadline;
} InputDefinition;

typedef struct {
    int active;
    unsigned long sequence;
    unsigned long response_sequence;
    long request_time;
    long response_time;
} InputState;

typedef struct {
    pthread_mutex_t mutex;
    int stop;
    int count;
    long start_time;
    InputDefinition inputs[MAX_INPUTS];
    InputState states[MAX_INPUTS];
} SharedState;

static inline long now_ms(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000L + time.tv_nsec / 1000000L;
}

#endif
