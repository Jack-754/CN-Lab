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

// Global socket file descriptor
int sockfd;

int main(){
    // Variables for socket connection
    int clilen;
    struct sockaddr_in cli_addr, serv_addr;
    char buf[100];

    // Create TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Cannot create socket");
        close(sockfd);
        exit(0);
    }

    // Configure server address details
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(20000);

    // Connect to the server
    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0){
        perror("Unable to connect to server\n");
        close(sockfd);
        exit(0);
    }
    printf("Connected to server\n");
    
    //Uncomment to test timeout after connection
    //sleep(40);

    // Counter for number of tasks completed
    int count=0;
    while (1){
        // Request a new task from server
        strcpy(buf, "GET_TASK");
        if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
            perror("Error sending task request to server");
            close(sockfd);
            exit(1);
        }

        // Uncomment to test repeated GET_TASK
        // if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        //     perror("Error sending task request to server");
        //     close(sockfd);
        //     exit(1);
        // }
        
        // Receive task from server
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
        
        // Check if no tasks are available
        if(strcmp(buf, "No tasks  available")==0){
            printf("No tasks available\n");
            close(sockfd);
            exit(0);
        }
        printf("Received task: %s\n", buf);
        
        // Parse the arithmetic task
        int num1, num2;
        char op;
        if(sscanf(buf, "Task: %d %c %d", &num1, &op, &num2) != 3){
            printf("Invalid task format\n");
            close(sockfd);
            exit(0);
        }
        
        // Perform the arithmetic operation
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
        
        // Simulate processing time
        sleep(1);
        

        //Uncomment to test timeout after GET_TASK
        //sleep(40);
        
        // Send result back to server
        sprintf(buf, "RESULT %d", result);
        printf("Sending result: %s\n", buf);
        if (send(sockfd, buf, strlen(buf) + 1, 0) < 0) {
            perror("Error sending result to server");
            close(sockfd);
            exit(1);
        }
        printf("Task completed\n");
        
        // Exit after completing 10 tasks
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
