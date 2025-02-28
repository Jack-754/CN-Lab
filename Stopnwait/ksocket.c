/*
NAME: MORE AAYUSH BABASAHEB
ROLL NO.: 22CS30063
ASSIGNMENT: 4
*/

#include "ksocket.h"

// Create a kernel socket with the specified domain, type, and protocol.
int k_socket(int domain, int type, int protocol){
    // Verify that the socket type is SOCK_KTP.
    if(type != SOCK_KTP){
        errno = EINVAL;
        perror("Wrong socket type.");
        exit(1);
    }
    // Create and return a socket using SOCK_DGRAM for the underlying transport.
    int sockfd = socket(domain, SOCK_DGRAM, protocol);
    return sockfd;
}

// Bind the socket to the source address and set the destination address for future use.
int k_bind(int sockfd, struct sockaddr_in* src, struct sockaddr_in* dest){
    // Bind the socket to the provided source address.
    if(bind(sockfd, (struct sockaddr*)src, sizeof(*src))){
        perror("Binding Error\n");
        exit(1);
    }
    // Save the destination address globally for use during send operations.
    destination = dest;
    return 1;
}

// Send data reliably by constructing a packet, handling retransmissions, and waiting for an acknowledgment.
int k_sendto(int sockfd, int* seq, char* buf, int len, struct sockaddr_in* dest){
    // Ensure that the message length does not exceed the maximum allowed size.
    if(len > MSG_SIZE){
        perror("Max message size that can be sent is 512 bytes. Current argument exceeds the limit.");
        exit(1);
    }
    
    // Buffer to hold the constructed packet.
    char message[PKT_SIZE];

    // Construct the packet with PSH mode, current sequence number, and the provided payload.
    make_pkt(PSH, curseq, message, sizeof(message), buf, len);
    printf("Packet %d constructed and sent\n", curseq);

    int activity;              // Variable to store the result of select.
    char recvbuffer[PKT_SIZE]; // Buffer to receive the acknowledgment packet.

    fd_set readfds;            // File descriptor set for select.
    struct timeval timeout;    // Timeout structure for select.
    int moveon = 0;            // Flag to indicate successful acknowledgment reception.
    int bytesReceived;         // Number of bytes received in the acknowledgment.
    int timeout_counter = 0;   // Counter for the number of retransmission attempts.

    // Continue sending until a valid acknowledgment is received or maximum retries are reached.
    do {
        // Send the constructed packet to the destination.
        int count = sendto(sockfd, message, sizeof(message), 0, (struct sockaddr*)dest, sizeof(*dest));
        if(count < 0){
            perror("Send error\n");
            exit(1);
        }
        
        // Initialize the file descriptor set.
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        // Set the select timeout interval.
        timeout.tv_sec = T;
        timeout.tv_usec = 0;

        // Wait for an acknowledgment with the specified timeout.
        activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        // If no data is received before timeout...
        if(activity == 0){
            timeout_counter++;
            // If maximum retries have been reached, return 0.
            if(timeout_counter == MAX_RETRIES){
                return 0;
            }
            printf("Timeout for packet %d. Retrying...\n", curseq);
            continue;
        }
        else {
            // Receive the acknowledgment packet.
            bytesReceived = recvfrom(sockfd, recvbuffer, PKT_SIZE, 0, NULL, NULL);
            if(bytesReceived < 0){
                perror("Receive error\n");
                exit(1);
            }
            int seq_rec;      // Variable to hold the received sequence number.
            int mode_rec;     // Variable to hold the received mode.
            char buf_rec[MSG_SIZE]; // Buffer to hold any payload from the acknowledgment.

            // Extract the mode, sequence number, and payload from the received packet.
            deconstruct_pkt(&mode_rec, &seq_rec, buf_rec, MSG_SIZE, recvbuffer, sizeof(recvbuffer));

            // Simulate an acknowledgment drop based on probability.
            if(DropMessage() == 1){
                printf("Ack dropped for packet %d\n", seq_rec);
                continue;
            }
            // If a valid ACK is received with a proper sequence number...
            if(mode_rec == ACK && seq_rec >= curseq){
                moveon = 1;            // Set the flag to exit the loop.
                curseq = seq_rec + 1;  // Update the current sequence number.
            }
        }
    } while(moveon == 0);

    // Update the sequence number provided by the caller.
    (*seq) = curseq - 1;
    return len;
}

