CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2

all: controller input_sim

controller: controller.c common.h
	$(CC) $(CFLAGS) controller.c -o controller -pthread -lrt

input_sim: input_sim.c common.h
	$(CC) $(CFLAGS) input_sim.c -o input_sim -pthread -lrt

clean:
	rm -f controller input_sim
