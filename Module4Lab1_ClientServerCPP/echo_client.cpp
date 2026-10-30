
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    const char* server_ip = "127.0.0.1";
    int port = 8080;

    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip,
                  &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (sockaddr*)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::string message;

    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    if (write(sock, message.c_str(),
              message.size()) < 0) {
        perror("write");
        close(sock);
        return 1;
    }

    char buffer[1024];

    ssize_t bytes_read = read(
        sock,
        buffer,
        sizeof(buffer)
    );

    if (bytes_read > 0) {
        std::cout << "Server echoed: ";
        std::cout.write(buffer, bytes_read);
        std::cout << std::endl;
    }

    close(sock);
    return 0;
}
