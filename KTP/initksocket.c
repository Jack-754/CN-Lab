#include "ksocket.h"
#include <signal.h>
#include <pthread.h>
#include <sys/select.h>

void free_resources(){
    shmdt(SM_table);
    semctl(sem_SM, 0, IPC_RMID);
    semctl(sem1, 0, IPC_RMID);
    semctl(sem2, 0, IPC_RMID);
    shmctl(shmid_SM, IPC_RMID, NULL);
}

void sig_handler(int sig){
    free_resources();
    exit(0);
}

int max(int a, int b){
    return (a>b)?a:b;
}

int seqtoidx(int seq, int curseq, int pointer){
    int tmp=seq-curseq;
    if(tmp<0)tmp+=256;
    return (pointer+tmp)%WINDOW_SIZE;
}

int incr(int cur, int mn, int mx){
    cur++;
    if(cur>mx)cur=mn;
    return cur;
}

void print_sm_table_entry(int sockfd) {
    if (sockfd < 0 || sockfd >= N || SM_table[sockfd].state == FREE) {
        printf("Socket %d is invalid or not allocated.\n", sockfd);
    } else {
        printf("Socket %d State: %d\n", sockfd, SM_table[sockfd].state);
        printf("Sent but not acknowledged: %d\n", SM_table[sockfd].sent_but_not_acked);
        printf("Send window pointer: %d\n", SM_table[sockfd].swnd.pointer);
        printf("Send window sequence: %d\n", SM_table[sockfd].swnd.seq);
        printf("Send window size: %d\n", SM_table[sockfd].swnd.size);
        for(int i=0; i<WINDOW_SIZE; i++){
            printf("%3d ", SM_table[sockfd].swnd.wndw[i]);
        }
        printf("\n");
        for(int i=0; i<WINDOW_SIZE; i++){
            printf("%3d ", SM_table[sockfd].send_buffer_msg_size[i]);
        }
        printf("\n");
        printf("Receive window pointer: %d\n", SM_table[sockfd].rwnd.pointer);
        printf("Receive window sequence: %d\n", SM_table[sockfd].rwnd.seq);
        printf("Receive window size: %d\n", SM_table[sockfd].rwnd.size);
        for(int i=0; i<WINDOW_SIZE; i++){
            printf("%3d ", SM_table[sockfd].rwnd.wndw[i]);
        }
        printf("\n");
        for(int i=0; i<WINDOW_SIZE; i++){
            printf("%3d ", SM_table[sockfd].recv_buffer_msg_size[i]);
        }
        printf("\n");
    }
    printf("************************************************\n\n");
}


