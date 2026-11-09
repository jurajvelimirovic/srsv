#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    long released;
    long started;
    long finished;
    long deadline_misses;
    long period_misses;
    long total_start_delay;
    long total_finish_delay;
    int period_counted;
} Statistics;

static void sleep_ms(long milliseconds)
{
    struct timespec duration = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L
    };
    nanosleep(&duration, NULL);
}

static int read_inputs(const char *path, InputDefinition *inputs)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return -1;

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (count == MAX_INPUTS ||
            sscanf(line, "%31s %d %ld %ld %ld %ld",
                   inputs[count].name, &inputs[count].priority,
                   &inputs[count].first, &inputs[count].period,
                   &inputs[count].computation, &inputs[count].deadline) != 6) {
            fclose(file);
            return -1;
        }
        count++;
    }
    fclose(file);
    return count;
}

static SharedState *create_shared_memory(void)
{
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0 || ftruncate(fd, sizeof(SharedState)) < 0)
        return NULL;

    SharedState *shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);
    close(fd);
    if (shared == MAP_FAILED)
        return NULL;

    memset(shared, 0, sizeof(*shared));
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_setpshared(&attributes, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared->mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
    return shared;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Invalid input");
        return 1;
    }

    InputDefinition inputs[MAX_INPUTS];
    int count = read_inputs(argv[1], inputs);
    long duration = strtol(argv[2], NULL, 10);
    if (count <= 0 || duration <= 0) {
        fprintf(stderr, "invalid input file or duration\n");
        return 1;
    }

    shm_unlink(SHM_NAME);
    SharedState *shared = create_shared_memory();
    if (!shared) {
        perror("input_sim: shared memory");
        return 1;
    }

    pthread_mutex_lock(&shared->mutex);
    shared->count = count;
    memcpy(shared->inputs, inputs, sizeof(inputs[0]) * count);
    pthread_mutex_unlock(&shared->mutex);

    Statistics statistics[MAX_INPUTS] = {0};
    long start = now_ms();
    shared->start_time = start;
    long next_release[MAX_INPUTS];
    for (int i = 0; i < count; i++)
        next_release[i] = inputs[i].first;

    while (now_ms() - start < duration) {
        long elapsed = now_ms() - start;
        for (int i = 0; i < count; i++) {
            pthread_mutex_lock(&shared->mutex);
            InputState *state = &shared->states[i];
            if (state->active && state->response_sequence == state->sequence) {
                long finish_delay = state->response_time - state->request_time;
                statistics[i].finished++;
                statistics[i].total_finish_delay += finish_delay;
                if (finish_delay > inputs[i].deadline)
                    statistics[i].deadline_misses++;
                state->active = 0;
                statistics[i].period_counted = 0;
                next_release[i] += inputs[i].period;
            }
            if (!state->active && elapsed >= next_release[i]) {
                state->active = 1;
                state->sequence++;
                state->request_time = now_ms();
                printf("%ld PROMJENA %s\n",
                       state->request_time - start, inputs[i].name);
                fflush(stdout);
                statistics[i].released++;
                statistics[i].total_start_delay += state->request_time - start - next_release[i];
                statistics[i].started++;
            }
            if (state->active && !statistics[i].period_counted &&
                now_ms() - state->request_time > inputs[i].period) {
                statistics[i].period_misses++;
                statistics[i].period_counted = 1;
            }
            pthread_mutex_unlock(&shared->mutex);
        }
        sleep_ms(1);
    }

    pthread_mutex_lock(&shared->mutex);
    shared->stop = 1;
    pthread_mutex_unlock(&shared->mutex);

    puts("input                    released started finished deadline period avg-start avg-finish");
    for (int i = 0; i < count; i++) {
        Statistics *s = &statistics[i];
        printf("%-24s %8ld %7ld %8ld %8ld %6ld %9.2f %10.2f\n",
               inputs[i].name, s->released, s->started, s->finished,
               s->deadline_misses, s->period_misses,
               s->started ? (double)s->total_start_delay / s->started : 0.0,
               s->finished ? (double)s->total_finish_delay / s->finished : 0.0);
    }

    munmap(shared, sizeof(*shared));
    shm_unlink(SHM_NAME);
    return 0;
}
