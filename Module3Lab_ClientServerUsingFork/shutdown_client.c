#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char response[1024];

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

    send(sock, "shutdown", 8, 0);

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