void * R(){
    printf("Started R thread\n");
    fflush(stdout);
    fd_set readfds;
    struct timeval tv;
    tv.tv_sec=1;
    tv.tv_usec=0;
    while(1){
        FD_ZERO(&readfds);
        P(sem_SM);
        int mx=0;
        for(int i=0; i<N; i++){
            if(SM_table[i].state==BOUND){
                FD_SET(SM_table[i].sockfd, &readfds);
                mx=max(mx, SM_table[i].sockfd);
            }
        }
        V(sem_SM);

        int activity = select(mx+1, &readfds, NULL, NULL, &tv);
        if(activity < 0){
            perror("Select failed.\n");
            free_resources();
            exit(1);
        }

        // Handle select timeout - check for nospace condition
        if(activity == 0) {
            printf("Select timeout\n");
            tv.tv_sec = 1;    // Reinitialize timeout
            tv.tv_usec = 0;
            
            P(sem_SM);
            for(int i=0; i<N; i++) {
                if(SM_table[i].state == BOUND && SM_table[i].nospace > 0) {
                    
                    // If space is available, send duplicate ACK
                    if(SM_table[i].rwnd.size > 0) {
                        printf("Space available for socket %d\n", i);
                        if(SM_table[i].nospace > MAX_TRIES) {
                            // Close connection if max tries exceeded
                            printf("Max tries exceeded for socket %d, closing connection\n", i);
                            SM_table[i].state = TO_CLOSE;
                            V(sem1);
                            P(sem2);
                            continue;
                        }
                        printf("Sending space available notification for socket %d\n", i);
                        packet ack;
                        ack.flag = (1 << 2) | (1 << 3);  // Set ACK flag and 4th bit
                        ack.ack_no = SM_table[i].rwnd.seq - 1;
                        ack.window = SM_table[i].rwnd.size;
                        
                        struct sockaddr_in addr;
                        memset(&addr, 0, sizeof(addr));
                        addr.sin_addr.s_addr = inet_addr(SM_table[i].dest_ip);
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(SM_table[i].dest_port);
                        
                        sendto(SM_table[i].sockfd, &ack, sizeof(ack), 0, 
                               (struct sockaddr*)&addr, sizeof(addr));
                        
                        printf("Space available notification sent for socket %d (attempt %d)\n", i, SM_table[i].nospace);
                        SM_table[i].nospace++;  // Increment nospace counter
                    }
                }
            }
            V(sem_SM);
            continue;
        }

        P(sem_SM);
        for(int i=0; i<N; i++){
            if(SM_table[i].state==BOUND && FD_ISSET(SM_table[i].sockfd, &readfds)){
                packet tmp;
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_addr.s_addr=inet_addr(SM_table[i].dest_ip);
                addr.sin_family=AF_INET;
                addr.sin_port=htons(SM_table[i].dest_port);
                int len=recvfrom(SM_table[i].sockfd, &tmp, sizeof(packet), 0, NULL, NULL);

                // DEBUGGING
                char str[11]; // 10 characters + null terminator
                strncpy(str, tmp.data, 10);
                str[10] = '\0'; // Null-terminate the string
                printf("R seq:%d ack:%d flag:%d window:%d len:%d str:%s\n", tmp.seq_no, tmp.ack_no, tmp.flag, tmp.window, tmp.len, str);

                if(len<0){
                    perror("Recvfrom failed.\n");
                    free_resources();
                    exit(1);
                }
                if(dropMessage()){
                    if(tmp.flag&(1<<2)){
                        printf("ACK for packet %d dropped for socket %d\n", tmp.ack_no, i);
                        continue;
                    }
                    else{
                        printf("Packet %d dropped for socket %d\n", tmp.seq_no, i);
                        continue;
                    }
                }
                if(tmp.flag&(1<<2)){
                    printf("Received ACK for packet %d from socket %d\n", tmp.ack_no, i);
                    
                    // Handle ACK with 4th bit set (space notification)
                    if(tmp.flag & (1 << 3)) {
                        SM_table[i].swnd.size = tmp.window;
                        
                        // If send buffer is empty, send acknowledgment
                        if(SM_table[i].send_msg_count == 0) {
                            printf("Sending space available notification acknowdledgment for socket %d\n", i);
                            packet response;
                            response.flag = 1 << 3;  // Set only 4th bit
                            sendto(SM_table[i].sockfd, &response, sizeof(response), 0,
                                   (struct sockaddr*)&addr, sizeof(addr));
                        }
                        continue;
                        // Continue normal ACK processing
                    }

                    int expected_seq = SM_table[i].swnd.seq;
                    int ack_seq = tmp.ack_no;

                    int diff = (ack_seq - expected_seq + 256) % 256;
                    if (diff < 0) {
                        // Invalid ACK, ignore
                        printf("Invalid ACK received for packet %d from socket %d\n", tmp.ack_no, i);
                        continue;
                    }

                    for (int j = 0; j < diff; j++) {
                        int idx = (SM_table[i].swnd.pointer + j) % WINDOW_SIZE;
                        if (SM_table[i].swnd.wndw[idx] == SENT || SM_table[i].swnd.wndw[idx] == ACKED) {
                            SM_table[i].swnd.wndw[idx] = ACKED;
                        }
                    }

                    // Move the window pointer and update sequence numbers
                    SM_table[i].send_ptr=SM_table[i].swnd.pointer = (SM_table[i].swnd.pointer + diff) % WINDOW_SIZE;
                    SM_table[i].swnd.seq = (SM_table[i].swnd.seq + diff-1) % 256+1;

                    // Update send buffer counts and window size
                    SM_table[i].sent_but_not_acked -= diff;
                    SM_table[i].send_msg_count -= diff;
                    SM_table[i].swnd.size = tmp.window;

                    // Reset send buffer entries
                    for (int j = 0; j < diff; j++) {
                        int idx = (SM_table[i].swnd.pointer - diff + j) % WINDOW_SIZE;
                        SM_table[i].send_buffer_msg_size[idx] = 0;
                        SM_table[i].swnd.wndw[idx] = WFREE;
                    }
                }
                // Reset nospace if 4th bit is set
                else if(tmp.flag & (1 << 3)) {
                    SM_table[i].nospace = 0;
                    printf("Resetting nospace for socket %d\n", i);
                    continue;
                }
                else{
                    int seq_no = tmp.seq_no;
                    int curseq = SM_table[i].rwnd.seq;
                    int diff = (seq_no - curseq + 256) % 256;
                    if (diff >= SM_table[i].rwnd.size) {
                        printf("Packet %d out of window for socket %d\n", seq_no, i);
                        continue;
                    }
                    int idx=seqtoidx(tmp.seq_no, SM_table[i].rwnd.seq, SM_table[i].rwnd.pointer);
                    printf("Received packet %d from socket %d\n", tmp.seq_no, i);

                    print_sm_table_entry(i);

                    SM_table[i].rwnd.wndw[idx]=RECVD;
                    for (int j = 0; j < tmp.len; j++) {
                        SM_table[i].recv_buffer[idx][j] = tmp.data[j];
                    }
                    SM_table[i].recv_buffer_msg_size[idx]=tmp.len;
                    SM_table[i].recv_msg_count++;
                    SM_table[i].rwnd.size = WINDOW_SIZE - SM_table[i].recv_msg_count;
                    if (SM_table[i].rwnd.size == 0) {
                        SM_table[i].nospace = 1;
                    } else {
                        SM_table[i].nospace = 0;
                    }
                    // JUST ADDED LOGIC FOR CUMULATIVE ACK'S CHECK'S REQUIRED
                    if(curseq==seq_no){
                        int cumulative=0;
                        while(SM_table[i].rwnd.wndw[SM_table[i].rwnd.pointer]==RECVD){
                            cumulative++;
                            SM_table[i].rwnd.pointer=(SM_table[i].rwnd.pointer+1)%WINDOW_SIZE;
                            if(cumulative==WINDOW_SIZE)break;
                        }
                        curseq=SM_table[i].rwnd.seq=(curseq+cumulative-1)%256+1;
                    }
                    print_sm_table_entry(i);
                    packet ack;
                    ack.ack_no=curseq;
                    ack.flag=(1<<2);
                    ack.window = SM_table[i].rwnd.size;
                    sendto(SM_table[i].sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&addr, sizeof(addr));
                    
                }
            }
        }
        V(sem_SM);
    }
}

