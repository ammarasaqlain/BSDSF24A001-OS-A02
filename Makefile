CC = gcc
CFLAGS = -Wall -Wextra -g

all: bin/ls

bin/ls: obj/lsv1.0.0.o
	$(CC) $(CFLAGS) -o bin/ls obj/lsv1.0.0.o

obj/lsv1.0.0.o: src/lsv1.0.0.c
	$(CC) $(CFLAGS) -c src/lsv1.0.0.c -o obj/lsv1.0.0.o

clean:
	rm -f obj/*.o bin/ls
