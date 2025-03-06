#include "ksocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define SENDER_IP "127.0.0.1"
#define SENDER_PORT 8080
#define RECEIVER_IP "127.0.0.1"
#define RECEIVER_PORT 8081
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

    char buffer[100];

    // Read and send file contents in chunks
    while (1) {
        scanf("%s", buffer);
        if (k_sendto(sockfd, buffer, strlen(buffer)+1, 0, 
                      (struct sockaddr*)&dest_addr, &addrlen) < 0) {
            while(errno==ENOSPACE){
                printf("No space in buffer\n");
                sleep(5);
                if(k_sendto(sockfd, buffer, strlen(buffer)+1, 0, 
                      (struct sockaddr*)&dest_addr, &addrlen)>0)break;
                continue;
            }
            perror("Send failed");
            k_close(sockfd);
            exit(1);
        }
        printf("Sent%s \n", buffer);
        fflush(NULL);
    }


    k_close(sockfd);
    return 0;
}
