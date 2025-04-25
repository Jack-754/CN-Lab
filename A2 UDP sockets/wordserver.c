/***************************************************************
 * ASSIGNMENT 2 SUBMISSION
 * NAME: MORE AAYUSH BABASAHEB
 * ROLL NO.: 22CS30063
 * Link of PCAP file: https://drive.google.com/file/d/17JDwDh3djSOd7i171t-dfF_8RYXHKgqS/view?usp=sharing
 ***************************************************************/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define MAXLINE 1000

int main()
{
    char buffer[MAXLINE+1];
    char message[2*MAXLINE];
    int serverfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    serverfd = socket(AF_INET, SOCK_DGRAM, 0);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    // bind server address to socket descriptor
    bind(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    printf("\nServer Running ...\n");
    FILE *file;
    while(1){
        len = sizeof(cliaddr);
        int n = recvfrom(serverfd, buffer, MAXLINE,
                0, (struct sockaddr *)&cliaddr, &len); // receive message from server

        buffer[n] = '\0';
        printf("\nRequest from Client: %s\n", buffer);
        file = fopen(buffer, "r");
        if (file == NULL)               // CASE 1: File doesn't exit, send error message, and wait for new request
        {
            sprintf(message, "NOTFOUND %s\n", buffer);
            sendto(serverfd, message, MAXLINE, 0,
                   (struct sockaddr *)&cliaddr, sizeof(cliaddr));
            continue;
        }
        fscanf(file, "%s", message);
        if(strcmp(message, "HELLO")!=0)  // CASE 2: User requests a file which exists but doesn't begin with HELLO thus, send error message, and wait for new request
        {
            sprintf(message, "NOTFOUND %s\n", buffer);
            sendto(serverfd, message, MAXLINE, 0,
            (struct sockaddr *)&cliaddr, sizeof(cliaddr));
        }
        else break;                 // CASE 3: User requests a file, it is in correct format, so start sending it 
    }

    
    sendto(serverfd, message, MAXLINE, 0,       // send initial HELLO message to client
           (struct sockaddr *)&cliaddr, sizeof(cliaddr));

    while (1)
    {
        int n = recvfrom(serverfd, buffer, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);      // recieve request for next word from client
        fscanf(file, "%s", message);
        sendto(serverfd, message, MAXLINE, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));    // send the next word to client
        if (strcmp(message, "FINISH") == 0)                                                     // last word sent, so terminate
        {   
            fclose(file);
            close(serverfd);
            exit(0);
        }
    }
}