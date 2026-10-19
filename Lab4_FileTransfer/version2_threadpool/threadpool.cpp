
#include "threadpool.h"

#include <iostream>
#include <chrono>
#include <sstream>
#include <unistd.h>

// File-transfer function defined in server.cpp.
extern void send_file(int client_socket);

ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }

    std::lock_guard<std::mutex> lock(metrics_mutex);

    std::cout << "Thread pool created with "
              << threads
              << " worker threads\n";
}

void ThreadPool::enqueue(int client_socket) {
    size_t queue_length = 0;

    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        if (stop) {
            close(client_socket);
            return;
        }

        tasks.push(client_socket);
        queue_length = tasks.size();
    }

    {
        std::lock_guard<std::mutex> lock(metrics_mutex);

        std::cout << "[QUEUE] New client enqueued"
                  << " | Queue length: "
                  << queue_length
                  << " | Active workers: "
                  << active_workers.load()
                  << '\n';
    }

    // Wake one available worker.
    condition.notify_one();
}

void ThreadPool::worker() {
    while (true) {
        int client_socket;
        size_t queue_length = 0;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Wait until a client arrives or shutdown begins.
            condition.wait(lock, [this] {
                return stop || !tasks.empty();
            });

            if (stop && tasks.empty()) {
                return;
            }

            client_socket = tasks.front();
            tasks.pop();

            queue_length = tasks.size();

            // Record the active worker before releasing
            // the queue lock.
            ++active_workers;
        }

        {
            std::ostringstream message;

            message << "[WORKER] Thread "
                    << std::this_thread::get_id()
                    << " started client"
                    << " | Active workers: "
                    << active_workers.load()
                    << " | Queue length: "
                    << queue_length;

            std::lock_guard<std::mutex> lock(metrics_mutex);
            std::cout << message.str() << '\n';
        }

        // Measure the time spent servicing this client.
        auto start = std::chrono::steady_clock::now();

        send_file(client_socket);
        close(client_socket);

        auto end = std::chrono::steady_clock::now();

        auto duration =
            std::chrono::duration_cast<
                std::chrono::microseconds
            >(end - start);

        uint64_t service_time_us =
            static_cast<uint64_t>(duration.count());

        // Update cumulative statistics.
        total_service_time_us.fetch_add(service_time_us);

        completed_tasks.fetch_add(1);

        active_workers.fetch_sub(1);

        print_metrics();
    }
}

void ThreadPool::print_metrics() {
    uint64_t completed = completed_tasks.load();
    uint64_t total_us = total_service_time_us.load();

    double average_ms = 0.0;

    if (completed > 0) {
        average_ms =
            static_cast<double>(total_us) /
            static_cast<double>(completed) /
            1000.0;
    }

    size_t queue_length = 0;

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue_length = tasks.size();
    }

    std::lock_guard<std::mutex> lock(metrics_mutex);

    std::cout << "\n========== THREAD POOL METRICS ==========\n"
              << "Active workers: "
              << active_workers.load() << '\n'
              << "Queue length: "
              << queue_length << '\n'
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

    // Wake all sleeping workers.
    condition.notify_all();

    // Wait for worker threads to finish.
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    print_metrics();
}
