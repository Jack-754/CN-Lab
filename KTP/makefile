all: init user1 user2 u1 u2

init: initksocket.c libksocket.a
	gcc -o init initksocket.c -L. -lksocket -pthread

user1: user1.c libksocket.a
	gcc -o user1 user1.c -L. -lksocket

user2: user2.c libksocket.a
	gcc -o user2 user2.c -L. -lksocket

u1: u1.c libksocket.a
	gcc -o u1 u1.c -L. -lksocket

u2: u2.c libksocket.a
	gcc -o u2 u2.c -L. -lksocket

libksocket.a: ksocket.o
	ar rcs libksocket.a ksocket.o

ksocket.o: ksocket.c ksocket.h
	gcc -Wall -c ksocket.c

rui:
	./init

ru1:
	./user1

ru2:
	./user2

clean:
	rm -f init libksocket.a *.o
	rm -f user1 user2 received.txt u1 u2