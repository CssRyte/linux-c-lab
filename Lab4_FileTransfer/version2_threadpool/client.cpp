
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

constexpr int PORT = 9000;
constexpr size_t BUFFER_SIZE = 4096;
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

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
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
            sock,
            bytes + received,
            length - received,
            0
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

// Read an 8-byte unsigned integer in network byte order.
bool recv_u64(int sock, uint64_t& value) {
    unsigned char bytes[8];

    if (!recv_all(sock, bytes, sizeof(bytes))) {
        return false;
    }

    value = 0;

    for (unsigned char byte : bytes) {
        value = (value << 8) |
                static_cast<uint64_t>(byte);
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./client <filename>\n";
        return 1;
    }

    std::string filename = argv[1];

    if (filename.empty() || filename.size() >= 256 ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos) {
        std::cerr << "Invalid filename\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(
            AF_INET,
            "127.0.0.1",
            &server.sin_addr) != 1) {
        std::cerr << "Invalid server address\n";
        close(sock);
        return 1;
    }

    if (connect(
            sock,
            reinterpret_cast<sockaddr*>(&server),
            sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "Connected to server\n";

    // Request the filename.
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
        std::cerr << "Failed to request file\n";
        close(sock);
        return 1;
    }

    // Read the 64-bit file size.
    uint64_t file_size = 0;

    if (!recv_u64(sock, file_size)) {
        std::cerr << "Failed to receive file size\n";
        close(sock);
        return 1;
    }

    if (file_size == 0) {
        std::cerr << "File not found or empty\n";
        close(sock);
        return 1;
    }

    std::cout << "File size: "
              << file_size << " bytes\n";

    std::string output_name =
        "received_" + filename;

    std::ofstream output(
        output_name,
        std::ios::binary
    );

    if (!output) {
        std::cerr << "Cannot create output file\n";
        close(sock);
        return 1;
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
        std::cerr << "SHA-256 initialization failed\n";
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    uint64_t total_received = 0;

    while (total_received < file_size) {
        size_t amount = static_cast<size_t>(
            std::min<uint64_t>(
                file_size - total_received,
                BUFFER_SIZE
            )
        );

        ssize_t count = recv(sock, buffer, amount, 0);

        if (count < 0 && errno == EINTR) {
            continue;
        }

        if (count <= 0) {
            std::cerr << "Transfer interrupted\n";
            close(sock);
            return 1;
        }

        output.write(buffer, count);

        if (!output ||
            EVP_DigestUpdate(
                ctx.get(),
                buffer,
                static_cast<size_t>(count)) != 1) {
            std::cerr << "File write or SHA-256 failed\n";
            close(sock);
            return 1;
        }

        total_received += static_cast<uint64_t>(count);
    }

    // Receive the server's SHA-256 checksum.
    unsigned char expected_digest[SHA256_SIZE];

    if (!recv_all(
            sock,
            expected_digest,
            SHA256_SIZE)) {
        std::cerr << "Failed to receive checksum\n";
        close(sock);
        return 1;
    }

    unsigned char actual_digest[SHA256_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(),
            actual_digest,
            &digest_len) != 1 ||
        digest_len != SHA256_SIZE) {
        std::cerr << "SHA-256 calculation failed\n";
        close(sock);
        return 1;
    }

    output.close();
    close(sock);

    if (!output) {
        std::cerr << "Failed to finish writing file\n";
        return 1;
    }

    if (std::memcmp(
            expected_digest,
            actual_digest,
            SHA256_SIZE) != 0) {
        std::cerr << "SHA-256 verification: FAIL\n";
        return 1;
    }

    std::cout << "File received successfully: "
              << output_name << '\n';

    std::cout << "Total bytes received: "
              << total_received << '\n';

    std::cout << "SHA-256 verification: PASS\n";

    return 0;
}
