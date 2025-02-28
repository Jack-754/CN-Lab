/*
NAME: MORE AAYUSH BABASAHEB
ROLL NO.: 22CS30063
ASSIGNMENT: 4
*/
#ifndef K_SOCKET
#define K_SOCKET

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#define T 5 
#define p 0.2
#define PSH 1
#define ACK 0
#define SOCK_KTP 100
#define MSG_SIZE 512
#define PKT_SIZE 521
#define MAX_RETRIES 20
#define ENOSPACE 1
#define ENOTBOUND 2

extern int curseq;
extern struct sockaddr_in* destination;

int k_socket(int domain, int type, int protocol);
int k_bind(int sockfd, struct sockaddr_in* src, struct sockaddr_in* dest);
int k_sendto(int sockfd, int* seq, char* buf, int len, struct sockaddr_in* dest);
int k_recvfrom(int sockfd, int* seq, char* buf, int len);
int k_close(int sockfd);

void make_pkt(int mode, int seq, char* message, int msg_size, char* buf, int len);
void deconstruct_pkt(int* mode, int* seq, char* buf, int len, char* message, int msg_size);

int DropMessage();
void to_binary(int num, char *binary_str);

#endif