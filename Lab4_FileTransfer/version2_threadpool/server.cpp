
#include <iostream>
#include <fstream>
#include <string>
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

#include "threadpool.h"

#define PORT 9000
#define BUFFER_SIZE 4096
#define THREADS 4

constexpr size_t SHA256_SIZE = 32;

// Send all bytes, even when send() returns a partial result.
bool send_all(int sock, const void* data, size_t length) {
    const char* bytes = static_cast<const char*>(data);
    size_t total_sent = 0;

    while (total_sent < length) {
        ssize_t n = send(
            sock,
            bytes + total_sent,
            length - total_sent,
            MSG_NOSIGNAL
        );

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
            perror("send");
            return false;
        }

        total_sent += static_cast<size_t>(n);
    }

    return true;
}

// Receive the exact number of requested bytes.
bool recv_all(int sock, void* data, size_t length) {
    char* bytes = static_cast<char*>(data);
    size_t total_received = 0;

    while (total_received < length) {
        ssize_t n = recv(
            sock,
            bytes + total_received,
            length - total_received,
            0
        );

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
            return false;
        }

        total_received += static_cast<size_t>(n);
    }

    return true;
}

// Tell the client that a file is unavailable.
void send_not_found(int client_socket) {
    uint32_t zero = htonl(0);
    send_all(client_socket, &zero, sizeof(zero));
}

// This function is called by each worker thread.
void send_file(int client_socket) {
    auto start = std::chrono::steady_clock::now();

    uint32_t net_name_len = 0;

    // Receive filename length.
    if (!recv_all(
            client_socket,
            &net_name_len,
            sizeof(net_name_len))) {
        std::cerr << "Failed to receive filename length\n";
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
        std::cerr << "Failed to receive filename\n";
        return;
    }

    std::cout << "File requested: "
              << filename << std::endl;

    // Open requested file.
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        std::cerr << "File not found: "
                  << filename << std::endl;

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

    // Send file size using network byte order.
    uint32_t net_size = htonl(file_size);

    if (!send_all(
            client_socket,
            &net_size,
            sizeof(net_size))) {
        return;
    }

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

        std::cerr << "SHA-256 initialization failed\n";
        return;
    }

    // Transfer file in chunks.
    char buffer[BUFFER_SIZE];
    uint64_t total_sent = 0;

    while (file) {
        file.read(buffer, sizeof(buffer));

        std::streamsize count = file.gcount();

        if (count <= 0) {
            break;
        }

        if (EVP_DigestUpdate(
                ctx.get(),
                buffer,
                static_cast<size_t>(count)) != 1) {

            std::cerr << "SHA-256 update failed\n";
            return;
        }

        if (!send_all(
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

    // Finish SHA-256 calculation.
    unsigned char digest[SHA256_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(),
            digest,
            &digest_len) != 1 ||
        digest_len != SHA256_SIZE) {

        std::cerr << "SHA-256 finalization failed\n";
        return;
    }

    // Send checksum to client.
    if (!send_all(
            client_socket,
            digest,
            SHA256_SIZE)) {

        std::cerr << "Checksum transmission failed\n";
        return;
    }

    file.close();

    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed =
        end - start;

    std::cout << "SHA-256 checksum sent\n";
    std::cout << "File transfer completed: "
              << filename << '\n';

    std::cout << "Transfer handling time: "
              << elapsed.count()
              << " seconds\n";
}

int main() {
    // Create TCP socket.
    int server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Allow reuse of the port after restarting.
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

    // Configure server address.
    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(PORT);

    // Bind to port 9000.
    if (bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {

        perror("bind");
        close(server_fd);
        return 1;
    }

    // Listen for incoming connections.
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Thread pool server listening on port "
              << PORT << std::endl;

    // Create four worker threads.
    ThreadPool pool(THREADS);

    // Main thread accepts connections.
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

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

        // Log the client's IP address.
        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(
                AF_INET,
                &client_address.sin_addr,
                client_ip,
                sizeof(client_ip))) {

            std::cout << "Client connected: "
                      << client_ip << std::endl;
        }

        // Add the client to the thread pool's queue.
        pool.enqueue(client_socket);

        std::cout << "Client added to task queue\n";
    }

    close(server_fd);
    return 0;
}