// Receive data from the socket, send an acknowledgment for valid packets, and copy the payload.
int k_recvfrom(int sockfd, int* seq, char* buf, int len){
    char message[PKT_SIZE];    // Buffer to hold the received packet.
    int moveon = 0;            // Flag to indicate successful reception.
    int bytesReceived;         // Number of bytes received.
    char recvbuffer[MSG_SIZE]; // Buffer to hold the extracted payload.

    // Continue receiving until a valid packet is processed.
    while(1){
        // Receive a packet from the socket.
        bytesReceived = recvfrom(sockfd, message, PKT_SIZE, 0, NULL, NULL);
        if(bytesReceived < 0){
            perror("Recieve error\n");
            exit(1);
        }
        int mode;  // Variable to hold the mode from the packet.
        
        // Extract the mode, sequence number, and payload from the packet.
        deconstruct_pkt(&mode, seq, recvbuffer, MSG_SIZE, message, sizeof(message));

        printf("Received message with sequence number: %d\n", (*seq));

        // Simulate message drop based on a predefined probability.
        if(DropMessage()){
            printf("Packet %d dropped\n", (*seq));
            continue;
        }
        
        // If the packet sequence number is valid...
        if((*seq) <= curseq){
            char ackbuffer[512+1+8]; // Buffer to hold the ACK packet.
            char buf_ack[512] = "";  // Empty payload for the ACK packet.
            // Construct an ACK for the previous sequence number.
            make_pkt(ACK, curseq - 1, ackbuffer, sizeof(ackbuffer), buf_ack, sizeof(buf_ack));
            if((*seq) == curseq){
                // Construct an ACK for the current sequence number.
                make_pkt(ACK, curseq, ackbuffer, sizeof(ackbuffer), buf_ack, sizeof(buf_ack)); 
                curseq++;         // Update the current sequence number.
                moveon = 1;       // Mark that a valid packet has been received.
                strcpy(buf, recvbuffer); // Copy the payload into the provided buffer.
            }
            // Send the ACK packet back to the source.
            sendto(sockfd, ackbuffer, sizeof(ackbuffer), 0, (struct sockaddr*)destination, sizeof(*destination));
        }
        if(moveon){
            break;
        }
    }
    // Return the length of the received payload.
    return strlen(buf);
}

// Close the socket.
int k_close(int sockfd){
    close(sockfd);
    return -1;
}

// Construct a packet with a specified mode, sequence number, and payload.
void make_pkt(int mode, int seq, char* message, int msg_size, char* buf, int len){
    char binary_seq[9];    // Buffer to hold the binary representation of the sequence number.
    // Convert the sequence number to an 8-bit binary string.
    to_binary(seq, binary_seq);
    // Build the packet string: mode (as a digit), binary sequence, followed by the payload.
    sprintf(message, "%d%s%s", mode, binary_seq, buf);
}

// Deconstruct a packet to extract its mode, sequence number, and payload.
void deconstruct_pkt(int* mode, int* seq, char* buf, int len, char* message, int msg_size){
    // Determine the mode based on the first character of the packet.
    *mode = (message[0] == '1') ? PSH : ACK;
    
    // Create a buffer to hold the 8-bit binary sequence number.
    char seq_buf[9] = {0};
    // Extract the binary sequence number from the packet.
    for (int i = 0; i < 8; i++) {
        seq_buf[i] = message[i + 1];
    }
    // Convert the binary string into an integer.
    *seq = (int) strtol(seq_buf, NULL, 2);

    // Clear the payload buffer.
    memset(buf, 0, len);

    // Define where the payload data starts in the packet.
    int data_start = 9;
    // Determine the maximum possible length for the payload.
    int data_len = msg_size - data_start;
    // Copy the payload from the packet into the provided buffer.
    if (data_len > 0 && data_len <= len) {
        strncpy(buf, message + data_start, data_len);
    }
}

// Simulate packet loss by randomly deciding whether to drop a message.
int DropMessage(){
    // Generate a random float between 0 and 1.
    float r = (float)rand() / (float)RAND_MAX;
    // Drop the message if the generated value is less than p.
    if (r < p) return 1;
    return 0;
}

// Convert an integer into its 8-bit binary string representation.
void to_binary(int num, char *binary_str){
    // Iterate over each bit from most significant (bit 7) to least significant (bit 0).
    for (int i = 7; i >= 0; i--) {
        binary_str[7 - i] = (num & (1 << i)) ? '1' : '0';
    }
    // Null-terminate the binary string.
    binary_str[8] = '\0';
}
