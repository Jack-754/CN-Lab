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
#define WINDOW_SIZE 10
#define MESSAGE_SIZE 512
#define MAX_TRIES 20
#define ENOSPACE -1
#define ENOTBOUND -2
#define ENOMESSAGE -3

typedef enum state{
    FREE,ALLOTED,TO_CREATE,CREATED,TO_BIND,BOUND,TO_CLOSE
}STATE;

typedef enum window_state{
    WFREE, SENT, ACKED, NOT_SENT, RECVD
}window_state;    

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

typedef struct window {
    window_state wndw[WINDOW_SIZE]; // state of the message corresponding to the currently mapped sequence number, FREE if no message is stored, SENT if message is sent but not acknowledged, ACKED if message is acknowledged, NOT_SENT if message is not sent, RECVD if message is received

    int size;                       // for send window (swnd), size of reciever window size available based on advertised window size (can be used to check if a message can be sent to receiver based on advertised window size), for reciever window (rwnd), number of free slots in window where no message is stored in corresponding message buffer

    int pointer;                    // points to the first unacknowledged sequence number in window in send window (swnd), points to the expected sequence number in reciever window (rwnd)

    int seq;              // sequence number for first unacknowledged message in send window and expected sequence number for first unreceived message in reciever window | all sequence numbers are in the range 1 to 256
}window;


typedef struct SM{
    STATE state;             // state of the socket can be FREE (no process is using it), ALLOTED (process is has been allotted a socket), TO_CREATE (process is waiting for socket to be created), CREATED (socket is created), TO_BIND (process is waiting for socket to be bound), BOUND (socket is bound), TO_CLOSE (process is waiting for socket to be closed)

    int pid;                 // process id of the process using the socket
    int sockfd;             // socket file descriptor of the udp socket
    char dest_ip[16];        // destination ip address of the socket
    int dest_port;          // destination port number of the socket
    char src_ip[16];        // source ip address of the socket
    int src_port;           // source port number of the socket
    char send_buffer[WINDOW_SIZE][MESSAGE_SIZE]; // buffer to store the messages to be sent
    int send_buffer_msg_size[WINDOW_SIZE];  // lenght of message stored in buffer (ith index corresponds to ith message in buffer)
    int send_msg_count;     // number of messages in buffer currently
    int send_ptr;           // pointer to first unacknowledged message in buffer
    char recv_buffer[WINDOW_SIZE][MESSAGE_SIZE]; // buffer to store the messages received
    int recv_buffer_msg_size[WINDOW_SIZE];        // lenght of message stored in buffer (ith index corresponds to ith message in buffer)
    int recv_msg_count;     // number of messages in buffer currently
    int recv_ptr;           // pointer to first unread message in buffer
    int nospace;            // Flag to track if receiver had no space
    int sent_but_not_acked; // Count of messages sent but not acked
    int send_retries;       // number of times the oldest unacked message is being retried
    window swnd;            // send window
    window rwnd;            // reciever window

    time_t time_sent[WINDOW_SIZE]; // time when message was sent
}SM;

// sizes of message buffer (number of messages that can be stored in buffer) is equal to size of window in swnd. this is used to keep a one to one mapping between the sequence number and the message in buffer. thus for i th message, its related inforamtion can be found in the window structure at index i (accordingly for send buffer <-> swnd and reciever buffer <-> rwnd)

typedef struct packet{
    int seq_no;             // sequence number of the message
    int ack_no;             // acknowledgement number of the message
    char data[MESSAGE_SIZE]; // data of the message
    int len;                // length of the message
    int flag;               //bits 0: fin, 1: syn, 2: ack, 3: nospace
    int window;             // if an ack message, window size of the receiver is advertised through this field, otherwise not used
}packet;

extern SM* SM_table;     // shared memory table for all the sockets
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