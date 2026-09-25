#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    char buffer[BUFFER_SIZE];

    // Prevent zombie child processes
    signal(SIGCHLD, SIG_IGN);

    // Create the server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int option = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );

    // Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to port
    if (bind(
        server_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Begin listening
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("C Server listening on port %d...\n", PORT);

    while (1) {

        client_fd = accept(
            server_fd,
            (struct sockaddr *)&server_addr,
            &addr_len
        );

        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        printf(
            "Client connected. Parent PID: %d\n",
            getpid()
        );

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            close(client_fd);
            continue;
        }

        // Child process
        if (pid == 0) {

            close(server_fd);

            printf(
                "Child PID %d handling client.\n",
                getpid()
            );

            memset(buffer, 0, BUFFER_SIZE);

            ssize_t bytes_read =
                read(client_fd, buffer, BUFFER_SIZE - 1);

            if (bytes_read > 0) {

                printf(
                    "Child PID %d received: %s\n",
                    getpid(),
                    buffer
                );

                char response[BUFFER_SIZE];

                snprintf(
                    response,
                    BUFFER_SIZE,
                    "Message received by server child PID %d",
                    getpid()
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );
            }

            close(client_fd);

            printf(
                "Child PID %d finished.\n",
                getpid()
            );

            exit(EXIT_SUCCESS);
        }

        // Parent does not handle client communication.
        // It immediately returns to accept().
        close(client_fd);
    }

    close(server_fd);

    return 0;
}