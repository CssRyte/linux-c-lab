
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <queue>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Shared task queue and synchronization objects.
std::queue<int> task_queue;
std::mutex queue_mutex;
std::mutex output_mutex;
std::condition_variable queue_cv;

bool stop_pool = false;

const int NUM_THREADS = 10;

// Worker threads wait for and process client connections.
void worker_thread(int worker_id) {
    while (true) {
        int client_fd;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Sleep until a connection becomes available.
            queue_cv.wait(lock, [] {
                return stop_pool || !task_queue.empty();
            });

            if (stop_pool && task_queue.empty()) {
                return;
            }

            // Remove the next client from the queue.
            client_fd = task_queue.front();
            task_queue.pop();
        }

        {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "Worker " << worker_id
                      << " handling client " << client_fd
                      << std::endl;
        }

        char buffer[1024];

        // Echo data until the client disconnects.
        while (true) {
            ssize_t bytes_read = read(
                client_fd,
                buffer,
                sizeof(buffer)
            );

            if (bytes_read == 0) {
                break;
            }

            if (bytes_read < 0) {
                if (errno == EINTR) {
                    continue;
                }

                perror("read");
                break;
            }

            {
                std::lock_guard<std::mutex> lock(output_mutex);

                std::cout << "Worker " << worker_id
                          << " received: ";

                std::cout.write(buffer, bytes_read);
                std::cout << std::endl;
            }

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

        {
            std::lock_guard<std::mutex> lock(output_mutex);

            std::cout << "Worker " << worker_id
                      << " finished client " << client_fd
                      << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;

    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    // Create a TCP listening socket.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

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

    sockaddr_in addr{};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(
            server_fd,
            (sockaddr*)&addr,
            sizeof(addr)
        ) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 20) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    // Create the worker threads only once.
    std::vector<std::thread> workers;

    for (int i = 0; i < NUM_THREADS; i++) {
        workers.emplace_back(worker_thread, i + 1);
    }

    std::cout << "Thread-pool echo server listening on port "
              << port << std::endl;

    std::cout << "Worker threads: "
              << NUM_THREADS << std::endl;

    // Main thread continuously accepts new clients.
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

        {
            std::lock_guard<std::mutex> lock(queue_mutex);

            task_queue.push(client_fd);

            std::lock_guard<std::mutex> output_lock(output_mutex);

            std::cout << "Client " << client_fd
                      << " queued. Queue size: "
                      << task_queue.size()
                      << std::endl;
        }

        // Wake an available worker.
        queue_cv.notify_one();
    }

    // Cleanup for a future graceful shutdown implementation.
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_pool = true;
    }

    queue_cv.notify_all();

    for (auto& worker : workers) {
        worker.join();
    }

    close(server_fd);

    return 0;
}
