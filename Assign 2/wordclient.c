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
#define MAXLINE 1001 
  
int main() 
{   
    char buffer[MAXLINE+1]; 

    char message[MAXLINE]; 
    printf("Enter file name to request server: ");
    scanf("%s", message);
    int sockfd, n;
    struct sockaddr_in servaddr; 
      
    // clear servaddr 
    bzero(&servaddr, sizeof(servaddr)); 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_family = AF_INET; 
      
    // create datagram socket 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
    
    // request to send datagram 
    sendto(sockfd, message, MAXLINE, 0, (struct sockaddr*)&servaddr, sizeof(servaddr)); 

    // waiting for response 
    n=recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL); 
    buffer[n]='\0';

    if(strcmp(buffer, "HELLO")==0){     // check if the special keyword is recieved for starting the conversation
        int cnt=1;
        FILE *file = fopen("received_words.txt", "w");
        fprintf(file, "%s\n",buffer);
        while(1){
            sprintf(message, "WORD%d", cnt);
            cnt++;
            sendto(sockfd, message, MAXLINE, 0, (struct sockaddr*)&servaddr, sizeof(servaddr));     // sending request for next word
            n=recvfrom(sockfd, buffer, MAXLINE, 0, NULL, NULL);     // receiving next word
            buffer[n]='\0';
            fprintf(file, "%s\n",buffer);               // saving the word to file
            if(strcmp(buffer, "FINISH")==0){            // special word FINISH received. end of word transmission.
                fclose(file);
                close(sockfd);
                return 0;
            }
        }
    }
    else{
        printf("FILE NOT FOUND\n" );
    }

    close(sockfd);  // close the descriptor 
    return 0;
    
} 
