#include "ksocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SENDER_IP "127.0.0.1"
#define SENDER_PORT 8080
#define RECEIVER_IP "127.0.0.1"
#define RECEIVER_PORT 8081
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
    if (k_bind(SENDER_IP, SENDER_PORT, RECEIVER_IP, RECEIVER_PORT) < 0) {
        perror("Bind failed");
        exit(1);
    }
    printf("Socket bound successfully\n");

    // Prepare destination address
    struct sockaddr_in dest_addr;
    socklen_t addrlen = sizeof(dest_addr);
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(RECEIVER_IP);
    dest_addr.sin_port = htons(RECEIVER_PORT);

    // Open the input file
    FILE *fp = fopen("sample.txt", "r");
    if (fp == NULL) {
        perror("Cannot open sample.txt");
        k_close(sockfd);
        exit(1);
    }

    char buffer[CHUNK_SIZE];
    size_t bytes_read;

    // Read and send file contents in chunks
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        while (k_sendto(sockfd, buffer, bytes_read, 0, 
                      (struct sockaddr*)&dest_addr, &addrlen) < 0) {
            if(errno==ENOSPACE){
                printf("No space in buffer\n");
                sleep(3);
                continue;
            }
            perror("Send failed");
            fclose(fp);
            k_close(sockfd);
            exit(1);
        }
        printf("Sent %zu bytes\n", bytes_read);
    }

    // Send a zero-length message to indicate end of transmission
    if (k_sendto(sockfd, "", 0, 0, 
                  (struct sockaddr*)&dest_addr, &addrlen) < 0) {
        perror("Send failed");
        fclose(fp);
        k_close(sockfd);
        exit(1);
    }

    fclose(fp);
    k_close(sockfd);
    return 0;
}
