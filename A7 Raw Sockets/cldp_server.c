// =====================================
// Assignment 7 Submission
// Name: More Aayush Babasaheb
// Roll number: 22CS30063
// =====================================

#include <stdio.h> // printf, puts, perror
#include <stdlib.h> // malloc
#include <string.h> // memset
#include <unistd.h> // close syscall
#include <netinet/ip.h> // IP header
#include <sys/socket.h> // Socket's APIs
#include <sys/select.h> // select
#include <sys/utsname.h> // uname
#include <sys/time.h>   // gettimeofday
#include <sys/sysinfo.h> // sysinfo
#include <arpa/inet.h> // inet_ntoa
#include <signal.h> // signal
#include <time.h>

#define BUF_SIZE 65536

#define HELLO 1         // 1<<0
#define QUERY 2         // 1<<1
#define RESPONSE 4      // 1<<2
#define CPULOAD 8       // 1<<3
#define SYSTIME 16      // 1<<4
#define HOSTNAME 32     // 1<<5

int sockfd;
uint32_t my_ip = 0;

void sigint_handler() {
    close(sockfd);
    exit(0);
}


void get_hostname(char *buffer, size_t size) {
    if(size>BUF_SIZE){
        printf("Invalid buffer size\n");
        exit(0);
    }
    if (gethostname(buffer, size) != 0) {
        perror("gethostname");
        snprintf(buffer, size, "Unknown");
    }
}

void get_sysinfo(char *buffer, size_t size) {
    if(size>BUF_SIZE){
        printf("Invalid buffer size\n");
        exit(0);
    }
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        snprintf(buffer, size, "Load Average: %.2f, %.2f, %.2f",
                 info.loads[0] / 65536.0,
                 info.loads[1] / 65536.0,
                 info.loads[2] / 65536.0);
    } else {
        perror("sysinfo");
        snprintf(buffer, size, "Failed to retrieve system info");
    }
}

void get_time(char *buffer, size_t size) {
    if(size>BUF_SIZE){
        printf("Invalid buffer size\n");
        exit(0);
    }
    struct timeval tv;
    struct tm *tm_info;

    if (gettimeofday(&tv, NULL) == 0) {
        tm_info = localtime(&tv.tv_sec);
        strftime(buffer, size, "%H:%M:%S", tm_info);
    } else {
        perror("gettimeofday");
        snprintf(buffer, size, "Unknown Time");
    }
}

