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

int main(){

    char buf[BUF_SIZE], packet[BUF_SIZE];
    sockfd=socket(AF_INET, SOCK_RAW, 253);
    if(sockfd<0){
        perror("Unable to create raw socket");
        return 1;
    }
    int one = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    signal(SIGINT, sigint_handler);

    my_ip=inet_addr("127.0.0.1");

    struct sockaddr_in src;
    socklen_t len = sizeof(src);

    printf("Sending first Hello!\n");
    struct iphdr *ip_packet = (struct iphdr*)packet;
    ip_packet->version = 4;
    ip_packet->ihl = 5;
    ip_packet->ttl = 64;
    ip_packet->protocol = 253;
    ip_packet->saddr = my_ip;
    ip_packet->daddr = inet_addr("255.255.255.255");
    ip_packet->tot_len = htons(ip_packet->ihl*4 + 16);

    char *custom_header_send = packet + ip_packet->ihl*4;

    int msg_type_send = htonl(HELLO);
    int payload_len_send = htonl(0);
    int tran_id_send = htonl(0);
    int reserved_send = htonl(0);

    printf("S: Sent message type: %d\n", msg_type_send);

    memcpy(custom_header_send, &msg_type_send, 4);
    memcpy(custom_header_send+4, &payload_len_send, 4);
    memcpy(custom_header_send+8, &tran_id_send, 4);
    memcpy(custom_header_send+12, &reserved_send, 4);

    sendto(sockfd, packet, ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);
    printf("Bytes sent: %d\n", ip_packet->ihl*4 + 16);
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
            struct iphdr *ip_packet = (struct iphdr*)packet;
            ip_packet->version = 4;
            ip_packet->ihl = 5;
            ip_packet->ttl = 64;
            ip_packet->protocol = 253;
            ip_packet->saddr = my_ip;
            ip_packet->daddr = inet_addr("255.255.255.255");
            ip_packet->tot_len = htons(ip_packet->ihl*4 + 16);

            char *custom_header_send = packet + ip_packet->ihl*4;

            int msg_type_send = htonl(HELLO);
            int payload_len_send = htonl(0);
            int tran_id_send = htonl(0);
            int reserved_send = htonl(0);

            printf("S: Sent message type: %d\n", msg_type_send);

            memcpy(custom_header_send, &msg_type_send, 4);
            memcpy(custom_header_send+4, &payload_len_send, 4);
            memcpy(custom_header_send+8, &tran_id_send, 4);
            memcpy(custom_header_send+12, &reserved_send, 4);

            sendto(sockfd, packet, ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);
            sleep(5);
            continue;
        }

        // If data is available, receive it
        int bytes = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&src, &len);
        if (bytes < 0) {
            perror("recvfrom() failed");
            return 1;
        }

        printf("Bytes received: %d\n", bytes);

        struct iphdr *ip_buf = (struct iphdr*)buf;

        if(ip_buf->protocol != 253){
            continue;
        }
        if(ip_buf->daddr != inet_addr("255.255.255.255") && ip_buf->daddr != my_ip){
            continue;
        }

        char *custom_header = buf + ip_buf->ihl*4;
        int r_msg_type, r_payload_len, r_tran_id, r_reserved;
        memcpy(&r_msg_type, custom_header, 4);
        memcpy(&r_payload_len, custom_header + 4, 4);
        memcpy(&r_tran_id, custom_header + 8, 4);
        memcpy(&r_reserved, custom_header + 12, 4);

        // Convert from network byte order
        r_msg_type = ntohl(r_msg_type);
        r_payload_len = ntohl(r_payload_len);
        r_tran_id = ntohl(r_tran_id);
        r_reserved = ntohl(r_reserved);

        printf("S: Recieved message type: %d\n", r_msg_type);

        if(r_msg_type&HELLO){
            printf("Received HELLO message from %s\n", inet_ntoa(src.sin_addr));
        }
        else if(r_msg_type&QUERY){
            printf("Received QUERY message from %s\n", inet_ntoa(src.sin_addr));

            int query_type = r_msg_type ^ QUERY;

            printf("Query type: %d\n", query_type);

            struct iphdr *ip_packet = (struct iphdr*)packet;
            memset(packet, 0, BUF_SIZE);

            ip_packet->version = 4;
            ip_packet->ihl = 5;
            ip_packet->ttl = 64;
            ip_packet->protocol = 253;
            ip_packet->saddr = my_ip;
            ip_packet->daddr = src.sin_addr.s_addr;

            char *custom_header_send = packet + ip_packet->ihl*4;
            char *payload = custom_header_send + 16;

            switch (query_type){
            case CPULOAD:
                printf("Sent CPULOAD response to %s\n", inet_ntoa(src.sin_addr));
                get_sysinfo(payload, BUF_SIZE-(ip_packet->ihl*4 + 16)-1);
                break;
            case SYSTIME:
                printf("Sent SYSTIME response to %s\n", inet_ntoa(src.sin_addr));
                get_time(payload, BUF_SIZE-(ip_packet->ihl*4 + 16)-1);
                break;
            case HOSTNAME:
                printf("Sent HOSTNAME response to %s\n", inet_ntoa(src.sin_addr));
                get_hostname(payload, BUF_SIZE-(ip_packet->ihl*4 + 16)-1);
                break;
            }

            int s_msg_type = htonl(RESPONSE | query_type);
            int s_payload_len = htonl(strlen(payload));
            int s_tran_id = htonl(0);
            int s_reserved = htonl(0);

            memcpy(custom_header_send, &s_msg_type, 4);
            memcpy(custom_header_send+4, &s_payload_len, 4);
            memcpy(custom_header_send+8, &s_tran_id, 4);
            memcpy(custom_header_send+12, &s_reserved, 4);

            ip_packet->tot_len = htons(ip_packet->ihl*4 + 16 + ntohl(s_payload_len));
            
            printf("S: Sent message type (response): %d\n", s_msg_type);

            sendto(sockfd, packet, ip_packet->ihl*4 + 16 + ntohl(s_payload_len), 0, (struct sockaddr *)&src, len);

        }
        else{
            printf("Invalid message type %d\n", r_msg_type);
        }


    }

    close(sockfd);
    return 0;
}