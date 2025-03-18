all: client server

client: mysmtp_client.c
	gcc -o client mysmtp_client.c

server: mysmtp_server.c
	gcc -o server mysmtp_server.c

run_client:
	./client 127.0.0.0 2525

run_server:
	./server 2525

clean:
	rm -f client server