int main(int argc, char *argv[]){

    if(argc<2){
        printf("Enter: ip address for this device to use in simulation\n");
        return 1;
    }

    char recv_buf[BUF_SIZE], send_buf[BUF_SIZE];
    sockfd=socket(AF_INET, SOCK_RAW, 253);
    if(sockfd<0){
        perror("Unable to create raw socket");
        return 1;
    }
    int one = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Failed to set broadcast option");
        return 1;
    }
    signal(SIGINT, sigint_handler);

    my_ip=inet_addr(argv[1]);
    int cur_transaction_id=0;

    struct sockaddr_in src;
    socklen_t len = sizeof(src);

    printf("Sending first Hello! (Transaction id: %d)\n", cur_transaction_id);
    struct iphdr *send_ip_packet = (struct iphdr*)send_buf;
    memset(send_buf, 0, BUF_SIZE);
    send_ip_packet->version = 4;
    send_ip_packet->ihl = 5;
    send_ip_packet->ttl = 64;
    send_ip_packet->protocol = 253;
    send_ip_packet->saddr = my_ip;
    send_ip_packet->daddr = inet_addr("255.255.255.255");
    send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16);

    src.sin_family = AF_INET;
    src.sin_port = 0;
    src.sin_addr.s_addr = inet_addr("255.255.255.255");
    len = sizeof(src);

    char *send_custom_header = send_buf + send_ip_packet->ihl*4;

    int send_msg_type = htonl(HELLO);
    int send_payload_len = htonl(0);
    int send_tran_id = htonl(cur_transaction_id);
    int send_reserved = htonl(0);

    memcpy(send_custom_header, &send_msg_type, 4);
    memcpy(send_custom_header+4, &send_payload_len, 4);
    memcpy(send_custom_header+8, &send_tran_id, 4);
    memcpy(send_custom_header+12, &send_reserved, 4);

    sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);

    // Set timeout of 10 seconds

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    while(1){
        fd_set readfds;

        // Initialize the fd_set
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Wait for data on sockfd
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select() failed");
            return 1;
        } 
        else if (activity == 0) {
            // Reset on timeout
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
            cur_transaction_id++;

            printf("Timeout! Sending Hello! (Transaction id: %d)\n", cur_transaction_id);
            struct iphdr *send_ip_packet = (struct iphdr*)send_buf;
            memset(send_buf, 0, BUF_SIZE);
            send_ip_packet->version = 4;
            send_ip_packet->ihl = 5;
            send_ip_packet->ttl = 64;
            send_ip_packet->protocol = 253;
            send_ip_packet->saddr = my_ip;
            send_ip_packet->daddr = inet_addr("255.255.255.255");
            send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16);

            src.sin_family = AF_INET;
            src.sin_port = 0;
            src.sin_addr.s_addr = inet_addr("255.255.255.255");
            len = sizeof(src);

            char *send_custom_header = send_buf + send_ip_packet->ihl*4;

            int send_msg_type = htonl(HELLO);
            int send_payload_len = htonl(0);
            int send_tran_id = htonl(cur_transaction_id);
            int send_reserved = htonl(0);

            memcpy(send_custom_header, &send_msg_type, 4);
            memcpy(send_custom_header+4, &send_payload_len, 4);
            memcpy(send_custom_header+8, &send_tran_id, 4);
            memcpy(send_custom_header+12, &send_reserved, 4);

            sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);
            continue;
        }

        // If data is available, receive it
        memset(recv_buf, 0, BUF_SIZE);
        int bytes = recvfrom(sockfd, recv_buf, BUF_SIZE, 0, (struct sockaddr *)&src, &len);
        if (bytes < 0) {
            perror("recvfrom() failed");
            return 1;
        }

        struct iphdr *recv_ip_buf = (struct iphdr*)recv_buf;

        if(recv_ip_buf->protocol != 253){
            continue;
        }
        if(src.sin_addr.s_addr == my_ip){
            continue;
        }
        if(recv_ip_buf->daddr != inet_addr("255.255.255.255") && recv_ip_buf->daddr != my_ip){
            continue;
        }

        char *recv_custom_header = recv_buf + recv_ip_buf->ihl*4;
        int recv_msg_type, recv_payload_len, recv_tran_id, recv_reserved;
        memcpy(&recv_msg_type, recv_custom_header, 4);
        memcpy(&recv_payload_len, recv_custom_header + 4, 4);
        memcpy(&recv_tran_id, recv_custom_header + 8, 4);
        memcpy(&recv_reserved, recv_custom_header + 12, 4);

        // Convert from network byte order
        recv_msg_type = ntohl(recv_msg_type);
        recv_payload_len = ntohl(recv_payload_len);
        recv_tran_id = ntohl(recv_tran_id);
        recv_reserved = ntohl(recv_reserved);

        if(recv_msg_type&HELLO){
            printf("Received HELLO message from %s (Transaction id: %d)\n", inet_ntoa(src.sin_addr), recv_tran_id);
        }
        else if(recv_msg_type&QUERY){
            printf("Received QUERY message from %s (Transaction id: %d) for", inet_ntoa(src.sin_addr), recv_tran_id);

            int recv_query_type = recv_msg_type ^ QUERY;

            struct iphdr *send_ip_packet = (struct iphdr*)send_buf;
            memset(send_buf, 0, BUF_SIZE);

            send_ip_packet->version = 4;
            send_ip_packet->ihl = 5;
            send_ip_packet->ttl = 64;
            send_ip_packet->protocol = 253;
            send_ip_packet->saddr = my_ip;
            send_ip_packet->daddr = src.sin_addr.s_addr;

            char *send_custom_header = send_buf + send_ip_packet->ihl*4;
            char *send_payload = send_custom_header + 16;

            switch (recv_query_type){
            case CPULOAD:
                printf(" CPULOAD\n");
                printf("Sent CPULOAD in response to %s\n", inet_ntoa(src.sin_addr));
                get_sysinfo(send_payload, BUF_SIZE-(send_ip_packet->ihl*4 + 16)-1);
                break;
            case SYSTIME:
                printf(" SYSTIME\n");
                printf("Sent SYSTIME in response to %s\n", inet_ntoa(src.sin_addr));
                get_time(send_payload, BUF_SIZE-(send_ip_packet->ihl*4 + 16)-1);
                break;
            case HOSTNAME:
                printf(" HOSTNAME\n");
                printf("Sent HOSTNAME in response to %s\n", inet_ntoa(src.sin_addr));
                get_hostname(send_payload, BUF_SIZE-(send_ip_packet->ihl*4 + 16)-1);
                break;
            }

            int send_msg_type = htonl(RESPONSE | recv_query_type);
            int send_payload_len = htonl(strlen(send_payload));
            int send_tran_id = htonl(recv_tran_id);
            int send_reserved = htonl(0);

            memcpy(send_custom_header, &send_msg_type, 4);
            memcpy(send_custom_header+4, &send_payload_len, 4);
            memcpy(send_custom_header+8, &send_tran_id, 4);
            memcpy(send_custom_header+12, &send_reserved, 4);

            send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16 + ntohl(send_payload_len));

            sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16 + ntohl(send_payload_len), 0, (struct sockaddr *)&src, len);

        }
        else{
            printf("Invalid message type %d\n", recv_msg_type);
        }

    }

    close(sockfd);
    return 0;
}