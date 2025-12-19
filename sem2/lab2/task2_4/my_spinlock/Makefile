EXEC = my_spinlock
CC = gcc
CFLAGS = -Wall -pthread

SRC = main.c my_spinlock.c ../../task2_1/queue.c
OBJ = $(SRC:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXEC)
	./$(EXEC)

clean:
	rm -f $(OBJ) $(EXEC)