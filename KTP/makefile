# Unified Makefile
.PHONY: all clean run

all: libksocket init user1 user2 u1testing u2testing

# Build libksocket
libksocket: ksocket.o
	ar rcs libksocket.a ksocket.o

ksocket.o: ksocket.c ksocket.h
	gcc -Wall -c ksocket.c

# Build init
init: initksocket.c libksocket.a
	gcc -o init initksocket.c -L. -lksocket -pthread

# Build user1
user1: user1.c libksocket.a
	gcc -o user1 user1.c -L. -lksocket

# Build user2
user2: user2.c libksocket.a
	gcc -o user2 user2.c -L. -lksocket

# Build u1testing
u1testing: u1testing.c libksocket.a
	gcc -o u1testing u1testing.c -L. -lksocket

# Build u2testing
u2testing: u2testing.c libksocket.a
	gcc -o u2testing u2testing.c -L. -lksocket

# Run executables
run:
	./init &
	./user1 &
	./user2 &

runtesting:
	./init &
	./u1testing &
	./u2testing &

# Clean all generated files
clean:
	rm -f init libksocket.a ksocket.o user1 user2 u1testing u2testing
