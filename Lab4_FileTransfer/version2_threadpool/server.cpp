
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <algorithm>

#include <openssl/evp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
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

// Reliable TCP send.
bool send_all(int sock, const void* data, size_t size) {
    const char* p = static_cast<const char*>(data);

    while (size > 0) {
        ssize_t n = send(sock, p, size, MSG_NOSIGNAL);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return false;

        p += n;
        size -= static_cast<size_t>(n);
    }

    return true;
}

// Reliable TCP receive.
bool recv_all(int sock, void* data, size_t size) {
    char* p = static_cast<char*>(data);

    while (size > 0) {
        ssize_t n = recv(sock, p, size, 0);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return false;

        p += n;
        size -= static_cast<size_t>(n);
    }

    return true;
}

// Send 64-bit integer in network byte order.
bool send_u64(int sock, uint64_t value) {
    unsigned char bytes[8];

    for (int i = 7; i >= 0; --i) {
        bytes[i] = static_cast<unsigned char>(value & 255);
        value >>= 8;
    }

    return send_all(sock, bytes, sizeof(bytes));
}

// Receive 64-bit integer in network byte order.
bool recv_u64(int sock, uint64_t& value) {
    unsigned char bytes[8];

    if (!recv_all(sock, bytes, sizeof(bytes)))
        return false;

    value = 0;

    for (unsigned char byte : bytes)
        value = (value << 8) | byte;

    return true;
}

// Safe filename: no paths or hidden special entries.
bool valid_filename(const std::string& name) {
    if (name.empty() || name.size() >= 256)
        return false;

    if (name == "." || name == "..")
        return false;

    for (unsigned char c : name) {
        if (c == '/' || c == '\\' || c == 0 ||
            c < 32 || c == 127)
            return false;
    }

    return true;
}

// Receive a filename from the client.
bool receive_filename(int sock, std::string& name) {
    uint32_t net_len = 0;

    if (!recv_all(sock, &net_len, sizeof(net_len)))
        return false;

    uint32_t len = ntohl(net_len);

    if (len == 0 || len >= 256)
        return false;

    name.assign(len, '\0');

    return recv_all(sock, name.data(), len);
}

// Write all bytes into an open local file.
bool write_all_fd(int fd, const void* data, size_t size) {
    const char* p = static_cast<const char*>(data);

    while (size > 0) {
        ssize_t n = write(fd, p, size);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return false;

        p += n;
        size -= static_cast<size_t>(n);
    }

    return true;
}

// Download: server -> client.
void download_file(int sock, const std::string& name) {
    std::ifstream file(name, std::ios::binary);

    if (!file) {
        log_message("Download file not found: " + name);
        send_u64(sock, 0);
        return;
    }

    file.seekg(0, std::ios::end);
    std::streampos pos = file.tellg();

    if (pos <= 0) {
        log_message("Download file empty or unreadable");
        send_u64(sock, 0);
        return;
    }

    uint64_t size = static_cast<uint64_t>(
        static_cast<std::streamoff>(pos)
    );

    file.seekg(0, std::ios::beg);

    if (!file || !send_u64(sock, size))
        return;

    using Digest = std::unique_ptr<
        EVP_MD_CTX, decltype(&EVP_MD_CTX_free)
    >;

    Digest ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    if (!ctx ||
        EVP_DigestInit_ex(
            ctx.get(), EVP_sha256(), nullptr) != 1)
        return;

    char buffer[BUFFER_SIZE];
    uint64_t sent = 0;

    while (sent < size) {
        size_t amount = static_cast<size_t>(
            std::min<uint64_t>(size - sent, BUFFER_SIZE)
        );

        file.read(buffer, amount);
        std::streamsize count = file.gcount();

        if (count <= 0)
            return;

        if (EVP_DigestUpdate(
                ctx.get(), buffer,
                static_cast<size_t>(count)) != 1 ||
            !send_all(sock, buffer,
                      static_cast<size_t>(count)))
            return;

        sent += static_cast<uint64_t>(count);
    }

    unsigned char hash[SHA256_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(), hash, &hash_len) != 1 ||
        hash_len != SHA256_SIZE)
        return;

    if (send_all(sock, hash, SHA256_SIZE)) {
        log_message(
            "DOWNLOAD COMPLETE: " + name +
            " | Bytes: " + std::to_string(sent)
        );
    }
}

