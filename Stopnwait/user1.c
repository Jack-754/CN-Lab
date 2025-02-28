/*
NAME: MORE AAYUSH BABASAHEB
ROLL NO.: 22CS30063
ASSIGNMENT: 4
*/

#include "ksocket.h"

// Global variable to keep track of the current sequence number for packets.
int curseq = 0;

// Global pointer to store the destination socket address.
struct sockaddr_in *destination;

int main(int argc, char *argv[])
{
    // Print a startup message indicating that USER1 is running.
    printf("The USER1 is running...\n");
    
    // Ensure that all required command-line arguments are provided.
    if (argc < 6)
    {
        // Print the expected input format and exit if arguments are insufficient.
        printf("Input format:\n<cmd> <src IP> <src Port> <dest IP> <dest Port> <inputfile.txt>\n");
        exit(1);
    }

    // Create a socket using the custom k_socket function.
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);

    // Check if the socket was created successfully.
    if (sockfd == -1)
    {
        perror("Socket error\n");
        exit(1);
    }

    // Declare structures to hold the source and destination socket addresses.
    struct sockaddr_in src_addr, dest_addr;

    // Zero out the memory for both source and destination address structures.
    bzero(&src_addr, sizeof(src_addr));
    bzero(&dest_addr, sizeof(src_addr));  // Using src_addr size for both is acceptable if both are same size.

    // Convert the source IP address from string format to network address and store it.
    inet_aton(argv[1], &src_addr.sin_addr);
    // Convert the destination IP address from string format to network address and store it.
    inet_aton(argv[3], &dest_addr.sin_addr);

    // Set the address family (IPv4) for both source and destination addresses.
    src_addr.sin_family = dest_addr.sin_family = AF_INET;

    // Convert and set the source port (provided as a string) to network byte order.
    src_addr.sin_port = htons(atoi(argv[2]));
    // Convert and set the destination port (provided as a string) to network byte order.
    dest_addr.sin_port = htons(atoi(argv[4]));

    // Bind the socket to the source address and also set the destination address globally.
    k_bind(sockfd, &src_addr, &dest_addr);

    // Open the input file (specified in the command-line arguments) in read-only mode.
    int fd = open(argv[5], O_RDONLY);

    // Check if the file was opened successfully.
    if (fd == -1)
    {
        perror("Error in opening file\n");
        exit(1);
    }

    // Buffer to store chunks of data read from the input file.
    char buf[MSG_SIZE];

    // Print a message indicating that the file transmission is starting.
    printf("The User1 starts sending file...\n");

    // Variable for iteration (unused further but declared as part of the loop structure).
    int i = 0;
    
    // Loop continuously until the entire file is read and sent.
    while (1)
    {
        // Clear the buffer to remove any residual data from previous iterations.
        memset(buf, '\0', sizeof(buf));
        
        // Read data from the file into the buffer, reserving one byte for the null-terminator.
        int byteRead = read(fd, buf, sizeof(buf) - 1);
        
        // Ensure the buffer is null-terminated after reading.
        buf[byteRead] = '\0';
        
        // Variable to store the sequence number associated with the sent packet.
        int seq;
        
        // Check for errors during file read operation.
        if (byteRead == -1)
        {
            perror("Read error\n");
            exit(1);
        }
        
        // If no bytes were read, it indicates the end-of-file.
        if (byteRead == 0)
        {
            // Send an empty packet to signal EOF.
            int byteSent = k_sendto(sockfd, &seq, buf, strlen(buf), &dest_addr);
            printf("EOF\n");
            break;
        }
        
        // Send the data read from the file as a packet using the custom k_sendto function.
        int byteSent = k_sendto(sockfd, &seq, buf, byteRead, &dest_addr);
        
        // Check for errors during the send operation.
        if (byteSent == -1)
        {
            // Handle the error if the socket is not bound.
            if (errno == ENOTBOUND)
            {
                perror("Trying to Send to a not bound port and IP.\n");
                exit(1);
            }
            // Handle the error if there is no space in the send buffer.
            else if (errno == ENOSPACE)
            {
                printf("There is no space in the send buffer will try again.\n");
                continue;
            }
        }
        // On successful transmission, print the sequence number and the number of bytes sent.
        printf("Data packet sent with seq number: %d and bytes: %d\n", seq, byteSent);
    }
    
    // Close the input file descriptor.
    close(fd);
    // Close the socket using the custom k_close function.
    k_close(sockfd);
    // Exit the program successfully.
    exit(0);
}
