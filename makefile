CC = gcc
CFLAGS = -Wall -Wextra -pedantic -pthread
OBJS = threading.o

all: $(OBJS)

threading.o: threading.c threading.h
	$(CC) $(CFLAGS) -c threading.c -o threading.o

clean:
	rm -f $(OBJS)
