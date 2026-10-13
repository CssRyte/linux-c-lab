#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define PORT 8080

void reap_children(int sig) {
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap all terminated child processes
    }
}

void handle_client(int client_sock) {
    char buffer[1024];

    ssize_t bytes_read = read(
        client_sock,
        buffer,
        sizeof(buffer) - 1
    );

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);
    }

    write(client_sock, "Hello from server", 17);
    close(client_sock);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    struct sigaction sa;

    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGCHLD, &sa, NULL);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(
        server_sock,
        (struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );

    listen(server_sock, 5);

    while (1) {
        addr_size = sizeof(client_addr);

        client_sock = accept(
            server_sock,
            (struct sockaddr*)&client_addr,
            &addr_size
        );

        pid_t pid = fork();

        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
            exit(0);
        }
        else if (pid > 0) {
            close(client_sock);
        }
    }

    return 0;
}
