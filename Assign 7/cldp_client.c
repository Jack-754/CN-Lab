#include <stdio.h> // printf, puts, perror
#include <stdlib.h> // malloc
#include <string.h> // memset
#include <unistd.h> // close syscall
#include <netinet/ip.h> // IP header
#include <sys/socket.h> // Socket's APIs
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

int main(int argc, char *argv[]){

    if(argc<2){
        printf("Enter: ip address for this device to use in simulation\n");
        return 1;
    }

    srand(time(NULL));

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

    while(1){
        memset(recv_buf, 0, BUF_SIZE);
        int bytes = recvfrom(sockfd, recv_buf, BUF_SIZE, 0, (struct sockaddr *)&src, &len);
        if(bytes<0){
            perror("Unable to retrieve data from socket");
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

        recv_msg_type = ntohl(recv_msg_type);
        recv_payload_len = ntohl(recv_payload_len);
        recv_tran_id = ntohl(recv_tran_id);
        recv_reserved = ntohl(recv_reserved);

        if(recv_msg_type&HELLO){
            printf("Received HELLO message from %s (Transaction id: %d)\n", inet_ntoa(src.sin_addr), recv_tran_id);

            struct iphdr *send_ip_packet = (struct iphdr*)send_buf;
            memset(send_buf, 0, BUF_SIZE);
            send_ip_packet->version = 4;
            send_ip_packet->ihl = 5;
            send_ip_packet->ttl = 64;
            send_ip_packet->protocol = 253;
            send_ip_packet->saddr = my_ip;
            send_ip_packet->daddr = src.sin_addr.s_addr;
            send_ip_packet->tot_len = htons(send_ip_packet->ihl*4 + 16);

            char *send_custom_header = send_buf + send_ip_packet->ihl*4;

            int send_query_avail[] = {CPULOAD, SYSTIME, HOSTNAME};
            int send_random_query = send_query_avail[rand()%3];

            int send_msg_type = htonl(QUERY | send_random_query);
            int send_payload_len = htonl(0);
            int send_tran_id = htonl(recv_tran_id);
            int send_reserved = htonl(0);

            memcpy(send_custom_header, &send_msg_type, 4);
            memcpy(send_custom_header+4, &send_payload_len, 4);
            memcpy(send_custom_header+8, &send_tran_id, 4);
            memcpy(send_custom_header+12, &send_reserved, 4);

            sendto(sockfd, send_buf, send_ip_packet->ihl*4 + 16, 0, (struct sockaddr *)&src, len);

            switch (send_random_query){
            case CPULOAD:
                printf("Sent CPULOAD query to %s (Transaction id: %d)\n", inet_ntoa(src.sin_addr), recv_tran_id);
                break;
            case SYSTIME:
                printf("Sent SYSTIME query to %s (Transaction id: %d)\n", inet_ntoa(src.sin_addr), recv_tran_id);
                break;
            case HOSTNAME:
                printf("Sent HOSTNAME query to %s (Transaction id: %d)\n", inet_ntoa(src.sin_addr), recv_tran_id);
                break;
            default:
                printf("Invalid query %d \n", send_random_query);
                break;
            }
        }
        else if(recv_msg_type&RESPONSE){
            char *recv_payload = recv_custom_header + 16;
            printf("Received RESPONSE message from %s (Transaction id: %d)\n", inet_ntoa(src.sin_addr), recv_tran_id);
            if (recv_msg_type & CPULOAD) 
                printf("CPULOAD: %.*s\n", recv_payload_len, recv_payload);
            else if (recv_msg_type & SYSTIME) 
                printf("SYSTIME: %.*s\n", recv_payload_len, recv_payload);
            else if (recv_msg_type & HOSTNAME) 
                printf("HOSTNAME: %.*s\n", recv_payload_len, recv_payload);
        }
        else{
            printf("Invalid message type %d\n", recv_msg_type);
        }

    }
    close(sockfd);
    return 0;
}