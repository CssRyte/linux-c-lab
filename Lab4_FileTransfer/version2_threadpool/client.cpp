
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <openssl/evp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9000
#define BUFFER_SIZE 4096

constexpr size_t SHA256_SIZE = 32;

// Send all requested bytes.
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

        if (n <= 0)
            return false;

        sent += static_cast<size_t>(n);
    }

    return true;
}

// Receive exactly the requested number of bytes.
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./client <filename>\n";
        return 1;
    }

    std::string filename = argv[1];

    if (filename.empty() || filename.size() >= 256) {
        std::cerr << "Invalid filename\n";
        return 1;
    }

    // Create client socket.
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket");
        return 1;
    }

    // Configure server address.
    sockaddr_in server{};

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(
            AF_INET,
            "127.0.0.1",
            &server.sin_addr) != 1 ||
        connect(
            sock,
            reinterpret_cast<sockaddr*>(&server),
            sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "Connected to server\n";

    // Send requested filename.
    uint32_t net_name_len = htonl(
        static_cast<uint32_t>(filename.size())
    );

    if (!send_all(
            sock,
            &net_name_len,
            sizeof(net_name_len)) ||
        !send_all(
            sock,
            filename.data(),
            filename.size())) {
        std::cerr << "Failed to send filename\n";
        close(sock);
        return 1;
    }

    // Receive file size.
    uint32_t net_size = 0;

    if (!recv_all(
            sock,
            &net_size,
            sizeof(net_size))) {
        std::cerr << "Failed to receive file size\n";
        close(sock);
        return 1;
    }

    uint32_t file_size = ntohl(net_size);

    if (file_size == 0) {
        std::cerr << "File not found or empty\n";
        close(sock);
        return 1;
    }

    std::cout << "File size: "
              << file_size << " bytes\n";

    // Create destination file.
    std::string output_name =
        "received_" + filename;

    std::ofstream output(
        output_name,
        std::ios::binary
    );

    if (!output) {
        std::cerr << "Cannot create destination file\n";
        close(sock);
        return 1;
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
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    uint32_t received = 0;

    // Receive the file contents.
    while (received < file_size) {
        size_t amount = std::min<size_t>(
            file_size - received,
            sizeof(buffer)
        );

        ssize_t count = recv(
            sock,
            buffer,
            amount,
            0
        );

        if (count < 0 && errno == EINTR)
            continue;

        if (count <= 0) {
            std::cerr << "Transfer interrupted\n";
            close(sock);
            return 1;
        }

        // Write received bytes.
        output.write(buffer, count);

        // Include received bytes in SHA-256 hash.
        if (!output ||
            EVP_DigestUpdate(
                ctx.get(),
                buffer,
                static_cast<size_t>(count)) != 1) {
            std::cerr << "Failed to write or hash file\n";
            close(sock);
            return 1;
        }

        received += static_cast<uint32_t>(count);
    }

    // Receive checksum from the server.
    unsigned char expected_digest[SHA256_SIZE];

    if (!recv_all(
            sock,
            expected_digest,
            SHA256_SIZE)) {
        std::cerr << "Failed to receive SHA-256 checksum\n";
        close(sock);
        return 1;
    }

    // Calculate checksum of received file data.
    unsigned char actual_digest[SHA256_SIZE];
    unsigned int actual_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(),
            actual_digest,
            &actual_len) != 1 ||
        actual_len != SHA256_SIZE) {
        std::cerr << "Failed to calculate SHA-256\n";
        close(sock);
        return 1;
    }

    output.close();
    close(sock);

    // Compare server and client checksums.
    if (std::memcmp(
            expected_digest,
            actual_digest,
            SHA256_SIZE) != 0) {
        std::cerr
            << "SHA-256 verification: FAIL "
            << "(checksum mismatch)\n";

        return 1;
    }

    // Successful transfer.
    std::cout << "File received successfully: "
              << output_name << '\n';

    std::cout << "Total bytes received: "
              << received << '\n';

    std::cout << "SHA-256 verification: PASS\n";

    return 0;
}
