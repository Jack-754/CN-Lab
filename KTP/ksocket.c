#include "ksocket.h"
#include <assert.h>

struct SM * SM_table = NULL;

// semaphore and shared memory variables
int sem_SM, shmid_SM, sem1, sem2;
struct sembuf pop, vop;

void init(){
    if(SM_table!=NULL)return;

    //retrieving keys for the shared memories
    int key_sem_SM, key_shmid_SM, key_sem1, key_sem2;
    key_sem_SM=ftok("makefile", 1);
    key_shmid_SM = ftok("makefile", 2);
    key_sem1=ftok("makefile", 3);
    key_sem2=ftok("makefile", 4);

    if(key_sem_SM==-1 || key_shmid_SM==-1 || key_sem1==-1 || key_sem2==-1){
        perror("ftok failed in ksocket.c\n");
        exit(1);
    }

    //getting the shared memory and semaphores
    sem_SM=semget(key_sem_SM, 1, 0666);
    sem1=semget(key_sem1, 1, 0666);
    sem2=semget(key_sem2, 1, 0666);
    shmid_SM=shmget(key_shmid_SM, sizeof(struct SM)*N, 0666);

    if(sem_SM==-1 || sem1==-1 || sem2==-1 || shmid_SM==-1){
        perror("semget or shmget failed in ksocket.c\n");
        exit(1);
    }

    //attaching shared memory table to current process
    SM_table=(SM*)shmat(shmid_SM,0,0);

    if(SM_table==(void *)-1){
        perror("shmat failed in ksocket.c\n");
        exit(1);
    }

    //initialising sembuf structures
    pop.sem_num=vop.sem_num=0;
    pop.sem_flg=vop.sem_flg=0;
    pop.sem_op=-1;
    vop.sem_op=1;
}

int dropMessage() {
    float r = (float)rand() / (float)RAND_MAX;
    if (r < p) return 1;
    return 0;
}

int k_socket(int domain, int type, int protocol){
    // checking for socket type
    if(type!=SOCK_KTP){
        errno=EINVAL;
        return -1;
    }
    // initialising the shared memory segments
    init();

    // obtaining a free slot in the shared memory table
    int sockfd;
    sockfd=free_slot();
    if(sockfd==-1){
        errno=ENOSPACE;
        return -1;
    }

    // signalling initksocket main loop for service request
    P(sem_SM);
    SM_table[sockfd].state=TO_CREATE;
    SM_table[sockfd].pid=getpid();
    V(sem1);
    P(sem2);
    printf("Socket created at %d\n", sockfd);
    V(sem_SM);

    return sockfd;
}

int free_slot() {
    P(sem_SM);
    for(int i=0;i<N;i++){
        if(SM_table[i].state==FREE){
            SM_table[i].state=ALLOTED;
            V(sem_SM);
            return i;
        }
    }
    V(sem_SM);
    return -1;
}

int k_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port){
    // initialising shared memory
    init();

    int sockfd=-1, cur_pid=getpid();
    P(sem_SM);
    // searching for socket created by current process
    for(int i=0; i<N; i++){
        if(SM_table[i].pid==cur_pid){
            sockfd=i;
            break;
        }
    }

    // error checking for socket created by this process
    if(sockfd==-1 || SM_table[sockfd].state!=CREATED){
        perror("No socket created by current process.\n");
        return -1;
    }

    // storing binding details in shared memory table
    SM_table[sockfd].state=TO_BIND;
    strcpy(SM_table[sockfd].dest_ip, dest_ip);
    SM_table[sockfd].dest_port=dest_port;
    strcpy(SM_table[sockfd].src_ip, src_ip);
    SM_table[sockfd].src_port=src_port;
    SM_table[sockfd].sent_but_not_acked=0;
    for(int j=0; j<WINDOW_SIZE; j++){
        SM_table[sockfd].swnd.wndw[j]=WFREE;
        SM_table[sockfd].rwnd.wndw[j]=WFREE;
    }
    // requesting service from initksocket main while loop
    V(sem1);
    P(sem2);
    printf("Socket %d bounded.\n", sockfd);
    V(sem_SM);
    return 0;
}

