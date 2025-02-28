/* 
   Assignment 4 Submission 
   Name: <Your_Name>
   Roll number: <Your_Roll_Number>
*/
#ifndef KSOCKET_H
#define KSOCKET_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

/* Custom socket type and parameters */
#define SOCK_KTP 100            /* Custom socket type for KTP */
#define T 5                     /* Timeout period (in seconds) */
#define DROP_PROBABILITY 0.1    /* Default drop probability */
#define MAX_KSOCKETS 10         /* Maximum number of KTP sockets */
#define MSG_SIZE 512            /* Fixed message size in bytes */

/* Error codes */
#define ENOSPACE 1
#define ENOTBOUND 2
#define ENOMESSAGE 3

/* KTP header structure (prepended to every 512-byte message) */
typedef struct {
    uint8_t seq;      /* Sequence number */
    uint8_t type;     /* 0 for DATA, 1 for ACK */
    uint8_t rwnd;     /* Receiver window size (for ACK messages) */
    uint8_t reserved; /* Reserved for future use */
} ktp_header_t;

/* KTP socket structure representing an entry in our “shared memory” table */
typedef struct {
    int in_use;                 /* 1 if entry is in use */
    int udp_fd;                 /* Underlying UDP socket file descriptor */
    struct sockaddr_in local_addr;   /* Local IP and port */
    struct sockaddr_in remote_addr;  /* Remote IP and port */
    int bound;                  /* 1 if k_bind has been called */
    int current_seq;            /* Next sequence number to send */
    int expected_seq;           /* Expected sequence number for incoming DATA */
    char send_buffer[MSG_SIZE]; /* Buffer holding last sent message (header+payload) */
    int send_len;               /* Length of message in send_buffer */
    time_t last_sent_time;      /* Timestamp of last transmission */
    int ack_received;           /* 1 if ACK has been received for the current message */
    char recv_buffer[MSG_SIZE]; /* Buffer holding received payload (without header) */
    int recv_len;               /* Length of message in recv_buffer */
} ktp_socket_t;

/* Global array for KTP sockets */
extern ktp_socket_t ktp_sockets[MAX_KSOCKETS];

/* Function prototypes for the KTP library */
int k_socket(int domain, int type, int protocol);
int k_bind(int sockfd, const struct sockaddr_in *local, const struct sockaddr_in *remote);
int k_sendto(int sockfd, const void *msg, size_t len, int flags,
             const struct sockaddr_in *dest_addr, socklen_t addrlen);
int k_recvfrom(int sockfd, void *buf, size_t len, int flags,
               struct sockaddr_in *src_addr, socklen_t *addrlen);
int k_close(int sockfd);

/* Function to simulate message drop with a given probability */
int dropMessage(float p);

#endif // KSOCKET_H
