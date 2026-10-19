
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>
#include <cstdint>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    void enqueue(int client_socket);

private:
    void worker();
    void print_metrics();

    std::vector<std::thread> workers;
    std::queue<int> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;

    // Protect metrics output.
    std::mutex metrics_mutex;

    // Number of workers currently handling clients.
    std::atomic<size_t> active_workers{0};

    // Completed client requests.
    std::atomic<uint64_t> completed_tasks{0};

    // Total time spent servicing client requests.
    std::atomic<uint64_t> total_service_time_us{0};

    bool stop = false;
};

#endif
