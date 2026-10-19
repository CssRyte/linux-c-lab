
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>
#include <algorithm>

#include <openssl/evp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include "threadpool.h"

constexpr int PORT = 9000;
constexpr size_t BUFFER_SIZE = 4096;
constexpr size_t SHA256_SIZE = 32;
constexpr size_t WORKER_COUNT = 4;
constexpr size_t MAX_CONNECTIONS = 8;

// 100 MiB maximum transfer.
constexpr uint64_t MAX_FILE_SIZE =
    100ULL * 1024ULL * 1024ULL;

constexpr int IDLE_TIMEOUT_SECONDS = 10;

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

// 64-bit integer in network byte order.
bool send_u64(int sock, uint64_t value) {
    unsigned char bytes[8];

    for (int i = 7; i >= 0; --i) {
        bytes[i] =
            static_cast<unsigned char>(value & 255);
        value >>= 8;
    }

    return send_all(sock, bytes, sizeof(bytes));
}

bool recv_u64(int sock, uint64_t& value) {
    unsigned char bytes[8];

    if (!recv_all(sock, bytes, sizeof(bytes)))
        return false;

    value = 0;

    for (unsigned char byte : bytes)
        value = (value << 8) | byte;

    return true;
}

// Apply timeout to receive and send operations.
bool set_socket_timeout(int sock) {
    timeval timeout{};
    timeout.tv_sec = IDLE_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    return setsockopt(
        sock, SOL_SOCKET, SO_RCVTIMEO,
        &timeout, sizeof(timeout)) == 0
        &&
        setsockopt(
            sock, SOL_SOCKET, SO_SNDTIMEO,
            &timeout, sizeof(timeout)) == 0;
}

bool valid_filename(const std::string& name) {
    if (name.empty() || name.size() >= 256 ||
        name == "." || name == "..")
        return false;

    for (unsigned char c : name) {
        if (c == '/' || c == '\\' || c == 0 ||
            c < 32 || c == 127)
            return false;
    }

    return true;
}

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

// DOWNLOAD: server to client.
void download_file(int sock, const std::string& name) {
    std::ifstream file(name, std::ios::binary);

    if (!file) {
        log_message("DOWNLOAD REJECTED: file not found");
        send_u64(sock, 0);
        return;
    }

    file.seekg(0, std::ios::end);
    std::streampos pos = file.tellg();

    if (pos <= 0) {
        log_message("DOWNLOAD REJECTED: empty/unreadable");
        send_u64(sock, 0);
        return;
    }

    uint64_t size = static_cast<uint64_t>(
        static_cast<std::streamoff>(pos)
    );

    // Check the size before transmitting the file.
    if (size > MAX_FILE_SIZE) {
        log_message(
            "DOWNLOAD REJECTED: exceeds 100 MiB limit"
        );
        send_u64(sock, 0);
        return;
    }

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
            std::min<uint64_t>(
                size - sent, BUFFER_SIZE
            )
        );

        file.read(buffer, amount);
        std::streamsize count = file.gcount();

        if (count <= 0)
            return;

        if (EVP_DigestUpdate(
                ctx.get(), buffer,
                static_cast<size_t>(count)) != 1 ||
            !send_all(
                sock, buffer,
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

// UPLOAD: client to server.
void upload_file(int sock, const std::string& name) {
    const std::string path = "uploads/" + name;

    // Prevent overwrites and final-component symlinks.
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
        log_message(
            "UPLOAD REJECTED: existing/inaccessible file"
        );
        return;
    }

    bool success = false;

    do {
        uint64_t size = 0;

        if (!recv_u64(sock, size)) {
            log_message("UPLOAD FAILED: missing size");
            break;
        }

        // Reject oversized uploads before writing content.
        if (size > MAX_FILE_SIZE) {
            log_message(
                "UPLOAD REJECTED: exceeds 100 MiB limit"
            );
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
                    fd, buffer,
                    static_cast<size_t>(n)) ||
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
        unsigned int hash_len = 0;

        if (!recv_all(sock, expected, SHA256_SIZE) ||
            EVP_DigestFinal_ex(
                ctx.get(), actual, &hash_len) != 1 ||
            hash_len != SHA256_SIZE)
            break;

        if (std::memcmp(
                expected, actual, SHA256_SIZE) != 0) {
            log_message("UPLOAD FAILED: checksum mismatch");
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

    unsigned char result = success ? 1 : 0;
    send_all(sock, &result, 1);
}

// Called by each thread-pool worker.
void send_file(int client_socket) {
    auto start = std::chrono::steady_clock::now();

    char operation = 0;
    std::string filename;

    if (!recv_all(client_socket, &operation, 1) ||
        !receive_filename(client_socket, filename)) {
        log_message(
            "Request failed or timed out while reading header"
        );
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
    // Require a real uploads directory.
    struct stat upload_info{};

    if (lstat("uploads", &upload_info) != 0 ||
        !S_ISDIR(upload_info.st_mode)) {
        std::cerr
            << "Error: uploads/ directory is missing\n";
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
    log_message("Maximum file size: 100 MiB");
    log_message("Maximum connections: 8");
    log_message("Socket timeout: 10 seconds");

    ThreadPool pool(WORKER_COUNT, MAX_CONNECTIONS);

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

        if (!set_socket_timeout(client_socket)) {
            perror("setsockopt timeout");
            close(client_socket);
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

        // Reject the connection when all slots are occupied.
        if (!pool.enqueue(client_socket)) {
            log_message(
                "CONNECTION REJECTED: maximum of 8 reached"
            );
            close(client_socket);
        }
    }

    close(server_fd);
    return 0;
}
