
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

bool send_u64(int sock, uint64_t value) {
    unsigned char bytes[8];

    for (int i = 7; i >= 0; --i) {
        bytes[i] = static_cast<unsigned char>(value & 255);
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

bool request_filename(int sock,
                      char operation,
                      const std::string& name) {
    uint32_t len = htonl(
        static_cast<uint32_t>(name.size())
    );

    return send_all(sock, &operation, 1) &&
           send_all(sock, &len, sizeof(len)) &&
           send_all(sock, name.data(), name.size());
}

// Download from server.
bool download_file(int sock, const std::string& name) {
    if (!request_filename(sock, 'D', name))
        return false;

    uint64_t size = 0;

    if (!recv_u64(sock, size))
        return false;

    if (size == 0) {
        std::cerr << "File not found or empty\n";
        return false;
    }

    std::cout << "File size: " << size << " bytes\n";

    std::string output_name = "received_" + name;
    std::ofstream output(output_name, std::ios::binary);

    if (!output)
        return false;

    using Digest = std::unique_ptr<
        EVP_MD_CTX, decltype(&EVP_MD_CTX_free)
    >;

    Digest ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    bool success = false;

    if (ctx &&
        EVP_DigestInit_ex(
            ctx.get(), EVP_sha256(), nullptr) == 1) {

        char buffer[BUFFER_SIZE];
        uint64_t received = 0;
        bool okay = true;

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
                okay = false;
                break;
            }

            output.write(buffer, n);

            if (!output ||
                EVP_DigestUpdate(
                    ctx.get(), buffer,
                    static_cast<size_t>(n)) != 1) {
                okay = false;
                break;
            }

            received += static_cast<uint64_t>(n);
        }

        if (okay && received == size) {
            unsigned char expected[SHA256_SIZE];
            unsigned char actual[SHA256_SIZE];
            unsigned int actual_len = 0;

            if (recv_all(sock, expected, SHA256_SIZE) &&
                EVP_DigestFinal_ex(
                    ctx.get(), actual, &actual_len) == 1 &&
                actual_len == SHA256_SIZE &&
                std::memcmp(
                    expected, actual, SHA256_SIZE) == 0) {
                success = true;
            }
        }

        if (success) {
            std::cout << "Total bytes received: "
                      << received << '\n';
        }
    }

    output.close();

    if (!output)
        success = false;

    if (!success) {
        std::remove(output_name.c_str());
        std::cerr << "Download verification: FAIL\n";
        return false;
    }

    std::cout << "File received successfully: "
              << output_name << '\n';
    std::cout << "SHA-256 verification: PASS\n";

    return true;
}

// Upload from client.
bool upload_file(int sock, const std::string& name) {
    std::ifstream file(name, std::ios::binary);

    if (!file) {
        std::cerr << "Cannot open upload file\n";
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streampos pos = file.tellg();

    if (pos < 0)
        return false;

    uint64_t size = static_cast<uint64_t>(
        static_cast<std::streamoff>(pos)
    );

    file.seekg(0, std::ios::beg);

    if (!file || !request_filename(sock, 'U', name))
        return false;

    // Wait for the server to reserve the filename.
    unsigned char ready = 0;

    if (!recv_all(sock, &ready, 1) || ready != 1) {
        std::cerr << "Server rejected upload\n";
        return false;
    }

    if (!send_u64(sock, size))
        return false;

    using Digest = std::unique_ptr<
        EVP_MD_CTX, decltype(&EVP_MD_CTX_free)
    >;

    Digest ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    if (!ctx ||
        EVP_DigestInit_ex(
            ctx.get(), EVP_sha256(), nullptr) != 1)
        return false;

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
            return false;

        if (EVP_DigestUpdate(
                ctx.get(), buffer,
                static_cast<size_t>(count)) != 1 ||
            !send_all(
                sock, buffer,
                static_cast<size_t>(count)))
            return false;

        sent += static_cast<uint64_t>(count);
    }

    unsigned char hash[SHA256_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestFinal_ex(
            ctx.get(), hash, &hash_len) != 1 ||
        hash_len != SHA256_SIZE ||
        !send_all(sock, hash, SHA256_SIZE))
        return false;

    // Wait for the server's verification result.
    unsigned char result = 0;

    if (!recv_all(sock, &result, 1) || result != 1) {
        std::cerr << "Server upload verification: FAIL\n";
        return false;
    }

    std::cout << "File uploaded successfully: "
              << name << '\n';
    std::cout << "Total bytes uploaded: "
              << sent << '\n';
    std::cout << "SHA-256 verification: PASS\n";

    return true;
}

int main(int argc, char* argv[]) {
    bool upload = false;
    std::string filename;

    if (argc == 2) {
        // Keep the old download syntax.
        filename = argv[1];
    } else if (argc == 3 &&
               std::string(argv[1]) == "upload") {
        upload = true;
        filename = argv[2];
    } else {
        std::cerr
            << "Usage:\n"
            << "  ./client <filename>          Download\n"
            << "  ./client upload <filename>   Upload\n";
        return 1;
    }

    if (!valid_filename(filename)) {
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
            AF_INET, "127.0.0.1",
            &server.sin_addr) != 1) {
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

    bool success = upload
        ? upload_file(sock, filename)
        : download_file(sock, filename);

    close(sock);

    return success ? 0 : 1;
}
