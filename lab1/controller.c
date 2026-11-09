#include "common.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void sleep_ms(long milliseconds)
{
    struct timespec duration = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L
    };
    nanosleep(&duration, NULL);
}

static SharedState *open_shared_memory(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (fd < 0)
        return NULL;

    SharedState *shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);
    close(fd);
    return shared == MAP_FAILED ? NULL : shared;
}

int main(void)
{
    SharedState *shared = open_shared_memory();
    if (!shared) {
        perror("controller: shared memory");
        return 1;
    }

    while (1) {
        int stop;
        pthread_mutex_lock(&shared->mutex);
        stop = shared->stop;
        pthread_mutex_unlock(&shared->mutex);
        if (stop)
            break;

        for (int i = 0; i < shared->count; i++) {
            long computation;
            unsigned long sequence;

            pthread_mutex_lock(&shared->mutex);
            if (!shared->states[i].active ||
                shared->states[i].response_sequence == shared->states[i].sequence) {
                pthread_mutex_unlock(&shared->mutex);
                continue;
            }
            computation = shared->inputs[i].computation;
            sequence = shared->states[i].sequence;
            printf("%ld %s počinje obrada\n",
                   now_ms() - shared->start_time, shared->inputs[i].name);
            fflush(stdout);
            pthread_mutex_unlock(&shared->mutex);

            sleep_ms(computation);

            pthread_mutex_lock(&shared->mutex);
            if (shared->states[i].sequence == sequence) {
                long response_time = now_ms();
                shared->states[i].response_time = response_time;
                shared->states[i].response_sequence = sequence;
                printf("%ld %s gotov\n",
                       response_time - shared->start_time,
                       shared->inputs[i].name);
                fflush(stdout);
            }
            pthread_mutex_unlock(&shared->mutex);
        }
        sleep_ms(1);
    }

    munmap(shared, sizeof(*shared));
    return 0;
}
