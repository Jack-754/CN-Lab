#include <stdio.h> // printf, puts, perror
#include <stdlib.h> // malloc
#include <string.h> // memset
#include <unistd.h> // close syscall
#include <netinet/ip.h> // IP header
#include <sys/socket.h> // Socket's APIs
#include <arpa/inet.h> // inet_ntoa
#include <signal.h> // signal
#include <time.h>
#include <ifaddrs.h> // getifaddrs

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

int main(){
    srand(time(NULL));

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

    while(1){
        int bytes = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&src, &len);
        if(bytes<0){
            perror("Unable to retrieve data from socket");
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
        int msg_type, payload_len, tran_id, reserved;
        memcpy(&msg_type, custom_header, 4);
        memcpy(&payload_len, custom_header + 4, 4);
        memcpy(&tran_id, custom_header + 8, 4);
        memcpy(&reserved, custom_header + 12, 4);

        printf("C: Received message type: %d\n", msg_type);

        msg_type = ntohl(msg_type);
        payload_len = ntohl(payload_len);
        tran_id = ntohl(tran_id);
        reserved = ntohl(reserved);


        if(msg_type&HELLO){
            printf("Received HELLO message from %s of type %d\n", inet_ntoa(src.sin_addr), msg_type);

            struct iphdr *ip_packet = (struct iphdr*)packet;
            memset(packet, 0, BUF_SIZE);
            ip_packet->version = 4;
            ip_packet->ihl = 5;
            ip_packet->ttl = 64;
            ip_packet->protocol = 253;
            ip_packet->saddr = my_ip;
            ip_packet->daddr = src.sin_addr.s_addr;
            ip_packet->tot_len = htons(ip_packet->ihl*4 + 16);

            char *custom_header_send = packet + ip_packet->ihl*4;

            int query_avail[] = {CPULOAD, SYSTIME, HOSTNAME};
            int random_query = query_avail[rand()%3];

            int msg_type_send = htonl(QUERY | random_query);
            int payload_len_send = htonl(0);
            int tran_id_send = htonl(0);
            int reserved_send = htonl(0);

            memcpy(custom_header_send, &msg_type_send, 4);
            memcpy(custom_header_send+4, &payload_len_send, 4);
            memcpy(custom_header_send+8, &tran_id_send, 4);
            memcpy(custom_header_send+12, &reserved_send, 4);

            printf("C: Sent message type: %d\n", msg_type_send);

            sendto(sockfd, packet, ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);

            switch (random_query){
            case CPULOAD:
                printf("Sent CPULOAD query to %s\n", inet_ntoa(src.sin_addr));
                break;
            case SYSTIME:
                printf("Sent SYSTIME query to %s\n", inet_ntoa(src.sin_addr));
                break;
            case HOSTNAME:
                printf("Sent HOSTNAME query to %s\n", inet_ntoa(src.sin_addr));
                break;
            default:
                printf("Invalid query %d \n", random_query);
                break;
            }
            sleep(5);
        }
        else if(msg_type&RESPONSE){
            char *payload = custom_header + 16;
            printf("Received RESPONSE message from %s\n", inet_ntoa(src.sin_addr));
            if(msg_type & CPULOAD) printf("CPULOAD: %s\n", payload);
            else if(msg_type & SYSTIME) printf("SYSTIME: %s\n", payload);
            else if(msg_type & HOSTNAME) printf("HOSTNAME: %s\n", payload);
        }
        else{
            printf("Invalid message type %d\n", msg_type);
        }

    }
    close(sockfd);
    return 0;
}