
#include "threadpool.h"
#include <iostream>
#include <unistd.h>

// File-transfer function defined in server.cpp
extern void send_file(int client_socket);

ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }

    std::cout << "Thread pool created with "
              << threads << " worker threads\n";
}

void ThreadPool::enqueue(int client_socket) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        if (stop) {
            close(client_socket);
            return;
        }

        tasks.push(client_socket);
    }

    // Wake one available worker.
    condition.notify_one();
}

void ThreadPool::worker() {
    while (true) {
        int client_socket;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Sleep until a task arrives or shutdown begins.
            condition.wait(lock, [this] {
                return stop || !tasks.empty();
            });

            if (stop && tasks.empty()) {
                return;
            }

            client_socket = tasks.front();
            tasks.pop();
        }

        std::cout << "Worker "
                  << std::this_thread::get_id()
                  << " handling client\n";

        // Handle the file transfer.
        send_file(client_socket);
        close(client_socket);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}