// Upload: client -> server.
void upload_file(int sock, const std::string& name) {
    // uploads/ is created during project setup.
    const std::string path = "uploads/" + name;

    // Exclusive creation prevents overwriting uploads.
    // O_NOFOLLOW protects against final-component symlinks.
    int fd = open(
        path.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,
        S_IRUSR | S_IWUSR
    );

    unsigned char ready = fd >= 0 ? 1 : 0;

    if (!send_all(sock, &ready, 1)) {
        if (fd >= 0) {
            close(fd);
            unlink(path.c_str());
        }
        return;
    }

    if (fd < 0) {
        log_message("Upload rejected (file already exists or inaccessible): "
                    + name);
        return;
    }

    bool success = false;

    do {
        uint64_t size = 0;

        if (!recv_u64(sock, size)) {
            log_message("Failed to receive upload size");
            break;
        }

        using Digest = std::unique_ptr<
            EVP_MD_CTX, decltype(&EVP_MD_CTX_free)
        >;

        Digest ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

        if (!ctx ||
            EVP_DigestInit_ex(
                ctx.get(), EVP_sha256(), nullptr) != 1)
            break;

        char buffer[BUFFER_SIZE];
        uint64_t received = 0;
        bool transfer_ok = true;

        while (received < size) {
            size_t amount = static_cast<size_t>(
                std::min<uint64_t>(
                    size - received, BUFFER_SIZE
                )
            );

            ssize_t n = recv(sock, buffer, amount, 0);

            if (n < 0 && errno == EINTR)
                continue;

            if (n <= 0) {
                transfer_ok = false;
                break;
            }

            if (!write_all_fd(
                    fd, buffer, static_cast<size_t>(n)) ||
                EVP_DigestUpdate(
                    ctx.get(), buffer,
                    static_cast<size_t>(n)) != 1) {
                transfer_ok = false;
                break;
            }

            received += static_cast<uint64_t>(n);
        }

        if (!transfer_ok || received != size)
            break;

        unsigned char expected[SHA256_SIZE];
        unsigned char actual[SHA256_SIZE];
        unsigned int actual_len = 0;

        if (!recv_all(sock, expected, SHA256_SIZE) ||
            EVP_DigestFinal_ex(
                ctx.get(), actual, &actual_len) != 1 ||
            actual_len != SHA256_SIZE)
            break;

        if (std::memcmp(
                expected, actual, SHA256_SIZE) != 0) {
            log_message("Upload SHA-256 mismatch: " + name);
            break;
        }

        success = true;

        log_message(
            "UPLOAD COMPLETE: " + name +
            " | Bytes: " + std::to_string(received) +
            " | SHA-256: PASS"
        );

    } while (false);

    if (close(fd) != 0)
        success = false;

    if (!success) {
        unlink(path.c_str());
        log_message("UPLOAD FAILED: " + name);
    }

    // Acknowledge whether the server stored the file.
    unsigned char result = success ? 1 : 0;
    send_all(sock, &result, 1);
}

// Called by the existing thread pool.
void send_file(int client_socket) {
    auto start = std::chrono::steady_clock::now();

    char operation = 0;
    std::string filename;

    if (!recv_all(client_socket, &operation, 1) ||
        !receive_filename(client_socket, filename)) {
        log_message("Invalid client request");
        return;
    }

    if (!valid_filename(filename)) {
        log_message("Rejected unsafe filename");
        return;
    }

    if (operation == 'D') {
        log_message("DOWNLOAD REQUEST: " + filename);
        download_file(client_socket, filename);
    } else if (operation == 'U') {
        log_message("UPLOAD REQUEST: " + filename);
        upload_file(client_socket, filename);
    } else {
        log_message("Unknown operation");
        return;
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    log_message(
        "Request handling time: " +
        std::to_string(elapsed.count()) + " seconds"
    );
}

int main() {
    // Ensure the upload destination is a real directory.
    struct stat upload_info{};

    if (lstat("uploads", &upload_info) != 0 ||
        !S_ISDIR(upload_info.st_mode)) {
        std::cerr << "Error: uploads/ directory is missing\n";
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;

    if (setsockopt(
            server_fd, SOL_SOCKET, SO_REUSEADDR,
            &reuse, sizeof(reuse)) < 0) {
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

    log_message("Thread pool server listening on port 9000");

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
            if (errno != EINTR)
                perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(
                AF_INET, &client_address.sin_addr,
                client_ip, sizeof(client_ip))) {
            log_message(
                "Client connected: " + std::string(client_ip)
            );
        }

        pool.enqueue(client_socket);
    }

    close(server_fd);
    return 0;
}
