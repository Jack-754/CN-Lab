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
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_TASKS 100
#define TIMEOUT 30

int sockfd, newsockfd;
int all_tasks_fetched = 0;

// Structure to store task information
typedef struct tasks
{
    int client_pid;  // Process ID of client handling this task
    int num1;        // First operand
    int num2;        // Second operand
    char op;         // Operation to perform
    int done;        // Flag indicating if task is completed
    int result;      // Result of the operation
} tasks;

// Semaphore operation macros
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)
int mutex, shmid, task_count = 0;
struct sembuf pop, vop;
tasks *task;

// Handler for SIGINT signal in parent process
void sig_handler_parent(int signum)
{
    shmdt(task);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(mutex, 0, IPC_RMID, NULL);
    close(sockfd);
    exit(0);
}

// Handler for SIGINT signal in child process
void sig_handler_child(int signum)
{
    shmdt(task);
    close(newsockfd);
    exit(0);
}

// Handler to prevent zombie processes
void sigchld_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Find available task slot in shared memory
int find_task()
{
    for (int i = 0; i < task_count; i++)
    {
        if (task[i].client_pid == -1)
        {
            return i;
        }
    }
    return -1;
}

// Clean up when client disconnects
void handle_client_closure(int task_id)
{
    if (task_id != -1)
    {
        P(mutex);
        task[task_id].client_pid = -1;
        V(mutex);
    }
}

// Handle client communication and task processing
void child_process(){
    // Initialize buffers for message handling
    char buf[100], recv_buf[100];
    close(sockfd);  // Close parent's socket descriptor
    int task_id = -1;  // No task assigned initially
    int curpid = getpid();
    
    // Set socket to non-blocking mode
    int flags = fcntl(newsockfd, F_GETFL, 0);
    fcntl(newsockfd, F_SETFL, flags | O_NONBLOCK);
    int read_bytes = 0, recv_bytes = 0;
    int last_activity = time(NULL);
    
    while (1){
        // Check if client has timed out
        if(time(NULL) - last_activity > TIMEOUT){
            handle_client_closure(task_id);
            printf("Client timed out\n");
            close(newsockfd);
            exit(0);
        }

        // Check if we need to receive new data
        if(read_bytes==recv_bytes){
            // Attempt to receive data from client
            read_bytes = recv_bytes = recv(newsockfd, recv_buf, 100, 0);
            if (recv_bytes < 0){
                // Handle non-blocking socket timeout
                if (errno == EWOULDBLOCK){
                    printf("No message received, continuing...\n");
                    sleep(1);
                    continue;
                }
                else{
                    // Handle other receive errors
                    handle_client_closure(task_id);
                    perror("recv error");
                    close(newsockfd);
                    exit(1);
                }
            }
            else if (recv_bytes == 0){
                // Client has closed connection
                handle_client_closure(task_id);
                printf("Client disconnected\n");
                close(newsockfd);
                exit(0);
            }

            last_activity = time(NULL);
            
            // Print received message
            printf("Received (recv_bytes: %d) : ", recv_bytes);
            for(int i=0; i<recv_bytes; i++){
                printf("%c", recv_buf[i]);
            }
            printf("\n");
            read_bytes = 0;
        }

        // Copy received message to processing buffer
        int i = 0;
        do{
            buf[i++] = recv_buf[read_bytes];
        }while(recv_buf[read_bytes++] != '\0');
        printf("Processing message: %s\n", buf);

        // Handle GET_TASK request
        if (strcmp(buf, "GET_TASK") == 0){
            // Check if client already has a task
            if (task_id != -1)
            {
                printf("Task already assigned to the client\n");
                continue;
            }
            P(mutex);  // Enter critical section
            task_id = find_task(task, task_count);
            if (task_id == -1){
                // No tasks available
                strcpy(buf, "No tasks  available");
                send(newsockfd, buf, strlen(buf) + 1, 0);
                close(newsockfd);
                V(mutex);
                exit(0);
            }
            // Assign task to client
            task[task_id].client_pid = curpid;
            sprintf(buf, "Task: %d %c %d", task[task_id].num1, task[task_id].op, task[task_id].num2);
            V(mutex);  // Exit critical section
            
            // Send task to client
            if (send(newsockfd, buf, strlen(buf) + 1, 0) < 0){
                handle_client_closure(task_id);
                printf("Error sending task to client\n");
                close(newsockfd);
                exit(1);
            }
            last_activity = time(NULL);
        }
        // Handle RESULT message
        else if (strncmp(buf, "RESULT", 6) == 0){
            int result;
            if (sscanf(buf, "RESULT %d", &result) == 1)
            {
                P(mutex);  // Enter critical section
                // Store result and mark task as complete
                task[task_id].result = result;
                task[task_id].done = 1;
                printf("Task completed by client %d\nTask : %d %c %d\nResult : %d\n", 
                       curpid, task[task_id].num1, task[task_id].op, task[task_id].num2, result);
                task_id = -1;
                fflush(stdout);

                // If not all tasks are fetched, read next task from config file
                if(!all_tasks_fetched){
                    char input_file_name[] = "config";
                    FILE *fp = fopen(input_file_name, "r");
                    if (fp == NULL)
                    {
                        printf("Error opening input file\n");
                        exit(1);
                    }
                    // Read and store new task
                    int num1, num2;
                    char op;
                    int chars_read = fscanf(fp, "%d %c %d", &num1, &op, &num2);
                    if (chars_read < 3)
                    {
                        break;
                    }
                    task[task_count].client_pid = -1;
                    task[task_count].num1 = num1;
                    task[task_count].num2 = num2;
                    task[task_count].op = op;
                    task[task_count].done = 0;
                    task_count++;

                    if (feof(fp))
                    {
                        all_tasks_fetched = 1;
                    }
                    fclose(fp);
                }
                V(mutex);  // Exit critical section
            }
            else
            {
                // Invalid result format
                printf("Failed to extract number.\n");
                handle_client_closure(task_id);
                close(newsockfd);
                exit(1);
            }
        }
        // Handle exit request
        else if (strcmp(buf, "exit") == 0){
            handle_client_closure(task_id);
            close(newsockfd);
            exit(0);
        }
        // Handle invalid messages
        else{
            printf("Invalid message from client: (bytes received: %d) %s\n", recv_bytes, buf);
            handle_client_closure(task_id);
            close(newsockfd);
            exit(1);
        }   
    }
}

