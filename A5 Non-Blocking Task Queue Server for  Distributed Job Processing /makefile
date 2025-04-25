all: client server

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c

run_client:
	./client

run_server:
	./server

clean:
	rm -f client server
