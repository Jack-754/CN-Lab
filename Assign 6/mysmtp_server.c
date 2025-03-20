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

// Semaphore operation macros
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)
struct sembuf pop, vop;

typedef enum state{
    INI, FRE, RCP, DAT
}STATE;

typedef struct user{
    char email[100];
    int mutex;
}user;

socklen_t clilen;
struct sockaddr_in cli_addr, serv_addr;
char buf[MAXSIZE];
int sockfd, newsockfd;
int usercount;
user users[MAXUSERS];

const char *folder_name = "mailbox";

// Handler to prevent zombie processes
void sigchld_handler(int signum){
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void free_resources(){
    for(int i=0; i<usercount; i++){
        semctl(users[i].mutex, 0, IPC_RMID, NULL);
    }
}

void sig_handler_child(int signum){
    close(newsockfd);
    exit(0);
}

void sig_handler_parent(int signum){
    close(sockfd);
    free_resources();
    exit(0);
}

int find_user(char * email){
    for(int i=0; i<usercount; i++){
        if(!strcmp(users[i].email, email))return i;
    }
    return -1;
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

void safe_recv() {
    int total_received = 0;

    while (total_received < MAXSIZE - 1) {
        int bytes_received = recv(newsockfd, buf + total_received, MAXSIZE - total_received - 1, 0);
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
}

// Function to get the current date and time as a string
void get_current_date(char *buffer, size_t size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(buffer, size, "(%02d-%02d-%04d)",
        tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
}

// Function to add an email entry to the file
int add_email(const char *sender, const char *recipient, const char *data, int userid) {
    P(users[userid].mutex);
    FILE *file = fopen(recipient, "a");
    if (!file) {
        send_server_error();
        perror("Error opening file");
        V(users[userid].mutex);
        return 0;
    }

    char date[25];
    get_current_date(date, sizeof(date));

    fprintf(file, "%s\nSENDER: %s\nDATE: %s\nDATA:%s", DELIMITER, sender, date, data+4);
    fclose(file);
    V(users[userid].mutex);
    return 1;
}

// Function to list email entries with sender, date, and index
void list_emails(const char *recipient, int userid){
    P(users[userid].mutex);
    FILE *file = fopen(recipient, "r");
    if (!file) {
        send_server_error();
        perror("Error opening file");
        V(users[userid].mutex);
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
        V(users[userid].mutex);
        exit(1);
    }

    fclose(file);
    V(users[userid].mutex);
}

void get_email_by_index(const char *filename, int target_index, int userid) {
    P(users[userid].mutex);
    FILE *file = fopen(filename, "r");
    if (!file) {
        send_server_error();
        perror("Error opening file");
        V(users[userid].mutex);
        return;
    }

    char line[2*MAXSIZE];
    char sender[100], date[25];
    int index = 0;
    int found = 0;
    int reading_data = 0;

    buf[0] = '\0';  // Initialize data buffer

    if(target_index<=0){
        printf("Invalid index: %d\n", target_index);
        sprintf(buf, "403 Invalid index requested");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            fclose(file);
            V(users[userid].mutex);
            exit(1);
        }
        V(users[userid].mutex);
        return;
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
        sprintf(line, "200 OK\nSender: %s\nDate: %s\nData: %s", sender, date, buf);
    }

    if(!found){
        printf("Invalid index: %d\n", target_index);
        sprintf(line, "401 NOT FOUND");
    }

    if (send(newsockfd, line, strlen(line) + 1, 0) < 0){
        printf("Error sending reply to client\n");
        close(newsockfd);
        V(users[userid].mutex);
        exit(1);
    }

    fclose(file);
    V(users[userid].mutex);
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
        printf("Invalid format: %s\n", buf);
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

        int userid=find_user(s_email);
        if(userid<0){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return 0;
        }

        sprintf(buf, "200 OK");
        if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
            printf("Error sending reply to client\n");
            close(newsockfd);
            exit(1);
        }
        return 1;
    } 
    else{
        printf("Invalid format: %s\n", buf);
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

        int userid=find_user(r_email);
        if(userid<0){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return 0;
        }

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
    int userid=find_user(r_email);
    if(add_email(s_email, temp, buf, userid)){
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

        int userid=find_user(r_email);
        if(userid<0){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return;
        }

        
        char temp[100+strlen(folder_name)];
        sprintf(temp, "%s/%s.txt", folder_name, r_email);
        list_emails(temp, userid);
        
    } 
    else{
        printf("Invalid format: %s\n", buf);
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

        int userid=find_user(r_email);
        if(userid<0){
            sprintf(buf, "401 NOT FOUND");
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                printf("Error sending reply to client\n");
                close(newsockfd);
                exit(1);
            }
            return;
        }

        
        char temp[100+strlen(folder_name)];
        sprintf(temp, "%s/%s.txt", folder_name, r_email);
        get_email_by_index(temp, index, userid);
        
    } 
    else{
        printf("Invalid format: %s\n", buf);
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
        safe_recv();
        if(starts_with(buf, "HELO")){
            if(state!=INI){
                sprintf(buf, "403 FORBIDDEN (DUPLICATE HELO REQUEST)");
                if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                    printf("Error sending reply to client\n");
                    close(newsockfd);
                    exit(1);
                }
            }
            else if(HELO(clientid)){
                state=FRE;
            }
        }
        else if(starts_with(buf, "MAIL FROM:")){
            if(state!=FRE){
                if(state==INI){
                    sprintf(buf, "403 FORBIDDEN (COMMUNICATION NOT INITIATED)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==RCP){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"RCPT TO:\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==DAT){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"DATA\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
            }
            else if(MAIL_FROM(s_email)){
                state=RCP;
            }
        }
        else if(starts_with(buf, "RCPT TO:")){
            if(state!=RCP){
                if(state==INI){
                    sprintf(buf, "403 FORBIDDEN (COMMUNICATION NOT INITIATED)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==FRE){
                    sprintf(buf, "403 FORBIDDEN (SEND \"MAIL TO:\" BEFORE \"RCPT TO:\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==DAT){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"DATA\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
            }
            else if(RCPT_TO(r_email)){
                state=DAT;
            }
        }
        else if(starts_with(buf, "DATA")){
            if(state!=DAT){
                if(state==INI){
                    sprintf(buf, "403 FORBIDDEN (COMMUNICATION NOT INITIATED)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==FRE || state==RCP){
                    sprintf(buf, "403 FORBIDDEN (SEND \"RCPT TP:\" BEFORE \"DATA\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
            }
            else if(DATA(s_email, r_email)){
                state=FRE;
            }
        }
        else if(starts_with(buf, "LIST")){
            if(state!=FRE){
                if(state==INI){
                    sprintf(buf, "403 FORBIDDEN (COMMUNICATION NOT INITIATED)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==RCP){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"RCPT TO:\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==DAT){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"DATA\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
            }
            else LIST();
        }
        else if(starts_with(buf, "GET_MAIL")){
            if(state!=FRE){
                if(state==INI){
                    sprintf(buf, "403 FORBIDDEN (COMMUNICATION NOT INITIATED)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==RCP){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"RCPT TO:\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
                else if(state==DAT){
                    sprintf(buf, "403 FORBIDDEN (WAITING FOR \"DATA\" REQUEST)");
                    if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                        printf("Error sending reply to client\n");
                        close(newsockfd);
                        exit(1);
                    }
                }
            }
            else GET_MAIL();
        }
        else if(starts_with(buf, "QUIT")){
            QUIT();
        }
        else{
            printf("INVALID MSG RECEIVED from client: %s\n", client_ip);
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
    if(argc<2){
        printf("Run as ./server $(server port)");
        exit(1);
    }

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

    strcpy(users[0].email, "alice@example.com");
    strcpy(users[1].email, "bob@example.com");
    strcpy(users[2].email, "charlie@example.com");
    usercount=3;

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    for(int i=0; i<usercount; i++){
        int key=ftok("makefile", i+1);
        if(key==-1){
            perror("Error in creating key\n");
            exit(1);
        }
        users[i].mutex=semget(key, 1, 0666 | IPC_CREAT);
        if(users[i].mutex==-1){
            perror("Error in creating mutex\n");
            exit(1);
        }
        if(semctl(users[i].mutex, 0, SETVAL, 1) == -1){
            perror("semctl failed in server.c\n");
            exit(1);
        }

        sprintf(buf, "%s/%s.txt", folder_name, users[i].email);
        FILE *file = fopen(buf, "a");
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
        free_resources();
        exit(1);
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