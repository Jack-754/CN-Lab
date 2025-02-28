/* 
   Assignment 4 Submission 
   Name: <Your_Name>
   Roll number: <Your_Roll_Number>
*/
#include "ksocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#define SELECT_TIMEOUT 5

int main() {
    printf("initksocket: Process started to handle UDP messages and retransmissions.\n");
    
    while (1) {
        fd_set readfds;
        int max_fd = -1;
        FD_ZERO(&readfds);
        
        for (int i = 0; i < MAX_KSOCKETS; i++) {
            if (ktp_sockets[i].in_use && ktp_sockets[i].bound) {
                FD_SET(ktp_sockets[i].udp_fd, &readfds);
                if (ktp_sockets[i].udp_fd > max_fd)
                    max_fd = ktp_sockets[i].udp_fd;
            }
        }
        
        struct timeval tv;
        tv.tv_sec = SELECT_TIMEOUT;
        tv.tv_usec = 0;
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("select error");
            continue;
        }
        
        for (int i = 0; i < MAX_KSOCKETS; i++) {
            if (ktp_sockets[i].in_use && ktp_sockets[i].bound) {
                int fd = ktp_sockets[i].udp_fd;
                if (FD_ISSET(fd, &readfds)) {
                    char buffer[MSG_SIZE] = {0};
                    struct sockaddr_in src_addr;
                    socklen_t addrlen = sizeof(src_addr);
                    ssize_t bytes = recvfrom(fd, buffer, MSG_SIZE, 0,
                                             (struct sockaddr *)&src_addr, &addrlen);
                    if (bytes < 0)
                        continue;
                    
                    if (dropMessage(DROP_PROBABILITY)) {
                        printf("initksocket: Dropped a message on socket %d\n", i);
                        continue;
                    }
                    
                    ktp_header_t header;
                    memcpy(&header, buffer, sizeof(ktp_header_t));
                    
                    if (header.type == 1) {
                        ktp_sockets[i].ack_received = 1;
                        printf("initksocket: Received ACK for seq %d on socket %d\n", header.seq, i);
                    }
                }
            }
        }
    }
    return 0;
}
