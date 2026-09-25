#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    sockaddr_in server_addr{};
    socklen_t addr_len = sizeof(server_addr);

    char buffer[BUFFER_SIZE];

    // Prevent zombie child processes
    signal(SIGCHLD, SIG_IGN);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    int option = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(
        server_fd,
        reinterpret_cast<sockaddr *>(&server_addr),
        sizeof(server_addr)
    ) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    std::cout << "C++ Server listening on port "
              << PORT << "..." << std::endl;

    while (true) {

        client_fd = accept(
            server_fd,
            reinterpret_cast<sockaddr *>(&server_addr),
            &addr_len
        );

        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        std::cout
            << "Client connected. Parent PID: "
            << getpid()
            << std::endl;

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            close(client_fd);
            continue;
        }

        // Child process
        if (pid == 0) {

            close(server_fd);

            std::cout
                << "Child PID "
                << getpid()
                << " handling client."
                << std::endl;

            std::memset(buffer, 0, BUFFER_SIZE);

            ssize_t bytes_read =
                read(client_fd, buffer, BUFFER_SIZE - 1);

            if (bytes_read > 0) {

                std::cout
                    << "Child PID "
                    << getpid()
                    << " received: "
                    << buffer
                    << std::endl;

                std::string response =
                    "Message received by C++ server child PID "
                    + std::to_string(getpid());

                send(
                    client_fd,
                    response.c_str(),
                    response.length(),
                    0
                );
            }

            close(client_fd);

            std::cout
                << "Child PID "
                << getpid()
                << " finished."
                << std::endl;

            exit(EXIT_SUCCESS);
        }

        // Parent returns to accept()
        close(client_fd);
    }

    close(server_fd);

    return 0;
}