all: client server

client: cldp_client.c
	gcc -o client cldp_client.c

server: cldp_server.c
	gcc -o server cldp_server.c

run_client:
	./client

run_server:
	./server

clean:
	rm -f client server
