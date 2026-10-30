
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// This function executes in a separate thread for each client.
void* handle_client(void* arg) {
    int client_fd = *static_cast<int*>(arg);

    // Free the memory allocated in the main thread.
    delete static_cast<int*>(arg);

    char buffer[1024];

    std::cout << "Thread started for client "
              << client_fd << std::endl;

    // Continue receiving data until the client disconnects.
    while (true) {
        ssize_t bytes_read = read(
            client_fd,
            buffer,
            sizeof(buffer)
        );

        if (bytes_read == 0) {
            std::cout << "Client disconnected (EOF)"
                      << std::endl;
            break;
        }

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("read");
            break;
        }

        std::cout << "Received: ";
        std::cout.write(buffer, bytes_read);
        std::cout << std::endl;

        // Send all received bytes back to the client.
        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t written = send(
                client_fd,
                buffer + total_written,
                bytes_read - total_written,
                MSG_NOSIGNAL
            );

            if (written < 0 && errno == EINTR) {
                continue;
            }

            if (written <= 0) {
                perror("send");
                break;
            }

            total_written += written;
        }

        if (total_written < bytes_read) {
            break;
        }
    }

    close(client_fd);

    std::cout << "Thread finished for client "
              << client_fd << std::endl;

    return nullptr;
}

int main(int argc, char* argv[]) {
    int port = 8080;

    // Allow the port to be specified as a command-line argument.
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    // Create the listening TCP socket.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Allow the server to reuse the port after restarting.
    int opt = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    // Configure the server address.
    sockaddr_in addr{};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the server to the specified port.
    if (bind(
            server_fd,
            (sockaddr*)&addr,
            sizeof(addr)
        ) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Begin listening for incoming connections.
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Multi-threaded echo server listening on port "
              << port << std::endl;

    // The main thread continuously accepts new clients.
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (sockaddr*)&client_addr,
            &client_len
        );

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        std::cout << "Client connected: "
                  << client_fd << std::endl;

        // Give each thread its own copy of the descriptor.
        int* client_ptr = new int(client_fd);

        pthread_t thread;

        // Create a new thread to handle the client.
        int result = pthread_create(
            &thread,
            nullptr,
            handle_client,
            client_ptr
        );

        if (result != 0) {
            errno = result;
            perror("pthread_create");

            delete client_ptr;
            close(client_fd);
            continue;
        }

        // Automatically reclaim thread resources on termination.
        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}