void * S(){
    printf("Started S thread\n");
    fflush(stdout);
    while(1){sleep(T/2);
        P(sem_SM);
        for(int i=0; i<N; i++){
            // add check for bound state in ksendto function
            if(SM_table[i].send_msg_count>0){
                // create and send packet
                packet tmp;
                int idx=-1, seq=SM_table[i].swnd.seq;
                for(int j=0; j<SM_table[i].send_msg_count; j++){
                    time_t curtime=time(NULL);

                    idx = (SM_table[i].swnd.pointer + j) % WINDOW_SIZE;
                    if(SM_table[i].swnd.wndw[idx]==NOT_SENT || (SM_table[i].swnd.wndw[idx] == SENT && (curtime - SM_table[i].time_sent[idx]) >= T)){
                        if(SM_table[i].swnd.wndw[idx]==NOT_SENT){
                            if(SM_table[i].swnd.size>0) {
                                SM_table[i].swnd.size--;
                                SM_table[i].sent_but_not_acked++;
                            } else {
                                continue;  // Skip if window is full
                            }
                        } 
                        for(int j=0; j<SM_table[i].send_buffer_msg_size[idx]; j++){
                            tmp.data[j]=SM_table[i].send_buffer[idx][j];
                        }
                        tmp.flag=0;
                        tmp.seq_no = (SM_table[i].swnd.seq + j - 1) % 256 + 1;
                        tmp.len=SM_table[i].send_buffer_msg_size[idx];
                        struct sockaddr_in addr;
                        memset(&addr, 0, sizeof(addr));
                        addr.sin_addr.s_addr=inet_addr(SM_table[i].dest_ip);
                        addr.sin_family=AF_INET;
                        addr.sin_port=htons(SM_table[i].dest_port);

                        // DEBUGGING
                        char str[11]; // 10 characters + null terminator
                        strncpy(str, tmp.data, 10);
                        str[10] = '\0'; // Null-terminate the string
                        printf("S idx:%d seq:%d flag:%d window:%d len:%d str:%s\n", idx, tmp.seq_no, tmp.flag, tmp.window, tmp.len, str);
                        print_sm_table_entry(i);

                        sendto(SM_table[i].sockfd, &tmp, sizeof(tmp), 0, (struct sockaddr*)&addr, sizeof(addr));
                        SM_table[i].time_sent[idx] = time(NULL);  // Update next send time
                        SM_table[i].swnd.wndw[idx] = SENT;        // Mark as sent
                    }
                }

            }
        }
        V(sem_SM);
    }
}

