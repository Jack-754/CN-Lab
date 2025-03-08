#include "ksocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define RECEIVER_IP "127.0.0.1"
#define RECEIVER_PORT 8083
#define SENDER_IP "127.0.0.1"
#define SENDER_PORT 8082
#define CHUNK_SIZE 512

void debug_mode(){
    printf("TURNING ON DEBUG MODE\n");
    fflush(NULL);
    char *name ="debug.txt";
    int fd = open(name, O_WRONLY|O_CREAT, 0777);
    if(fd==-1){
        perror("open failed.\n");
        exit(1);
    }
    if(dup2(fd, 1)==-1){
        perror("dup2 failed.\n");
        exit(1);
    }
    printf("DEBUG MODE ON\n");
}

int main() {
    //debug_mode();
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

    printf("Waiting for messages...\n");
    char buffer[100];
    sleep(10);
    printf("woke up\n");
    fflush(NULL);
    while (1) {
        printf("waiting for message\n");
        fflush(NULL);
        int len = k_recvfrom(sockfd, buffer, 0, 
                                    (struct sockaddr*)&
                                    src_addr, &addrlen) ;
        printf("message received\n");
        fflush(NULL);
        if(len==0){
            printf("0 sized message received.\n");
            continue;
        }
        if(len<0){
            if(errno==ENOMESSAGE){
                printf("No message received.\n");
                fflush(NULL);
                sleep(1);
            }
            else{
                perror("Receive failed");
            }
            continue;
        }
        printf("Received %d: %s\n", len, buffer);
    }

        
    printf("File received successfully\n");
    k_close(sockfd);
    return 0;
}
