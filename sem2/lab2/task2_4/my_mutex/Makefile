EXECUTABLES=my_mutex
SOURCES=./

TYPE=.c
CC=gcc
RM=rm
CFLAGS=-Wall
LIBS=../../task2_1/queue.c my_mutex/my_mutex.c
LDFLAGS=-o $@

all: $(EXECUTABLES)

$(EXECUTABLES): %: $(SOURCES)%$(TYPE)
	$(CC) $(CFLAGS) $(LDFLAGS) $< ${LIBS}

run: $(EXECUTABLES)
	./${EXECUTABLES}

clean:
	${RM} -f *.o $(EXECUTABLES)