int main()
{
    // semaphore and shared memory setup ----------------------------------------------
    int key_mutex = ftok("makefile", 1);
    int key_shmid = ftok("makefile", 2);

    if (key_mutex == -1 || key_shmid == -1)
    {
        perror("ftok failed in server.c\n");
        exit(1);
    }

    // Create and initialize semaphore for mutual exclusion
    mutex = semget(key_mutex, 1, 0666 | IPC_CREAT);
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    if (mutex == -1)
    {
        perror("semget failed in server.c\n");
        exit(1);
    }

    if (semctl(mutex, 0, SETVAL, 1) == -1)
    {
        perror("semctl failed in server.c\n");
        exit(1);
    }

    // Create shared memory segment for tasks
    shmid = shmget(key_shmid, sizeof(tasks) * MAX_TASKS, 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget failed in server.c\n");
        exit(1);
    }

    task = (tasks *)shmat(shmid, NULL, 0);
    if (task == (tasks *)-1)
    {
        perror("shmat failed in server.c\n");
        exit(1);
    }
    //----------------------------------------------------------------------------------

    // socket setup --------------------------------------------------------------------
    int clilen;
    struct sockaddr_in cli_addr, serv_addr;
    char buf[100];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Unable to bind local address");
        exit(0);
    }
    listen(sockfd, 5);

    printf("Server listening ...\n");
    //----------------------------------------------------------------------------------

    signal(SIGINT, sig_handler_parent);
    signal(SIGCHLD, sigchld_handler);

    // Read initial tasks from config file
    char input_file_name[] = "config";
    FILE *fp = fopen(input_file_name, "r");
    if (fp == NULL)
    {
        printf("Error opening input file\n");
        exit(1);
    }

    P(mutex);
    while (1)
    {

        int num1, num2;
        char op;
        int chars_read = fscanf(fp, "%d %c %d", &num1, &op, &num2);
        if (chars_read < 3)
        {
            break;
        }
        task[task_count].client_pid = -1;
        task[task_count].num1 = num1;
        task[task_count].num2 = num2;
        task[task_count].op = op;
        task[task_count].done = 0;
        task_count++;

        if (feof(fp))
        {
            all_tasks_fetched = 1;
            break;
        }
    }
    fclose(fp);
    V(mutex);
    //----------------------------------------------------------------------------------

    // Set socket to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    //----------------------------------------------------------------------------------

    // Main server loop - accept connections and fork child processes
    while (1)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd == -1 && errno == EWOULDBLOCK)
        {
            printf("No connection request\n");
            sleep(2);
        }
        else if (newsockfd < 0)
        {
            printf("Accept error\n");
        }
        else
        {
            int pid = fork();
            if (pid == 0)
            {
                // child process
                signal(SIGINT, sig_handler_child);
                child_process();
            }
            else
            {
                // parent process
                close(newsockfd);
            }
        }
    }
    //----------------------------------------------------------------------------------

    return 0;
}
