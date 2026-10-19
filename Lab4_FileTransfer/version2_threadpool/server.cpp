
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <sstream>

#include <openssl/evp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "threadpool.h"

constexpr int PORT = 9000;
constexpr size_t BUFFER_SIZE = 4096;
constexpr size_t SHA256_SIZE = 32;
constexpr size_t WORKER_COUNT = 4;

std::mutex log_mutex;

void log_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << message << std::endl;
}

// Send exactly the number of bytes requested.
bool send_all(int sock, const void* data, size_t length) {
    const char* bytes = static_cast<const char*>(data);
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(
            sock, bytes + sent, length - sent, MSG_NOSIGNAL
        );

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
            perror("send");
            return false;
        }

        sent += static_cast<size_t>(n);
    }

    return true;
}

// Receive exactly the number of bytes requested.
bool recv_all(int sock, void* data, size_t length) {
    char* bytes = static_cast<char*>(data);
    size_t received = 0;

    while (received < length) {
        ssize_t n = recv(
            sock, bytes + received, length - received, 0
        );

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
            return false;
        }

        received += static_cast<size_t>(n);
    }

    return true;
}

// Send a 64-bit integer in network byte order.
bool send_u64(int sock, uint64_t value) {
    unsigned char bytes[8];

    for (int i = 7; i >= 0; --i) {
        bytes[i] = static_cast<unsigned char>(
            value & 0xFF
        );
        value >>= 8;
    }

    return send_all(sock, bytes, sizeof(bytes));
}

// 0 means file unavailable in this lab protocol.
void send_not_found(int sock) {
    send_u64(sock, 0);
}

// Called by threadpool.cpp for each client.
void send_file(int client_socket) {
    auto start = std::chrono::steady_clock::now();

    uint32_t net_name_len = 0;

    if (!recv_all(
            client_socket,
            &net_name_len,
            sizeof(net_name_len))) {
        log_message("Failed to receive filename length");
        return;
    }

    uint32_t name_len = ntohl(net_name_len);

    if (name_len == 0 || name_len >= 256) {
        log_message("Invalid filename length");
        return;
    }

    std::string filename(name_len, '\0');

    if (!recv_all(
            client_socket,
            filename.data(),
            name_len)) {
        log_message("Failed to receive filename");
        return;
    }

    log_message("File requested: " + filename);

    // Basic path validation: allow files in the server's
    // current directory, not arbitrary system paths.
    if (filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos ||
        filename.find('\0') != std::string::npos ||
        filename == "." || filename == "..") {
        log_message("Rejected unsafe filename");
        send_not_found(client_socket);
        return;
    }

    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        log_message("File not found: " + filename);
        send_not_found(client_socket);
        return;
    }

    file.seekg(0, std::ios::end);
    std::streampos end_pos = file.tellg();

    if (end_pos <= 0) {
        log_message("Empty or unreadable file");
        send_not_found(client_socket);
        return;
    }

    uint64_t file_size = static_cast<uint64_t>(
        static_cast<std::streamoff>(end_pos)
    );

    file.seekg(0, std::ios::beg);

    if (!file) {
        log_message("Failed to rewind file");
        send_not_found(client_socket);
        return;
    }

    log_message(
        "File size: " + std::to_string(file_size) +
        " bytes"
    );

    // Send the 8-byte file size.
    if (!send_u64(client_socket, file_size)) {
        log_message("Failed to send file size");
        return;
    }

    using DigestContext = std::unique_ptr<
        EVP_MD_CTX, decltype(&EVP_MD_CTX_free)
    >;

    DigestContext ctx(
        EVP_MD_CTX_new(), EVP_MD_CTX_free
    );

    if (!ctx ||
        EVP_DigestInit_ex(
            ctx.get(), EVP_sha256(), nullptr) != 1) {
        log_message("SHA-256 initialization failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    uint64_t total_sent = 0;

    while (total_sent < file_size) {
        size_t remaining = static_cast<size_t>(
            std::min<uint64_t>(
                file_size - total_sent, BUFFER_SIZE
            )
        );

        file.read(buffer, remaining);
        std::streamsize count = file.gcount();

        if (count <= 0) {
            log_message("File read interrupted");
            return;
        }

        if (EVP_DigestUpdate(
                ctx.get(),
                buffer,
                static_cast<size_t>(count)) != 1) {
            log_message("SHA-256 update failed");
            return;
        }

        if (!send_all(
                client_socket,
                buffer,
                static_cast<size_t>(count))) {
            log_message("File transmission failed");
            return;
        }

        total_sent += static_cast<uint64_t>(count);
    }

    if (total_sent != file_size) {
        log_message("Incomplete file transfer");
        return;
    }

    unsigned char digest[SHA256_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(),
            digest,
            &digest_len) != 1 ||
        digest_len != SHA256_SIZE) {
        log_message("SHA-256 finalization failed");
        return;
    }

    if (!send_all(
            client_socket, digest, SHA256_SIZE)) {
        log_message("Failed to send checksum");
        return;
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::ostringstream message;
    message << "Transfer completed: " << filename
            << " | Bytes: " << total_sent
            << " | Time: " << elapsed.count()
            << " seconds";

    log_message("SHA-256 checksum sent");
    log_message(message.str());
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

    log_message(
        "Thread pool server listening on port " +
        std::to_string(PORT)
    );

    ThreadPool pool(WORKER_COUNT);

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        int client_socket = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_len
        );

        if (client_socket < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(
                AF_INET,
                &client_address.sin_addr,
                client_ip,
                sizeof(client_ip))) {
            log_message(
                "Client connected: " +
                std::string(client_ip)
            );
        }

        pool.enqueue(client_socket);
        log_message("Client added to task queue");
    }

    close(server_fd);
    return 0;
}