void * G(){
    printf("Started G thread\n");
    fflush(stdout);
    while(1){
        sleep(T);
        P(sem_SM);
        for(int i=0; i<N; i++){
            if(SM_table[i].state!=FREE && kill(SM_table[i].pid, 0)!=0){
                if(SM_table[i].state==BOUND){
                    if(close(SM_table[i].sockfd)<0){
                        perror("Close failed.\n");
                        free_resources();
                        exit(1);
                    }
                }
                printf("Freeing socket %d\n", i);
                SM_table[i].state=FREE;
                for(int j = 0; j < WINDOW_SIZE; j++) {
                    SM_table[i].swnd.wndw[j] = WFREE;  
                    SM_table[i].rwnd.wndw[j] = WFREE;
                }
                SM_table[i].swnd.size = WINDOW_SIZE;
                SM_table[i].rwnd.size = WINDOW_SIZE;
            }
        }
        V(sem_SM);
    }
}

int main(){
    printf("Starting initksocket.c main\n");
    srand(time(0));
    signal(SIGINT, sig_handler);

    int key_sem_SM, key_shmid_SM, key_sem1, key_sem2;
    key_sem_SM=ftok("makefile", 1);
    key_shmid_SM = ftok("makefile", 2);
    key_sem1=ftok("makefile", 3);
    key_sem2=ftok("makefile", 4);

    printf("Creating semaphores and shared memory\n");
    fflush(stdout);
    if(key_sem_SM==-1 || key_shmid_SM==-1 || key_sem1==-1 || key_sem2==-1){
        perror("ftok failed in initksocket.c\n");
        exit(1);
    }

    sem_SM=semget(key_sem_SM, 1, 0666|IPC_CREAT);
    sem1=semget(key_sem1, 1, 0666|IPC_CREAT);
    sem2=semget(key_sem2, 1, 0666|IPC_CREAT);
    shmid_SM=shmget(key_shmid_SM, sizeof(struct SM)*N, 0666|IPC_CREAT);

    printf("%d \n", shmid_SM);

    if(sem_SM==-1 || sem1==-1 || sem2==-1 || shmid_SM==-1){
        perror("semget or shmget failed in initksocket.c\n");
        exit(1);
    }

    if(semctl(sem_SM, 0, SETVAL, 1)==-1 || semctl(sem1, 0, SETVAL, 0)==-1 || semctl(sem2, 0, SETVAL, 0)==-1){
        perror("semctl SETVAL failed in initksocket.c\n");
        exit(1);
    }

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    SM_table=(struct SM*)shmat(shmid_SM, 0, 0);

    if(SM_table==(void *)-1){
        perror("shmat failed in ksocket.c\n");
        exit(1);
    }

    printf("About to take sem_SM semaphore\n");
    fflush(stdout);
    P(sem_SM);
    printf("Took sem_SM semaphore\n");
    fflush(stdout);

    for(int i=0; i<N; i++){
        SM_table[i].state=FREE;
        for(int j = 0; j < WINDOW_SIZE; j++) {
            SM_table[i].swnd.wndw[j] = WFREE;
            SM_table[i].rwnd.wndw[j] = WFREE;
        }
        SM_table[i].swnd.size = WINDOW_SIZE;
        SM_table[i].rwnd.size = WINDOW_SIZE;
    }
    V(sem_SM);

    printf("Creating threads\n");
    fflush(stdout);
    pthread_t S_thread, R_thread, G_thread;
    pthread_attr_t attr;
    int ret;

    // Initialize thread attributes
    if ((ret = pthread_attr_init(&attr)) != 0) {
        perror("pthread_attr_init failed\n");
        exit(1);
    }
    // Set detach state to detached
    if ((ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
        perror("pthread_attr_setdetachstate failed\n");
        pthread_attr_destroy(&attr);
        exit(1);
    }
    // Create S_thread
    if ((ret = pthread_create(&S_thread, &attr, S, NULL)) != 0) {
        perror("pthread_create for S_thread failed\n");
        pthread_attr_destroy(&attr);
        exit(1);
    }
    // Create R_thread
    if ((ret = pthread_create(&R_thread, &attr, R, NULL)) != 0) {
        perror("pthread_create for R_thread failed");
        pthread_attr_destroy(&attr);
        exit(1);
    }
    // Create G_thread
    if ((ret = pthread_create(&G_thread, &attr, G, NULL)) != 0) {
        perror("pthread_create for G_thread failed\n");
        pthread_attr_destroy(&attr);
        exit(1);
    }
    // Destroy thread attributes (no longer needed)
    pthread_attr_destroy(&attr);

    // main loop for creating, binding and closing sockets, services are requested through semaphore sem1 and sem2 is used to signal the completion of the service

    printf("Starting initksocket.c main while loop\n");
    fflush(stdout);
    while(1){
        P(sem1);

        for(int i=0; i<N; i++){
            if(SM_table[i].state==TO_CREATE){
                SM_table[i].sockfd=socket(AF_INET, SOCK_DGRAM, 0);
                if(SM_table[i].sockfd<0){
                    perror("Socket creation failed.\n");
                    free_resources();
                    exit(1);
                }
                SM_table[i].state=CREATED;
                SM_table[i].swnd.pointer=0;
                SM_table[i].swnd.seq=1;
                SM_table[i].swnd.size=WINDOW_SIZE;
                SM_table[i].rwnd.pointer=0;
                SM_table[i].rwnd.seq=1;
                SM_table[i].rwnd.size=WINDOW_SIZE;
                SM_table[i].send_msg_count=0;
                SM_table[i].recv_msg_count=0;
                SM_table[i].send_ptr=0;
                SM_table[i].recv_ptr=0;

                // additional initialisation may be required
                break;
            }
            else if(SM_table[i].state==TO_BIND){
                struct sockaddr_in addr;
                addr.sin_addr.s_addr=inet_addr(SM_table[i].src_ip); 
                addr.sin_port=htons(SM_table[i].src_port); 
                addr.sin_family=AF_INET;
                if(bind(SM_table[i].sockfd, (struct sockaddr*)&addr, sizeof(addr))<0){
                    perror("Bind failed.\n");
                    free_resources();
                    exit(1);
                }
                SM_table[i].state=BOUND;
                break;
            }
            else if(SM_table[i].state==TO_CLOSE){
                if(close(SM_table[i].sockfd)<0){
                    perror("Close failed.\n");
                    free_resources();
                    exit(1);
                }
                SM_table[i].state=FREE;
                for(int j = 0; j < WINDOW_SIZE; j++) {
                    SM_table[i].swnd.wndw[j] = WFREE;  
                    SM_table[i].rwnd.wndw[j] = WFREE;
                }
                SM_table[i].swnd.size = WINDOW_SIZE;
                SM_table[i].rwnd.size = WINDOW_SIZE;
                break;
            }
        }

        V(sem2);
    }



}