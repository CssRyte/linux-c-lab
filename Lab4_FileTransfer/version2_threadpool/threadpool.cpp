
#include "threadpool.h"

#include <iostream>
#include <chrono>
#include <sstream>
#include <unistd.h>

// Implemented in server.cpp.
extern void send_file(int client_socket);

ThreadPool::ThreadPool(size_t threads,
                       size_t max_clients)
    : max_connections(max_clients) {

    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }

    std::lock_guard<std::mutex> lock(metrics_mutex);

    std::cout << "Thread pool created with "
              << threads << " worker threads\n"
              << "Maximum accepted connections: "
              << max_connections << '\n';
}

bool ThreadPool::enqueue(int client_socket) {
    size_t queue_length;

    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        if (stop ||
            outstanding_connections >= max_connections) {
            return false;
        }

        // Reserve a connection slot BEFORE queuing.
        ++outstanding_connections;
        tasks.push(client_socket);
        queue_length = tasks.size();
    }

    {
        std::lock_guard<std::mutex> lock(metrics_mutex);

        std::cout << "[QUEUE] Client accepted"
                  << " | Queue length: " << queue_length
                  << " | Active workers: "
                  << active_workers.load() << '\n';
    }

    condition.notify_one();
    return true;
}

void ThreadPool::worker() {
    while (true) {
        int client_socket;
        size_t queue_length;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            condition.wait(lock, [this] {
                return stop || !tasks.empty();
            });

            if (stop && tasks.empty())
                return;

            client_socket = tasks.front();
            tasks.pop();

            queue_length = tasks.size();
            ++active_workers;
        }

        {
            std::ostringstream output;

            output << "[WORKER] Thread "
                   << std::this_thread::get_id()
                   << " handling client"
                   << " | Active workers: "
                   << active_workers.load()
                   << " | Queue length: "
                   << queue_length;

            std::lock_guard<std::mutex> lock(metrics_mutex);
            std::cout << output.str() << '\n';
        }

        auto start = std::chrono::steady_clock::now();

        send_file(client_socket);
        close(client_socket);

        auto finish = std::chrono::steady_clock::now();

        auto elapsed =
            std::chrono::duration_cast<
                std::chrono::microseconds
            >(finish - start);

        total_service_time_us.fetch_add(
            static_cast<uint64_t>(elapsed.count())
        );

        ++completed_tasks;
        --active_workers;

        // Release the connection slot when work finishes.
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            --outstanding_connections;
        }

        print_metrics();
    }
}

void ThreadPool::print_metrics() {
    size_t queue_length;
    size_t outstanding;

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue_length = tasks.size();
        outstanding = outstanding_connections;
    }

    uint64_t completed = completed_tasks.load();
    uint64_t total_time = total_service_time_us.load();

    double average_ms = completed == 0
        ? 0.0
        : static_cast<double>(total_time) /
          static_cast<double>(completed) / 1000.0;

    std::lock_guard<std::mutex> lock(metrics_mutex);

    std::cout
        << "\n========== THREAD POOL METRICS ==========\n"
        << "Active workers: "
        << active_workers.load() << '\n'
        << "Queue length: "
        << queue_length << '\n'
        << "Outstanding connections: "
        << outstanding << '\n'
        << "Completed requests: "
        << completed << '\n'
        << "Average service time: "
        << average_ms << " ms\n"
        << "=========================================\n\n";
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();

    for (std::thread& thread : workers) {
        if (thread.joinable())
            thread.join();
    }

    print_metrics();
}
