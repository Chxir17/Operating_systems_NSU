EXECUTABLES=main
SOURCES=./
TYPE=.c

CC=gcc
RM=rm
CFLAGS=-Wall
FILES=handler/handlers.c handler/messages/messages.c list/list.c proxy/proxy.c
LIBS=-lpthread
LDFLAGS=-o $@

all: $(EXECUTABLES)

$(EXECUTABLES): %: $(SOURCES)%$(TYPE)
	$(CC) $(CFLAGS) $(LDFLAGS) $< ${LIBS} ${FILES}

run: $(EXECUTABLES)
	./${EXECUTABLES}

clean:
	${RM} -f *.o $(EXECUTABLES)