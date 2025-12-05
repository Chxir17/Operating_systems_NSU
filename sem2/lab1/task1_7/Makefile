EXECUTABLES=main
SOURCES=./
TYPE=.c

CC=gcc
RM=rm
CFLAGS=-Wall
LIBS=./u_thread.c
LDFLAGS=-o $@

TMPFILES=.stack.*

all: $(EXECUTABLES)

$(EXECUTABLES): %: $(SOURCES)%$(TYPE)
	$(CC) $(CFLAGS) $(LDFLAGS) $< ${LIBS}

run: $(EXECUTABLES)
	./${EXECUTABLES}

clean:
	${RM} -f *.o $(TMPFILES) $(EXECUTABLES)