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

void * R(){
    
        
}

void * S(){
    while(1){sleep(T/2);
        P(sem_SM);
        for(int i=0; i<N; i++){
            // add check for bound state in ksendto function
            if(SM_table[i].send_msg_count>0 && SM_table[i].swnd.size>0){
                // create and send packet
                packet tmp;
                int idx=-1, seq=SM_table[i].swnd.seq;
                for(int j=0; j<SM_table[i].send_msg_count; j++){
                    time_t curtime=time(NULL);
                    if(SM_table[i].swnd.wndw[SM_table[i].swnd.pointer+j]==0 && SM_table[i].time_sent[j+SM_table[i].swnd.pointer]<=curtime){
                        idx=j+SM_table[i].swnd.pointer;
                        break;
                    }
                    seq++;
                }
                if(idx==-1)continue;
                strcpy(tmp.data, SM_table[idx].send_buffer[idx]);
                tmp.flag=0;
                tmp.seq_no=seq;
                tmp.len=SM_table[i].send_buffer_msg_size[idx];


                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_addr.s_addr=inet_addr(SM_table[i].dest_ip);
                addr.sin_family=AF_INET;
                addr.sin_port=htons(SM_table[i].dest_port);
                sendto(SM_table[i].sockfd, &tmp, sizeof(tmp), 0, (struct sockaddr*)&addr, sizeof(addr));
                SM_table[i].swnd.size--;
            }
        }
        V(sem_SM);
    }
}

void * G(){
    while(1){
        sleep(T);
        P(sem_SM);
        for(int i=0; i<N; i++){
            if(SM_table[i].state!=FREE && kill(SM_table[i].pid, 0)!=0){
                SM_table[i].state=FREE;
            }
        }
        V(sem_SM);
    }
}

int main(){
    srand(time(0));
    signal(SIGINT, sig_handler);

    int key_sem_SM, key_shmid_SM, key_sem1, key_sem2;
    key_sem_SM=ftok("makefile", 1);
    key_shmid_SM = ftok("makefile", 2);
    key_sem1=ftok("makefile", 3);
    key_sem2=ftok("makefile", 4);

    if(key_sem_SM==-1 || key_shmid_SM==-1 || key_sem1==-1 || key_sem2==-1){
        perror("ftok failed in initksocket.c\n");
        exit(1);
    }

    sem_SM=semget(key_sem_SM, 1, 0666|IPC_CREAT);
    sem1=semget(key_sem1, 1, 0666|IPC_CREAT);
    sem2=semget(key_sem2, 1, 0666|IPC_CREAT);
    shmid_SM=shmget(key_shmid_SM, sizeof(struct SM)*N, 0666|IPC_CREAT);

    if(sem_SM==-1 || sem1==-1 || sem2==-1 || shmid_SM==-1){
        perror("semget or shmget failed in initksocket.c\n");
        exit(1);
    }

    if(semctl(sem_SM, 0, SETVAL, 1)==-1 || semctl(sem1, 0, SETVAL, 0)==-1 || semctl(sem2, 0, SETVAL, 0)==-1){
        perror("semctl SETVAL failed in initksocket.c\n");
        exit(1);
    }

    SM_table=(struct SM*)shmat(shmid_SM, 0, 0);

    if(SM_table==(void *)-1){
        perror("shmat failed in ksocket.c\n");
        exit(1);
    }

    P(sem_SM);
    for(int i=0; i<N; i++){
        SM_table[i].state=FREE;
    }
    V(sem_SM);

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
            else if(SM_table[i].state=TO_CLOSE){
                if(close(SM_table[i].sockfd)<0){
                    perror("Close failed.\n");
                    free_resources();
                    exit(1);
                }
                SM_table[i].state=FREE;
                break;
            }
        }

        V(sem2);
    }



}