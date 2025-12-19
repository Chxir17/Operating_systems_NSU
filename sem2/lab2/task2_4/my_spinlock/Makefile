EXECUTABLES=my_spinlock
SOURCES=./

TYPE=.c
CC=gcc
RM=rm
CFLAGS=-Wall
LIBS=../../task2_1/queue.c my_spinlock/my_spinlock.c
LDFLAGS=-o $@

all: $(EXECUTABLES)

$(EXECUTABLES): %: $(SOURCES)%$(TYPE)
	$(CC) $(CFLAGS) $(LDFLAGS) $< ${LIBS}

run: $(EXECUTABLES)
	./${EXECUTABLES}

clean:
	${RM} -f *.o $(EXECUTABLES)