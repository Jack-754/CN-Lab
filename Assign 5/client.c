// =====================================
// Assignment 5 Submission
// Name: More Aayush Babasaheb
// Roll number: 22CS30063
// =====================================

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
#include <fcntl.h>


int sockfd;

int main(){
    // socket setup --------------------------------------------------------------------
    int clilen;
    struct sockaddr_in cli_addr, serv_addr;
    char buf[100];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Cannot create socket");
        close(sockfd);
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);
	serv_addr.sin_port = htons(20000);

    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0){
        perror("Unable to connect to server\n");
        close(sockfd);
        exit(0);
    }


    // main loop ----------------------------------------------------------------------
    int count=0;
    while (1){
        strcpy(buf, "GET_TASK");
        send(sockfd, buf, strlen(buf) + 1, 0);
        int bytes_received = recv(sockfd, buf, 100, 0);
        if(bytes_received<0){
            perror("Error receiving task from server\n");
            close(sockfd);
            exit(0);
        }
        else if(bytes_received==0){
            printf("Server disconnected\n");
            close(sockfd);
            exit(0);
        }
        if(strcmp(buf, "No tasks  available")==0){
            printf("No tasks available\n");
            close(sockfd);
            exit(0);
        }
        printf("Received task: %s\n", buf);
        int num1, num2;
        char op;
        if(sscanf(buf, "Task: %d %c %d", &num1, &op, &num2) != 3){
            printf("Invalid task format\n");
            close(sockfd);
            exit(0);
        }
        int result;
        switch(op){
            case '+':
                result = num1 + num2;
                break;
            case '-':
                result = num1 - num2;
                break;
            case '*':
                result = num1 * num2;
                break;
            case '/':
                result = num1 / num2;
                break;
            default:
                printf("Invalid operator\n");
                close(sockfd);
                exit(0);
        }
        sleep(1); //simulate some processing time
        sprintf(buf, "RESULT %d", result);
        printf("Sending result: %s\n", buf);
        send(sockfd, buf, strlen(buf) + 1, 0);
        printf("Task completed\n");
        count++;
        if(count==10){
            strcpy(buf, "exit");
            send(sockfd, buf, strlen(buf) + 1, 0);
            printf("Exiting\n");
            close(sockfd);
            exit(0);
        }
    }
}
