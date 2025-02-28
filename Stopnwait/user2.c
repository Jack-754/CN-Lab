/*
NAME: MORE AAYUSH BABASAHEB
ROLL NO.: 22CS30063
ASSIGNMENT: 4
*/

#include "ksocket.h"

// Global variable to keep track of the current sequence number for packets.
int curseq = 0;

// Global pointer to store the destination socket address for future use.
struct sockaddr_in *destination;

int main(int argc, char *argv[])
{
    // Print a startup message indicating that USER 2 is running.
    printf("The USER 2 is running...\n");
    
    // Check if the required number of command-line arguments is provided.
    if (argc < 6)
    {
        // Inform the user about the correct input format and exit.
        printf("Input format:\n<cmd> <Source IP> <Source Port> <Destination IP> <Destination Port> <outputfile.txt>\n");
        exit(1);
    }

    // Create a socket using the custom k_socket function with the specified domain and type.
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);

    // Check if the socket was created successfully.
    if (sockfd == -1)
    {
        perror("Socket Error\n");
        exit(1);
    }

    // Declare structures to hold the source and destination (sender) addresses.
    struct sockaddr_in src_addr, dest_addr;

    // Clear the memory for both source and destination address structures.
    bzero(&src_addr, sizeof(src_addr));
    bzero(&dest_addr, sizeof(src_addr)); // Both structures have the same size.

    // Convert the source IP address (argv[1]) from a string to a network address and store it.
    inet_aton(argv[1], &src_addr.sin_addr);
    // Convert the destination IP address (argv[3]) from a string to a network address and store it.
    inet_aton(argv[3], &dest_addr.sin_addr);

    // Set the address family for both addresses to IPv4.
    src_addr.sin_family = dest_addr.sin_family = AF_INET;

    // Convert and set the source port number (argv[2]) to network byte order.
    src_addr.sin_port = htons(atoi(argv[2]));
    // Convert and set the destination port number (argv[4]) to network byte order.
    dest_addr.sin_port = htons(atoi(argv[4]));

    // Bind the socket to the source address and also assign the destination address globally.
    k_bind(sockfd, &src_addr, &dest_addr);

    // Open the output file (argv[5]) with flags to create, write, and truncate it, setting permissions to 0666.
    int fd = open(argv[5], O_CREAT | O_WRONLY | O_TRUNC, 0666);

    // Check if the file was opened successfully.
    if (fd == -1)
    {
        perror("Error in opening file\n");
        exit(1);
    }
    
    // Allocate a buffer to store incoming data from the socket.
    char buf[MSG_SIZE];

    // Print a message indicating that file reception is starting.
    printf("The User 2 starts receiving file...\n");
    
    // Declare variables to store the number of bytes received and the sequence number.
    int bytesReceived;
    int seq;
    int i = 0;
    
    // Continuously receive packets until the transmission is complete.
    while (1)
    {
        // Receive data from the socket using the custom k_recvfrom function.
        // The function fills the buffer 'buf' and updates the sequence number 'seq'.
        bytesReceived = k_recvfrom(sockfd, &seq, buf, sizeof(buf));
        
        // If the number of bytes received is zero or negative, the connection is likely closed.
        if (bytesReceived <= 0)
        {
            printf("The connection is removed.\n");
            break;
        }
        
        // If an error occurs during reception, notify the user.
        if (bytesReceived == -1)
        {
            printf("The Received Messages are empty right now.\n");
        }
        
        // Print the sequence number of the newly received segment.
        printf("User 2 gets new segment with sequence number: %d\n", seq);
        
        // Write the received data to the output file.
        if (write(fd, buf, bytesReceived) == -1)
        {
            perror("Write error to file\n");
            exit(1);
        }
        
        // If an empty message is received (i.e., zero-length), break out of the loop.
        if (strlen(buf) == 0)
        {
            break;
        }
    }
    
    // Close the output file.
    close(fd);
    // Close the socket using the custom k_close function.
    k_close(sockfd);
    // Exit the program successfully.
    exit(0);
}
