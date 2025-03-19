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
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAXSIZE 10000
#define MAXUSERS 10
#define DELIMITER "@@@EMAIL@@@"

typedef enum state{
    INI, FRE, RCP, DAT
}STATE;

int clilen;
struct sockaddr_in cli_addr, serv_addr;
char buf[MAXSIZE];
int sockfd, newsockfd;
int usercount;
char users[MAXUSERS][100];
const char *folder_name = "mailbox";

// Handler to prevent zombie processes
void sigchld_handler(int signum){
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void sig_handler_child(int signum){
    close(newsockfd);
    exit(0);
}

void sig_handler_parent(int signum){
    close(sockfd);
    exit(0);
}  

int starts_with(char *str, char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

void send_server_error(){
    sprintf(buf, "500 SERVER ERROR");
    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
        printf("Error sending reply to client\n");
        close(newsockfd);
        exit(1);
    }
}

// Function to get the current date and time as a string
void get_current_date(char *buffer, size_t size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(buffer, size, "(%02d-%02d-%04d)",
        tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
}

// Function to add an email entry to the file
int add_email(const char *sender, const char *recipient, const char *data) {
    FILE *file = fopen(recipient, "a");
    if (!file) {
        send_server_error();
        perror("Error opening file");
        return 0;
    }

    char date[25];
    get_current_date(date, sizeof(date));

    fprintf(file, "%s\nSENDER: %s\nDATE: %s\nDATA: %s", DELIMITER, sender, date, data+4);
    fclose(file);
    return 1;
}

// Function to list email entries with sender, date, and index
void list_emails(const char *recipient){
    FILE *file = fopen(recipient, "r");
    if (!file) {
        send_server_error();
        perror("Error opening file");
        return;
    }

    char sender[100], date[25], temp[MAXSIZE], line[MAXSIZE];
    int index = 0;
    int found = 0;
    strcpy(temp, "200 OK\n");

    while (fgets(buf, sizeof(buf), file)) {
        if (strncmp(buf, DELIMITER, strlen(DELIMITER)) == 0) {
            index++;
            found = 1;
        } 
        else if (strncmp(buf, "SENDER:", 7) == 0) {
            sscanf(buf, "SENDER: %s", sender);
        } 
        else if (strncmp(buf, "DATE:", 5) == 0) {
            sscanf(buf, "DATE: %s", date);
            sprintf(line, "%3d Email from %s %s\n", index, sender, date);
            strcat(temp, line);
        }
    }

    if (!found) {
        strcat(temp, "No emails found");
    }

    if (send(newsockfd, temp, strlen(temp) + 1, 0) < 0){
        printf("Error sending reply to client\n");
        close(newsockfd);
        fclose(file);
        exit(1);
    }

    fclose(file);
}

void get_email_by_index(const char *filename, int target_index) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        send_server_error();
        perror("Error opening file");
        return;
    }

    char line[2*MAXSIZE];
    char sender[100], date[25];
    int index = 0;
    int found = 0;
    int reading_data = 0;

    buf[0] = '\0';  // Initialize data buffer

    if(target_index<=0){
        printf("Invalid index: %d", target_index);
        sprintf(buf, "403 Invalid index requested");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            fclose(file);
            exit(1);
        }
    }

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, DELIMITER, strlen(DELIMITER)) == 0) {
            if (found){
                break;
            }
            index++;
            if(index==target_index)found=1;
            reading_data = 0;
            buf[0] = '\0';  // Reset data buffer for next email
        } else if (index == target_index) {
            if (strncmp(line, "SENDER:", 7) == 0) {
                sscanf(line, "SENDER: %s", sender);
            } else if (strncmp(line, "DATE:", 5) == 0) {
                sscanf(line, "DATE: %s", date);
            } else if (strncmp(line, "DATA:", 5) == 0) {
                reading_data = 1;
                snprintf(buf, sizeof(buf), "%s", line + 5);
            } else if (reading_data) {
                strcat(buf, line);
            }
        }
    }

    // Print the last email if it was found
    if (found) {
        sprintf(line, "200 OK\nSender: %s\nDate: %s\nData:\n%s\n", sender, date, buf);
    }

    if(!found){
        printf("Invalid index: %d", target_index);
        sprintf(line, "401 NOT FOUND");
    }

    if (send(newsockfd, line, strlen(line) + 1, 0) < 0){
        printf("Error sending reply to client\n");
        close(newsockfd);
        exit(1);
    }

    fclose(file);
}

int HELO(char clientid[]){
    if (sscanf(buf, "HELO %s", clientid) == 1){
        printf("HELO received from %s\n", clientid);
        sprintf(buf, "200 OK");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
        return 1;
    } 
    else{
        printf("Invalid format: %s", buf);
        sprintf(buf, "400 ERR");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
    }
    return 0;
}

int MAIL_FROM(char s_email[]){
    if (sscanf(buf, "MAIL FROM: %s", s_email) == 1){
        printf("%s\n", buf);

        sprintf(buf, "%s/%s.txt", folder_name, s_email);
        FILE *file=fopen(buf, "r");
        if(file==NULL){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return 0;
        }
        fclose(file);

        sprintf(buf, "200 OK");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
        return 1;
    } 
    else{
        printf("Invalid format: %s", buf);
        sprintf(buf, "400 ERR");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
    }
    return 0;
}

