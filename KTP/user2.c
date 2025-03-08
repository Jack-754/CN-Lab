#include "ksocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RECEIVER_IP "127.0.0.1"
#define RECEIVER_PORT 8081
#define SENDER_IP "127.0.0.1"
#define SENDER_PORT 8080
#define CHUNK_SIZE 512

int main() {
    // Create socket
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Socket created successfully\n");

    // Bind socket
    if (k_bind(RECEIVER_IP, RECEIVER_PORT, SENDER_IP, SENDER_PORT) < 0) {
        perror("Bind failed");
        exit(1);
    }
    printf("Socket bound successfully\n");

    // Prepare source address structure
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    memset(&src_addr, 0, sizeof(src_addr));
    
    // Open output file
    FILE *fp = fopen("received.txt", "w");
    if (fp == NULL) {
        perror("Cannot open received.txt");
        k_close(sockfd);
        exit(1);
    }

    printf("Waiting for messages...\n");
    char buffer[CHUNK_SIZE];
    int bytes_received;

    int count=20, last=20;
    // Receive and write data until we get a zero-length message 
    while (1) {
        bytes_received = k_recvfrom(sockfd, buffer, 0, 
                                    (struct sockaddr*)&
                                    src_addr, &addrlen) ;
        if(bytes_received==0){
            continue;
        }
        if(bytes_received<0){
            if(errno==ENOSPACE){
                perror("No space in receiver window");
                sleep(3);
            }
            else{
                perror("Receive failed");
            }
            sleep(2);
            continue;
        }
        if (fwrite(buffer, 1, bytes_received, fp) != bytes_received) {
            perror("Write to file failed");
            fclose(fp);
            k_close(sockfd);
            exit(1);
        }
        printf("Received and wrote %d bytes\n", bytes_received);
    }

        
    printf("File received successfully\n");
    // Close socket
    fclose(fp);
    k_close(sockfd);
    return 0;
}
