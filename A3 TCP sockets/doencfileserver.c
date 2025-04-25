/***************************************************************
 * ASSIGNMENT 2 SUBMISSION
 * NAME: MORE AAYUSH BABASAHEB
 * ROLL NO.: 22CS30063
 * Link of PCAP file: https://drive.google.com/file/d/1_3EL8_lA_vzI0RCYj5lyZ6lYCpgevFK7/view?usp=sharing
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

int sockfd, newsockfd;

void handle_sigint(int sig) {
    printf("\nReceived SIGINT, closing socket and exiting...\n");
    close(sockfd);
    close(newsockfd);
    exit(0);
}

char *sockaddr_to_string(struct sockaddr *addr) {
    char ip[16]; 
    char *result = malloc(30); 
    if (result == NULL) {
        perror("malloc failed");
        return NULL;
    }

    struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;

    inet_ntop(AF_INET, &(addr_in->sin_addr), ip, 16);
    int port = ntohs(addr_in->sin_port);
    snprintf(result, 30, "%s.%d.txt", ip, port);

    return result;
}

int main(){
    //setting up signal handler to close socket when SIGINT received from user
    signal(SIGINT, handle_sigint); 

    int clilen;
    struct sockaddr_in cli_addr, serv_addr;
    char buf[100];
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Cannot create socket");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);

    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        perror("Unable to bind local address");
        exit(0);
    }
    listen(sockfd, 5);

    printf("Server waiting ...\n");
    int flag=1;
    while(1){
        // getting new client from queue if no client currently connected
        if(flag){   
            clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
            if(newsockfd < 0){
                perror("Accept error");
                exit(0);
            }
            flag=0;
            printf("Connected...\n");
        }

        // receiving key
        char key[27];
        int n, char_received = 0;
        while(1){
            n = recv(newsockfd, buf, 26, 0);
            for(int i = 0; i < n; i++) key[char_received++] = buf[i];
            if(char_received == 26) break;
        }
        key[26] = '\0';
        printf("Key received.\n");

        //receiving the file to encrypt from user
        char *fname = sockaddr_to_string((struct sockaddr*)&cli_addr);
        FILE *file = fopen(fname, "w");
        char_received = 0;
        while(1){
            n = recv(newsockfd, buf, 100, 0);
            for(int i = 0; i < n; i++){
                if(buf[i] == '\0'){
                    fclose(file);
                    file = NULL;
                    break;
                }
                fprintf(file, "%c", buf[i]);
            }
            if(file == NULL) break;
        }
        
        printf("File received.\n");
        char *encfname = malloc(35);
        sprintf(encfname, "%s.enc", fname);

        FILE *encfile = fopen(encfname, "w");
        file = fopen(fname, "r");

        //encrypting the file based on key
        while(1){
            char c;
            if(fscanf(file, "%c", &c) == EOF) break;
            if(c >= 'a' && c <= 'z'){
                c = key[c - 'a'] - 'A' + 'a';
            }
            else if(c >= 'A' && c <= 'Z'){
                c = key[c - 'A'];
            }
            fprintf(encfile, "%c", c);
        }
        fclose(file);
        fclose(encfile);
        printf("File encrypted.\n");

        encfile = fopen(encfname, "r");
        if(encfile == NULL){
            printf("File %s didn't open.\n", encfname);
            return 0;
        }
        int file_sent = 0;

        //sending the encrypted file to client
        while(1){
            int i = 0;
            for(i = 0; i < 100; i++){
                if(fscanf(encfile, "%c", &buf[i]) == EOF){
                    buf[i] = '\0'; 
                    file_sent = 1; 
                    break;
                }
            }
            send(newsockfd, buf, 100, 0);
            if(file_sent == 1) break;
        }
        printf("File sent.\n");

        char finish[10];
        char_received=0;
        // waiting for client to tell if it wants to send more files or terminate connection
        while(1){
            n = recv(newsockfd, buf, 100, 0);
            for(int i = 0; i < n; i++) {
                finish[char_received++] = buf[i]; 
                if(buf[i]=='\0'){ 
                    char_received=-1; 
                    break;
                }
            }
            if(char_received==-1)break;
        }
        if(!strcmp(finish, "FINISH")){
            close(newsockfd);
            flag=1;
        }
        if(flag){
            printf("Waiting for new client...\n");
        }
        else{
            printf("Another file to encrypt...\n");
        }
    }

}
