
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    int port = 8080;

    // Allow the user to specify a port.
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    // Create a TCP socket.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Configure the server address.
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address and port.
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Listen for incoming connections.
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Echo server listening on port "
              << port << std::endl;

    // Continuously accept client connections.
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (sockaddr*)&client_addr,
            &client_len
        );

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::cout << "Client connected" << std::endl;

        char buffer[1024];

        // Keep reading until the client sends EOF.
        while (true) {
            ssize_t bytes_read = read(
                client_fd,
                buffer,
                sizeof(buffer)
            );

            // Client has finished sending data.
            if (bytes_read == 0) {
                std::cout << "Client disconnected (EOF)"
                          << std::endl;
                break;
            }

            // Handle read errors.
            if (bytes_read < 0) {
                if (errno == EINTR) {
                    continue;
                }

                perror("read");
                break;
            }

            // Display the received message.
            std::cout << "Received: ";
            std::cout.write(buffer, bytes_read);
            std::cout << std::endl;

            // Echo all received bytes to the client.
            ssize_t total_written = 0;

            while (total_written < bytes_read) {
                ssize_t written = write(
                    client_fd,
                    buffer + total_written,
                    bytes_read - total_written
                );

                if (written < 0 && errno == EINTR) {
                    continue;
                }

                if (written <= 0) {
                    perror("write");
                    break;
                }

                total_written += written;
            }

            // Stop if the response was not fully sent.
            if (total_written < bytes_read) {
                break;
            }
        }

        // Close the current client connection.
        close(client_fd);
    }

    // Close the listening socket.
    close(server_fd);

    return 0;
}
