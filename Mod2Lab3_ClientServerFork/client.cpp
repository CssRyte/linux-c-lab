#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    sockaddr_in server_addr{};

    char buffer[BUFFER_SIZE];

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket failed");
        return 1;
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
        return 1;
    }

    // Connect to server
    if (connect(
        sock,
        reinterpret_cast<sockaddr *>(&server_addr),
        sizeof(server_addr)
    ) < 0) {
        perror("connection failed");
        close(sock);
        return 1;
    }

    std::cout << "Connected to C++ server." << std::endl;

    std::string message;

    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    send(
        sock,
        message.c_str(),
        message.length(),
        0
    );

    std::memset(buffer, 0, BUFFER_SIZE);

    ssize_t bytes_read =
        read(sock, buffer, BUFFER_SIZE - 1);

    if (bytes_read > 0) {
        std::cout
            << "Server response: "
            << buffer
            << std::endl;
    }

    close(sock);

    return 0;
}