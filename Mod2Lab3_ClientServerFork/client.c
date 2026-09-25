#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;

    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Create client socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(
        AF_INET,
        "127.0.0.1",
        &server_addr.sin_addr
    ) <= 0) {
        perror("invalid address");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(
        sock,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) < 0) {
        perror("connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to C server.\n");

    printf("Enter message: ");
    fgets(message, BUFFER_SIZE, stdin);

    // Remove newline from fgets
    message[strcspn(message, "\n")] = '\0';

    send(
        sock,
        message,
        strlen(message),
        0
    );

    memset(buffer, 0, BUFFER_SIZE);

    ssize_t bytes_read =
        read(sock, buffer, BUFFER_SIZE - 1);

    if (bytes_read > 0) {
        printf("Server response: %s\n", buffer);
    }

    close(sock);

    return 0;
}