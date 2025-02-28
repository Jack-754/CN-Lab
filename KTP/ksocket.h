#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>

#define p 0.05
#define T 5
#define N 25
#define SOCK_KTP 3
#define MAX_RETRIES 5
#define WINDOW_SIZE 10
#define MESSAGE_SIZE 512
#define ENOSPACE -1
#define ENOTBOUND -2
#define ENOMESSAGE -3

typedef enum state{
    FREE,ALLOTED,TO_CREATE,CREATED,TO_BIND,BOUND,TO_CLOSE
}STATE;

typedef enum window_state{
    FREE, SENT, ACKED, NOT_SENT
}window_state;    

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

typedef struct window {
    window_state wndw[WINDOW_SIZE]; // state of each sequence number in window
    int size;                       // size of window
    int pointer;                    // points to the first unacknowledged sequence number in window
    int seq;              // sequence number for send window and expected sequence number for receiver window
}window;


typedef struct SM{
    STATE state;
    int pid;
    int sockfd;             // socket file descriptor of the udp socket
    char dest_ip[16];
    int dest_port;
    char src_ip[16];
    int src_port;
    char send_buffer[WINDOW_SIZE][MESSAGE_SIZE];
    int send_buffer_msg_size[WINDOW_SIZE];
    int send_msg_count;     // number of messages in buffer currently
    int send_ptr;           // pointer to first message in buffer
    char recv_buffer[WINDOW_SIZE][MESSAGE_SIZE];
    int recv_buffer_msg_size[WINDOW_SIZE];
    int recv_msg_count;     // number of messages in buffer currently
    int recv_ptr;           // pointer to first message in buffer

    window swnd;
    window rwnd;

    time_t time_sent[WINDOW_SIZE];
}SM;

typedef struct packet{
    int seq_no;
    int ack_no;
    char data[MESSAGE_SIZE];
    int len;
    int flag;   //bits 0: fin, 1: syn, 2: ack
    // int fin;
    // int syn;
    // int ack;
    int window;
}packet;

extern SM* SM_table;
extern int sem_SM, // for controlling access to shared memory table
shmid_SM,          // related to shared memory table
sem1,              // for signalling service request from initksocket main while loop
sem2;              // for signalling service request completion from initksocket main while loop
extern struct sembuf pop, vop;


int free_slot();

int dropMessage();

int k_socket(int domain, int type, int protocol);

int k_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port);

ssize_t k_sendto(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

ssize_t k_recvfrom(int sockfd, void *buf, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

int k_close(int sockfd);