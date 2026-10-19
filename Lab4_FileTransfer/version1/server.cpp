
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <cstdio>
#include <memory>

#include <openssl/evp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr int PORT = 9000;
constexpr size_t BUFFER_SIZE = 4096;
constexpr size_t SHA256_SIZE = 32;

// Send all requested bytes.
bool send_all(int sock, const void* data, size_t length) {
    const char* bytes = static_cast<const char*>(data);
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(
            sock, bytes + sent, length - sent, MSG_NOSIGNAL
        );

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0) {
            perror("send");
            return false;
        }

        sent += static_cast<size_t>(n);
    }

    return true;
}

// Receive all requested bytes.
bool recv_all(int sock, void* data, size_t length) {
    char* bytes = static_cast<char*>(data);
    size_t received = 0;

    while (received < length) {
        ssize_t n = recv(
            sock, bytes + received, length - received, 0
        );

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return false;

        received += static_cast<size_t>(n);
    }

    return true;
}

// Send a 64-bit integer in network byte order.
bool send_u64(int sock, uint64_t value) {
    unsigned char bytes[8];

    for (int i = 7; i >= 0; --i) {
        bytes[i] = static_cast<unsigned char>(value & 0xFF);
        value >>= 8;
    }

    return send_all(sock, bytes, sizeof(bytes));
}

void send_file(int client_socket) {
    uint32_t net_name_len = 0;

    if (!recv_all(
            client_socket,
            &net_name_len,
            sizeof(net_name_len))) {
        std::cerr << "Failed to read filename length\n";
        return;
    }

    uint32_t name_len = ntohl(net_name_len);

    if (name_len == 0 || name_len >= 256) {
        std::cerr << "Invalid filename length\n";
        return;
    }

    std::string filename(name_len, '\0');

    if (!recv_all(
            client_socket,
            filename.data(),
            name_len)) {
        std::cerr << "Failed to read filename\n";
        return;
    }

    std::cout << "File requested: "
              << filename << '\n';

    // Open the requested file.
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        std::cerr << "File not found\n";
        send_u64(client_socket, 0);
        return;
    }

    // Determine file size using a 64-bit integer.
    file.seekg(0, std::ios::end);
    std::streampos end_pos = file.tellg();

    if (end_pos <= 0) {
        std::cerr << "Empty file or unable to read size\n";
        send_u64(client_socket, 0);
        return;
    }

    uint64_t file_size = static_cast<uint64_t>(
        static_cast<std::streamoff>(end_pos)
    );

    file.seekg(0, std::ios::beg);

    if (!file) {
        std::cerr << "Failed to rewind file\n";
        send_u64(client_socket, 0);
        return;
    }

    std::cout << "File size: "
              << file_size << " bytes\n";

    // Updated protocol:
    // [8-byte file size]
    // [file contents]
    // [32-byte SHA-256 checksum]

    if (!send_u64(client_socket, file_size))
        return;

    // Initialize SHA-256.
    using DigestCtx = std::unique_ptr<
        EVP_MD_CTX,
        decltype(&EVP_MD_CTX_free)
    >;

    DigestCtx ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    if (!ctx ||
        EVP_DigestInit_ex(
            ctx.get(),
            EVP_sha256(),
            nullptr) != 1) {
        std::cerr << "Failed to initialize SHA-256\n";
        return;
    }

    // Transfer file contents.
    char buffer[BUFFER_SIZE];
    uint64_t total_sent = 0;

    while (total_sent < file_size) {
        file.read(buffer, sizeof(buffer));
        std::streamsize count = file.gcount();

        if (count <= 0)
            break;

        if (EVP_DigestUpdate(
                ctx.get(),
                buffer,
                static_cast<size_t>(count)) != 1 ||
            !send_all(
                client_socket,
                buffer,
                static_cast<size_t>(count))) {
            std::cerr << "File transfer failed\n";
            return;
        }

        total_sent += static_cast<uint64_t>(count);
    }

    if (total_sent != file_size) {
        std::cerr << "Incomplete file read\n";
        return;
    }

    // Finalize and send SHA-256.
    unsigned char digest[SHA256_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(),
            digest,
            &digest_len) != 1 ||
        digest_len != SHA256_SIZE ||
        !send_all(client_socket, digest, SHA256_SIZE)) {
        std::cerr << "Failed to send checksum\n";
        return;
    }

    std::cout << "SHA-256 checksum sent\n";
    std::cout << "File transfer completed\n";
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(PORT);

    if (bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port "
              << PORT << '\n';

    // Single-threaded client handling.
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        int client_socket = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_len
        );

        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(
                AF_INET,
                &client_address.sin_addr,
                client_ip,
                sizeof(client_ip))) {
            std::cout << "Client connected: "
                      << client_ip << '\n';
        }

        auto start = std::chrono::steady_clock::now();

        send_file(client_socket);

        auto finish = std::chrono::steady_clock::now();

        std::chrono::duration<double> elapsed =
            finish - start;

        std::cout << "Transfer handling time: "
                  << elapsed.count()
                  << " seconds\n";

        close(client_socket);
    }

    close(server_fd);
    return 0;
}