int RCPT_TO(char r_email[]){
    if (sscanf(buf, "RCPT TO: %s", r_email) == 1){
        printf("%s\n", buf);

        sprintf(buf, "%s/%s.txt", folder_name, r_email);
        FILE *file=fopen(buf, "r");
        if(file==NULL){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return 0;
        }
        fclose(file);

        sprintf(buf, "200 OK");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
        return 1;
    } 
    else{
        printf("Invalid format: %s", buf);
        sprintf(buf, "400 ERR");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
    }
    return 0;
}

int DATA(char s_email[], char r_email[]){
    char temp[100+strlen(folder_name)];
    sprintf(temp, "%s/%s.txt", folder_name, r_email);
    if(add_email(s_email, temp, buf)){
        printf("DATA received, message stored\n");
        sprintf(buf, "200 Message stored successfully");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
        return 1;
    }
    return 0;
}

void LIST(){
    char r_email[100];
    if (sscanf(buf, "LIST %s", r_email) == 1){
        printf("%s\n", buf);

        // to replace with faster code
        sprintf(buf, "%s/%s.txt", folder_name, r_email);
        FILE *file=fopen(buf, "r");
        if(file==NULL){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return;
        }
        fclose(file);

        
        char temp[100+strlen(folder_name)];
        sprintf(temp, "%s/%s.txt", folder_name, r_email);
        list_emails(temp);
        
    } 
    else{
        printf("Invalid format: %s", buf);
        sprintf(buf, "400 ERR");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
    }
}

void GET_MAIL(){
    char r_email[100];
    int index;
    if (sscanf(buf, "GET_MAIL %s %d", r_email, &index) >0){
        printf("%s\n", buf);

        // to replace with faster code
        sprintf(buf, "%s/%s.txt", folder_name, r_email);
        FILE *file=fopen(buf, "r");
        if(file==NULL){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return;
        }
        fclose(file);

        
        char temp[100+strlen(folder_name)];
        sprintf(temp, "%s/%s.txt", folder_name, r_email);
        get_email_by_index(temp, index);
        
    } 
    else{
        printf("Invalid format: %s", buf);
        sprintf(buf, "400 ERR");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
    }

    
}



void QUIT(){
    printf("Client disconnected.\n");
    sprintf(buf, "200 Goodbye");
    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
        printf("Error sending reply to client\n");
        close(newsockfd);
        exit(1);
    }
    exit(0);
}

void child_process(){
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("Client connected: %s\n", client_ip);

    STATE state=INI;
    char s_email[100], r_email[100], clientid[100];;

    while(1){
        int bytes_received = recv(newsockfd, buf, MAXSIZE, 0);
        if(bytes_received<0){
            perror("Error receiving from client\n");
            close(newsockfd);
            exit(0);
        }
        else if(bytes_received==0){
            printf("Client disconnected\n");
            close(newsockfd);
            exit(0);
        }
        if(starts_with(buf, "HELO")){
            if(HELO(clientid)){
                state=FRE;
            }
        }
        else if(starts_with(buf, "MAIL FROM:")){
            if(MAIL_FROM(s_email)){
                state=RCP;
            }
        }
        else if(starts_with(buf, "RCPT TO:")){
            if(RCPT_TO(r_email)){
                state=DAT;
            }
        }
        else if(starts_with(buf, "DATA")){
            if(DATA(s_email, r_email)){
                state=FRE;
            }
        }
        else if(starts_with(buf, "LIST")){
            LIST();
        }
        else if(starts_with(buf, "GET_MAIL")){
            GET_MAIL();
        }
        else if(starts_with(buf, "QUIT")){
            QUIT();
        }
        else{
            printf("INVALID MSG RECEIVED from client: %s", client_ip);
            sprintf(buf, "400 ERR");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
        }
    }

    exit(0);
}

int main(int argc, char *argv[]){
    

    // Create the directory
    if (mkdir(folder_name, 0777) == -1) {
        if (errno == EEXIST) {
            printf("Directory %s already exists.\n", folder_name);
        } else {
            perror("Error creating directory");
            exit(1);
        }
    } else {
        printf("Directory %s created successfully.\n", folder_name);
    }

    strcpy(users[0], "alice@example.com");
    strcpy(users[1], "bob@example.com");
    strcpy(users[2], "charlie@example.com");
    usercount=3;

    for(int i=0; i<usercount; i++){
        sprintf(buf, "%s/%s.txt", folder_name, users[i]);
        FILE *file = fopen(buf, "w");
        if (file == NULL) {
            perror("Error creating file");
            exit(1);
        }
        fclose(file);
    }


    int serverport=atoi(argv[1]);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(serverport);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Unable to bind local address");
        close(sockfd);
        sig_handler_parent(0);
        exit(0);
    }
    listen(sockfd, 5);

    printf("Listening on port %d...\n", serverport);

    signal(SIGINT, sig_handler_parent);
    signal(SIGCHLD, sigchld_handler);

    // Main server loop - accept connections and fork child processes
    while (1){
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0){
            printf("Accept error\n");
            continue;
        }
        int pid = fork();
        if (pid == 0){
            // child process
            close(sockfd);
            signal(SIGINT, sig_handler_child);
            child_process();
        }
        else{
            // parent process
            close(newsockfd);
        }
    }


}