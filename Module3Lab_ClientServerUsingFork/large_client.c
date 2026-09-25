#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MESSAGE_SIZE 10000

int main() {
    int sock;
    struct sockaddr_in server_addr;

    char message[MESSAGE_SIZE + 1];
    char response[1024];

    memset(message, 'A', MESSAGE_SIZE);
    message[MESSAGE_SIZE] = '\0';

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &server_addr.sin_addr
    );

    if (connect(
            sock,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) < 0) {
        perror("connect");
        return 1;
    }

    printf("Sending %d characters...\n", MESSAGE_SIZE);

    size_t total_sent = 0;

while (total_sent < MESSAGE_SIZE) {
    ssize_t sent = send(
        sock,
        message + total_sent,
        MESSAGE_SIZE - total_sent,
        0
    );

    if (sent < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    total_sent += sent;
}

    shutdown(sock, SHUT_WR);

    ssize_t bytes_read = read(
        sock,
        response,
        sizeof(response) - 1
    );

    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        printf("Server response: %s\n", response);
    }

    close(sock);

    return 0;
}