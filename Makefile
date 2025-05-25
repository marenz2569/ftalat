CC=gcc
CFLAGS=-O3 -g -march=native -Wall -Wextra
LDFLAGS=

# add -fopenmp -DOMP for multi-thread-measurement (todo)
# add  -DNB_WAIT_RANDOM to wait a random time between 0 and NB_WAIT_US in us
MORE_FLAGS?=-DNB_WAIT_RANDOM -DNB_WAIT_US=10000 -DNB_REPORT_TIMES=10000 -DFREQ_SETTER_FILE=\"scaling_max_speed\"


.PHONY: all clean

all:
	$(CC) $(MORE_FLAGS) $(CFLAGS) $(LDFLAGS) main.c loop.c FreqGetter.c FreqSetter.c utils.c ConfInterval.c -o ftalat -lm -pthread

clean:
	rm -f ./ftalat
