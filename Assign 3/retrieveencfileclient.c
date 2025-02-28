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

int main(){
    int sockfd;
    struct sockaddr_in serv_addr;
    int i;
    char buf[100];

    if((sockfd=socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("Unable to create socket\n");
        exit(0);
    }

    serv_addr.sin_family=AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port=htons(20000);

    // connecting to server
    if((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0){
        perror("Unable to connect to server\n");
        exit(0);
    }
    
    FILE * file;
    char*encfname=malloc(110);
    while(1){
        // taking file name to encrypt from user
        printf("Enter the name of file to send: ");
        scanf("%s", buf);
        file=fopen(buf, "r");
        while(file==NULL){
            printf("NOT FOUND %s\nEnter Again: ", buf);
            scanf("%s", buf);
            file=fopen(buf, "r");
        }

        sprintf(encfname, "%s.enc", buf);
        
        // taking key from user
        printf("Enter Key: ");
        scanf("%s", buf);
        while(strlen(buf)<26){
            printf("Key is short. Enter again: ");
            scanf("%s", buf);
        }

        for(int i=0; i<26; i++){
            if(buf[i]>='a' && buf[i]<='z')buf[i]=buf[i]-'a'+'A';
        }

        //sending key, only first 26 characters of the buffer contain key
        //rest are of no use
        send(sockfd, buf, 26, 0);

        // sending the file to server
        int file_sent=0;
        while(1){
            int i=0;
            for( i=0; i<100; i++){
                if(fscanf(file, "%c", &buf[i])==EOF){ buf[i]='\0'; file_sent=1; break;}
            }
            send(sockfd, buf, 100, 0);
            if(file_sent==1){
                break;
            }
        }
        printf("File sent.\n");

        // receiving the encrypted file from server
        file=fopen(encfname, "w");
        int char_received=0, n;
        while(1){
            n=recv(sockfd, buf, 100, 0);
            for(int i=0; i<n; i++){
                if(buf[i]=='\0'){
                    fclose(file);
                    file=NULL;
                    break;
                }
                fprintf(file, "%c", buf[i]);
            }
            if(file==NULL)break;
        }
        printf("Encrypted file received.\n");

        // asking user if he wants to terminate connection or not
        printf("Do you want to encrypt another file (\"No\" for no, anything else for yes): ");
        char tmp[10];
        scanf("%s", tmp);
        if(!strcmp(tmp, "No")){
            sprintf(buf, "FINISH");
            send(sockfd, buf, strlen(buf)+1, 0);
            break;
        }
        else{
            sprintf(buf, "CONTINUE");
            send(sockfd, buf, strlen(buf)+1, 0);
        }
    }

	close(sockfd);

    return 0;

}