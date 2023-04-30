CC=gcc
CFLAGS=-g -std=gnu99 -pedantic -Wall -Wextra
TARGET=proj2

all: $(TARGET)

proj2: proj2.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET) proj2.out