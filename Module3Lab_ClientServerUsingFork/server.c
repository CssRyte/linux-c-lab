#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080

volatile sig_atomic_t server_running = 1;
int server_sock = -1;

void reap_children(int sig) {
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap terminated child processes
    }
}

void graceful_shutdown(int sig) {
    (void)sig;

    server_running = 0;

    if (server_sock >= 0) {
        close(server_sock);
    }
}

void handle_client(int client_sock) {
    char buffer[1024];
    char command[9] = {0};

    ssize_t bytes_read;
    size_t total_bytes = 0;
    size_t command_len = 0;

    while ((bytes_read = read(
                client_sock,
                buffer,
                sizeof(buffer)
            )) > 0) {

        total_bytes += bytes_read;

        if (command_len < 8) {
            size_t copy_amount = bytes_read;

            if (copy_amount > 8 - command_len) {
                copy_amount = 8 - command_len;
            }

            memcpy(
                command + command_len,
                buffer,
                copy_amount
            );

            command_len += copy_amount;
        }
    }

    if (bytes_read < 0) {
        perror("read");
    }

    printf("Total bytes received: %zu\n", total_bytes);

    if (total_bytes == 8 &&
        strcmp(command, "shutdown") == 0) {

        printf("Shutdown command received.\n");

        write(
            client_sock,
            "Server shutting down",
            20
        );

        close(client_sock);

        kill(getppid(), SIGTERM);

        return;
    }

    write(
        client_sock,
        "Hello from server",
        17
    );

    close(client_sock);
}

int main() {
    int client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    struct sigaction sa;
    struct sigaction shutdown_sa;

    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGCHLD, &sa, NULL);

    shutdown_sa.sa_handler = graceful_shutdown;
    sigemptyset(&shutdown_sa.sa_mask);
    shutdown_sa.sa_flags = 0;

    sigaction(SIGTERM, &shutdown_sa, NULL);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;

    if (setsockopt(
            server_sock,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {

        perror("setsockopt");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(
            server_sock,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("bind");
        exit(1);
    }

    listen(server_sock, 5);

    while (server_running) {
        addr_size = sizeof(client_addr);

        client_sock = accept(
            server_sock,
            (struct sockaddr*)&client_addr,
            &addr_size
        );

        if (client_sock < 0) {
            if (!server_running) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        pid_t pid = fork();

        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
            exit(0);
        } else if (pid > 0) {
            close(client_sock);
        }
    }

    printf("Server shut down cleanly.\n");

    return 0;
}