ssize_t k_sendto(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, socklen_t *addrlen){
    // checking for message size correctness
    if(len > MESSAGE_SIZE || len < 0){
        perror("Message size specified by len attribute not in correct range.\n");
        exit(1);
    }

    // initializing the shared memory
    init();

    // locking the semaphore for shared memory access
    P(sem_SM);

    // checking if the socket file descriptor is valid and bound
    if(sockfd >= N || sockfd < 0 || SM_table[sockfd].state != BOUND){
        perror("Wrong sockfd.\n");
        V(sem_SM);
        return -1;
    }

    // checking if the destination address matches the bound address
    struct sockaddr_in addr = *((struct sockaddr_in*)dest_addr);
    if(addr.sin_addr.s_addr != inet_addr(SM_table[sockfd].dest_ip) || addr.sin_port != htons(SM_table[sockfd].dest_port)){
        errno = ENOTBOUND;
        V(sem_SM);
        return -1;
    }

    // checking if there is space in the send buffer
    if(SM_table[sockfd].send_msg_count == WINDOW_SIZE){
        errno = ENOSPACE;
        V(sem_SM);
        return -1;
    }

    // copying the message to the send buffer
    int next_free = (SM_table[sockfd].send_ptr + SM_table[sockfd].send_msg_count) % WINDOW_SIZE;
    printf("next_free: %d send_ptr:%d send_msg_count:%d window:%d\n", next_free, SM_table[sockfd].send_ptr, SM_table[sockfd].send_msg_count, SM_table[sockfd].swnd.size);
    for(int i = 0; i < len; i++) {
        SM_table[sockfd].send_buffer[next_free][i] = ((char*)buf)[i];
    }

    // updating the send message count
    SM_table[sockfd].send_msg_count++;
    SM_table[sockfd].swnd.wndw[next_free] = NOT_SENT;
    SM_table[sockfd].send_buffer_msg_size[next_free] = len;

    // unlocking the semaphore for shared memory access
    V(sem_SM);

    // printing a message indicating that the message was sent
    printf("Message sent.\n");

    return len;
}

ssize_t k_recvfrom(int sockfd, void *buf, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    // Initialize shared memory and semaphores
    init();
    int len;

    fflush(NULL);
    // Lock the semaphore for shared memory access
    P(sem_SM);
    fflush(NULL);

    // Check if the socket file descriptor is valid and bound
    if(sockfd >= N || sockfd < 0 || SM_table[sockfd].state != BOUND){
        perror("Wrong sockfd.\n");
        V(sem_SM);
        return -1;
    }

    // Check if there are any messages in the receive buffer
    if(SM_table[sockfd].recv_msg_count == 0 || SM_table[sockfd].rwnd.wndw[SM_table[sockfd].recv_ptr] != RECVD){
        errno = ENOMESSAGE;
        V(sem_SM);
        return -1;
    }

    // Copy the message from the receive buffer to the provided buffer
    for(int i = 0; i < SM_table[sockfd].recv_buffer_msg_size[SM_table[sockfd].recv_ptr]; i++){
        ((char*)buf)[i] = SM_table[sockfd].recv_buffer[SM_table[sockfd].recv_ptr][i];
    }

    // Get the length of the received message
    len = SM_table[sockfd].recv_buffer_msg_size[SM_table[sockfd].recv_ptr];

    printf("recv_msg_count: %d recv_ptr: %d len: %d\n", SM_table[sockfd].recv_msg_count, SM_table[sockfd].recv_ptr, len);

    // Mark the message as read
    SM_table[sockfd].rwnd.wndw[SM_table[sockfd].recv_ptr] = WFREE;

    // Update the receive message count and pointer
    SM_table[sockfd].recv_msg_count--;
    SM_table[sockfd].rwnd.size=WINDOW_SIZE-SM_table[sockfd].recv_msg_count;
    SM_table[sockfd].recv_ptr = (SM_table[sockfd].recv_ptr + 1) % WINDOW_SIZE;

    // If src_addr is not NULL, fill it with the source address information
    if(src_addr != NULL){
        struct sockaddr_in *addr = (struct sockaddr_in*)src_addr;
        addr->sin_addr.s_addr = inet_addr(SM_table[sockfd].dest_ip);
        addr->sin_port = htons(SM_table[sockfd].dest_port);
        addr->sin_family = AF_INET;
    }

    // If addrlen is not NULL, set it to the size of struct sockaddr_in
    if(addrlen != NULL){
        (*addrlen) = sizeof(struct sockaddr_in);
    }

    // Unlock the semaphore for shared memory access
    V(sem_SM);

    // Print a message indicating that the message was received
    printf("Message received.\n");

    return len;
}

int k_close(int sockfd){
    init();
    P(sem_SM);
    if(sockfd>=N || sockfd<0 || SM_table[sockfd].state==FREE){
        perror("Socket can't be closed.\n");
        return -1;
    }
    SM_table[sockfd].state=TO_CLOSE;
    V(sem1);
    P(sem2);
    V(sem_SM);
    printf("Socket %d closed.\n", sockfd);
    shmdt(SM_table);
    return 0;
}