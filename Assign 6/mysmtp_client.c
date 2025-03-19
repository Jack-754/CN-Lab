// =====================================
// Assignment 6 Submission
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
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAXSIZE 10000

// Variables for socket connection
int clilen;
struct sockaddr_in cli_addr, serv_addr;
char buf[MAXSIZE];
int sockfd;

void discard_line() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF); // Consume remaining characters
}


void safe_recv_and_print() {
    int total_received = 0;

    while (total_received < MAXSIZE - 1) {
        int bytes_received = recv(sockfd, buf + total_received, MAXSIZE - total_received - 1, 0);
        if (bytes_received < 0) {
            perror("Error receiving from server");
            close(sockfd);
            exit(1);
        } else if (bytes_received == 0) {
            printf("Server disconnected\n");
            close(sockfd);
            exit(1);
        }

        // Check if null terminator is received
        if (memchr(buf+total_received, '\0', bytes_received) != NULL) {
            total_received += bytes_received;
            break;
        }
        total_received += bytes_received;
    }

    // Ensure buffer is null-terminated
    buf[total_received] = '\0';

    printf("%s\n", buf);
}

void HELO(){
    char clientid[100];
    scanf("%s", clientid);
    sprintf(buf, "HELO %s", clientid);
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (HELO) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
}

void MAIL_FROM(){
    char email[100];
    scanf("%s", email);
    sprintf(buf, "MAIL FROM: %s", email);
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (MAIL FROM:) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
}

void RCPT_TO(){
    char email[100];
    scanf("%s", email);
    sprintf(buf, "RCPT TO: %s", email);
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (RCPT TO:) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
}

void DATA(){
    printf("Enter your message (end with a single dot'.'):\n");
    int len=4;
    buf[0]='D';
    buf[1]='A';
    buf[2]='T';
    buf[3]='A';
    while(1){
        scanf("%c", &buf[len]);
        if(buf[len]=='.' && buf[len-1]=='\n'){
            buf[len]='\0';
            break;
        }
        len++;
    }
    printf("Reading complete\n");
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (DATA) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
}

void LIST(){
    char email[100];
    scanf("%s", email);
    sprintf(buf, "LIST %s", email);
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (LIST) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
}

void GET_MAIL(){
    char email[100];
    int id;
    scanf("%s %d", email, &id);
    sprintf(buf, "GET_MAIL %s %d", email, id);
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (LIST) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
}

void QUIT(){
    sprintf(buf, "QUIT");
    if(send(sockfd, buf, strlen(buf) + 1, 0) < 0){
        perror("Error sending request (QUIT) to server");
        close(sockfd);
        exit(1);
    }

    safe_recv_and_print();

    // int bytes_received = recv(sockfd, buf, MAXSIZE, 0);
    // if(bytes_received<0){
    //     perror("Error receiving from server\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // else if(bytes_received==0){
    //     printf("Server disconnected\n");
    //     close(sockfd);
    //     exit(0);
    // }
    // printf("%s\n", buf);
    // exit(0);
}



int main(int argc, char *argv[]){

    if(argc<3){
        printf("run as ./client $(server ip address) $(server port)");
        exit(1);
    }

    char * serverip = strdup(argv[1]);
    int serverport = atoi(argv[2]);

    

    // Create TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Cannot create socket");
        close(sockfd);
        exit(0);
    }

    // Configure server address details
    serv_addr.sin_family = AF_INET;
    inet_aton(serverip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(serverport);

    // Connect to the server
    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0){
        perror("Unable to connect to server\n");
        close(sockfd);
        exit(0);
    }

    printf("Connected to My_SMTP server.\n");

    char keyword[30];

    while(1){
        printf(">");
        scanf("%s", keyword);
        if(strcmp(keyword, "HELO")==0){
            HELO();
        }
        else if(strcmp(keyword, "MAIL")==0){
            scanf("%s", keyword);
            if(strcmp(keyword, "FROM:")==0){
                MAIL_FROM();
            }
            else{
                printf("Invalid command. Discarding input.\n");
                discard_line();  // Clear the rest of the line
            }
        }
        else if(strcmp(keyword, "RCPT")==0){
            scanf("%s", keyword);
            if(strcmp(keyword, "TO:")==0){
                RCPT_TO();
            }
            else{
                printf("Invalid command. Discarding input.\n");
                discard_line();  // Clear the rest of the line
            }
        }
        else if(strcmp(keyword, "DATA")==0){
            DATA();
        }
        else if(strcmp(keyword, "LIST")==0){
            LIST();
        }
        else if(strcmp(keyword, "GET_MAIL")==0){
            GET_MAIL();
        }
        else if(strcmp(keyword, "QUIT")==0){
            QUIT();
        }
        else{
            printf("Invalid command. Discarding input.\n");
            discard_line();  // Clear the rest of the line
        }

    }
}