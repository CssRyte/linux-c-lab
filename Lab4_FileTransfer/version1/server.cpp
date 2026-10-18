
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <memory>
#include <openssl/evp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9000
#define BUFFER_SIZE 4096

constexpr size_t SHA256_SIZE = 32;

// Send all bytes, including when send() is partial.
bool send_all(int sock, const void* data, size_t length) {
    const char* bytes = static_cast<const char*>(data);
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(
            sock,
            bytes + sent,
            length - sent,
            MSG_NOSIGNAL
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

// Receive an exact number of bytes.
bool recv_all(int sock, void* data, size_t length) {
    char* bytes = static_cast<char*>(data);
    size_t received = 0;

    while (received < length) {
        ssize_t n = recv(
            sock,
            bytes + received,
            length - received,
            0
        );

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return false;

        received += static_cast<size_t>(n);
    }

    return true;
}

// Respond when a file is unavailable.
void send_not_found(int client_socket) {
    uint32_t zero = htonl(0);
    send_all(client_socket, &zero, sizeof(zero));
}

void send_file(int client_socket) {
    uint32_t net_name_len = 0;

    // Receive filename length.
    if (!recv_all(
            client_socket,
            &net_name_len,
            sizeof(net_name_len))) {
        std::cerr << "Could not read filename length\n";
        return;
    }

    uint32_t name_len = ntohl(net_name_len);

    if (name_len == 0 || name_len >= 256) {
        std::cerr << "Invalid filename length\n";
        return;
    }

    // Receive filename.
    std::string filename(name_len, '\0');

    if (!recv_all(
            client_socket,
            filename.data(),
            name_len)) {
        std::cerr << "Could not read filename\n";
        return;
    }

    std::cout << "File requested: "
              << filename << '\n';

    // Open requested file.
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        std::cerr << "File not found\n";
        send_not_found(client_socket);
        return;
    }

    // Determine file size.
    file.seekg(0, std::ios::end);
    std::streampos end_pos = file.tellg();

    if (end_pos <= 0 ||
        end_pos > static_cast<std::streampos>(
            std::numeric_limits<uint32_t>::max())) {
        std::cerr << "Empty or unsupported file size\n";
        send_not_found(client_socket);
        return;
    }

    uint32_t file_size =
        static_cast<uint32_t>(end_pos);

    file.seekg(0, std::ios::beg);

    std::cout << "File size: "
              << file_size << " bytes\n";

    // Protocol:
    // [4-byte file size]
    // [file data]
    // [32-byte SHA-256 checksum]

    uint32_t net_size = htonl(file_size);

    if (!send_all(
            client_socket,
            &net_size,
            sizeof(net_size))) {
        return;
    }

    // Initialize SHA-256 calculation.
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

    // Read and send file data in chunks.
    char buffer[BUFFER_SIZE];
    uint64_t total_sent = 0;

    while (file) {
        file.read(buffer, sizeof(buffer));

        std::streamsize count = file.gcount();

        if (count <= 0)
            break;

        // Include this chunk in the hash.
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

        total_sent += static_cast<size_t>(count);
    }

    if (file.bad() || total_sent != file_size) {
        std::cerr << "File read failed or size changed\n";
        return;
    }

    // Finalize the SHA-256 checksum.
    unsigned char digest[SHA256_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(),
            digest,
            &digest_len) != 1 ||
        digest_len != SHA256_SIZE ||
        !send_all(
            client_socket,
            digest,
            SHA256_SIZE)) {
        std::cerr << "Failed to send SHA-256 checksum\n";
        return;
    }

    std::cout << "SHA-256 checksum sent\n";
    std::cout << "File transfer completed\n";
}

int main() {
    // Create TCP socket.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Allow the port to be reused after restarting.
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

    // Bind server to port.
    if (bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Listen for incoming clients.
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port "
              << PORT << std::endl;

    // Handle clients sequentially.
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len =
            sizeof(client_address);

        int client_socket = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(
                &client_address),
            &client_len
        );

        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        // Record client IP address.
        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(
                AF_INET,
                &client_address.sin_addr,
                client_ip,
                sizeof(client_ip))) {
            std::cout << "Client connected: "
                      << client_ip << '\n';
        }

        // Start transfer timer.
        auto start =
            std::chrono::steady_clock::now();

        send_file(client_socket);

        // Stop transfer timer.
        auto finish =
            std::chrono::steady_clock::now();

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
