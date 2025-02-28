/* 
   Assignment 4 Submission 
   Name: <Your_Name>
   Roll number: <Your_Roll_Number>
*/
#include "ksocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

ktp_socket_t ktp_sockets[MAX_KSOCKETS] = {0};

/* Utility: Set a socket to nonblocking mode */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* k_socket: Creates a new KTP socket. */
int k_socket(int domain, int type, int protocol) {
    if (type != SOCK_KTP) {
        errno = EINVAL;
        return -1;
    }
    for (int i = 0; i < MAX_KSOCKETS; i++) {
        if (!ktp_sockets[i].in_use) {
            ktp_sockets[i].in_use = 1;
            ktp_sockets[i].bound = 0;
            ktp_sockets[i].current_seq = 1;
            ktp_sockets[i].expected_seq = 1;
            ktp_sockets[i].send_len = 0;
            ktp_sockets[i].recv_len = 0;
            ktp_sockets[i].ack_received = 1;
            int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (udp_fd < 0) {
                ktp_sockets[i].in_use = 0;
                return -1;
            }
            if (set_nonblocking(udp_fd) < 0) {
                close(udp_fd);
                ktp_sockets[i].in_use = 0;
                return -1;
            }
            ktp_sockets[i].udp_fd = udp_fd;
            return i;
        }
    }
    errno = ENOSPACE;
    return -1;
}

/* k_bind: Binds the KTP socket to a local address and sets the remote address. */
int k_bind(int sockfd, const struct sockaddr_in *local, const struct sockaddr_in *remote) {
    if (sockfd < 0 || sockfd >= MAX_KSOCKETS) {
        errno = EBADF;
        return -1;
    }
    ktp_socket_t *ks = &ktp_sockets[sockfd];
    if (!ks->in_use) {
        errno = EBADF;
        return -1;
    }
    if (bind(ks->udp_fd, (struct sockaddr *)local, sizeof(struct sockaddr_in)) < 0)
        return -1;
    ks->local_addr = *local;
    ks->remote_addr = *remote;
    ks->bound = 1;
    return 0;
}
