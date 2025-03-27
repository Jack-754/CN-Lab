#include <stdio.h> // printf, puts, perror
#include <stdlib.h> // malloc
#include <string.h> // memset
#include <unistd.h> // close syscall
#include <netinet/tcp.h> // TCP header
#include <netinet/udp.h> // UDP header
#include <netinet/ip_icmp.h> // ICMP header
#include <netinet/ip.h> // IP header
#include <sys/socket.h> // Socket's APIs
#include <sys/select.h> // select
#include <sys/utsname.h> // uname
#include <sys/time.h>   // gettimeofday
#include <sys/sysinfo.h> // sysinfo
#include <arpa/inet.h> // inet_ntoa
#include <signal.h> // signal
#include <ifaddrs.h> // getifaddrs
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

void compute_my_ip() {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(1);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            if (strncmp(ifa->ifa_name, "lo", 2) != 0) {  // Ignore loopback
                my_ip = sa->sin_addr.s_addr;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);

    if (my_ip == 0) {
        fprintf(stderr, "Could not determine local IP\n");
        exit(1);
    }

    printf("My IP: %s\n", inet_ntoa(*(struct in_addr *)&my_ip));
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
        snprintf(buffer, size, "System Uptime: %ld seconds\nLoad Average: %.2f, %.2f, %.2f",
                 info.uptime,
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

    signal(SIGINT, sigint_handler);

    my_ip=inet_addr(argv[1]);

    struct sockaddr_in src;
    socklen_t len = sizeof(src);

    printf("Sending first Hello!\n");
    struct iphdr *send_ip_packet = (struct iphdr*)send_buf;
    memset(send_buf, 0, BUF_SIZE);
    send_ip_packet->version = 4;
    send_ip_packet->ihl = 5;
    send_ip_packet->ttl = 64;
    send_ip_packet->protocol = 253;
    send_ip_packet->saddr = my_ip;
    send_ip_packet->daddr = inet_addr("255.255.255.255");
    send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16);

    char *send_custom_header = send_buf + send_ip_packet->ihl*4;

    int send_msg_type = htonl(HELLO);
    int send_payload_len = htonl(0);
    int send_tran_id = htonl(0);
    int send_reserved = htonl(0);

    printf("S: Sent message type: %d\n", send_msg_type);

    memcpy(send_custom_header, &send_msg_type, 4);
    memcpy(send_custom_header+4, &send_payload_len, 4);
    memcpy(send_custom_header+8, &send_tran_id, 4);
    memcpy(send_custom_header+12, &send_reserved, 4);

    sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);
    printf("Bytes sent: %d\n", send_ip_packet->ihl*4 + 16);
    sleep(5);

    while(1){
        fd_set readfds;
        struct timeval timeout;

        // Initialize the fd_set
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Set timeout of 10 seconds
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        // Wait for data on sockfd
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select() failed");
            return 1;
        } 
        else if (activity == 0) {
            printf("Timeout! Sending Hello!\n");
            struct iphdr *send_ip_packet = (struct iphdr*)send_buf;
            memset(send_buf, 0, BUF_SIZE);
            send_ip_packet->version = 4;
            send_ip_packet->ihl = 5;
            send_ip_packet->ttl = 64;
            send_ip_packet->protocol = 253;
            send_ip_packet->saddr = my_ip;
            send_ip_packet->daddr = inet_addr("255.255.255.255");
            send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16);

            char *send_custom_header = send_buf + send_ip_packet->ihl*4;

            int send_msg_type = htonl(HELLO);
            int send_payload_len = htonl(0);
            int send_tran_id = htonl(0);
            int send_reserved = htonl(0);

            printf("S: Sent message type: %d\n", send_msg_type);

            memcpy(send_custom_header, &send_msg_type, 4);
            memcpy(send_custom_header+4, &send_payload_len, 4);
            memcpy(send_custom_header+8, &send_tran_id, 4);
            memcpy(send_custom_header+12, &send_reserved, 4);

            sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);
            sleep(5);
            continue;
        }

        // If data is available, receive it
        memset(recv_buf, 0, BUF_SIZE);
        int bytes = recvfrom(sockfd, recv_buf, BUF_SIZE, 0, (struct sockaddr *)&src, &len);
        if (bytes < 0) {
            perror("recvfrom() failed");
            return 1;
        }

        printf("Bytes received: %d\n", bytes);

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

        printf("S: Recieved message type: %d\n", recv_msg_type);

        if(recv_msg_type&HELLO){
            printf("Received HELLO message from %s\n", inet_ntoa(src.sin_addr));
        }
        else if(recv_msg_type&QUERY){
            printf("Received QUERY message from %s\n", inet_ntoa(src.sin_addr));

            int recv_query_type = recv_msg_type ^ QUERY;

            printf("Query type: %d\n", recv_query_type);

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
                printf("Sent CPULOAD response to %s\n", inet_ntoa(src.sin_addr));
                get_sysinfo(send_payload, BUF_SIZE-(send_ip_packet->ihl*4 + 16)-1);
                break;
            case SYSTIME:
                printf("Sent SYSTIME response to %s\n", inet_ntoa(src.sin_addr));
                get_time(send_payload, BUF_SIZE-(send_ip_packet->ihl*4 + 16)-1);
                break;
            case HOSTNAME:
                printf("Sent HOSTNAME response to %s\n", inet_ntoa(src.sin_addr));
                get_hostname(send_payload, BUF_SIZE-(send_ip_packet->ihl*4 + 16)-1);
                break;
            }

            int send_msg_type = htonl(RESPONSE | recv_query_type);
            int send_payload_len = htonl(strlen(send_payload));
            int send_tran_id = htonl(0);
            int send_reserved = htonl(0);

            memcpy(send_custom_header, &send_msg_type, 4);
            memcpy(send_custom_header+4, &send_payload_len, 4);
            memcpy(send_custom_header+8, &send_tran_id, 4);
            memcpy(send_custom_header+12, &send_reserved, 4);

            send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16 + ntohl(send_payload_len));
            
            printf("S: Sent message type (response): %d\n", send_msg_type);

            sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16 + ntohl(send_payload_len), 0, (struct sockaddr *)&src, len);

        }
        else{
            printf("Invalid message type %d\n", recv_msg_type);
        }

    }

    close(sockfd);
    return 0;
}