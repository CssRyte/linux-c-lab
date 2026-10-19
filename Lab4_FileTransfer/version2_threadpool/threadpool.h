
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <cstddef>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads,
                        size_t max_connections = 8);
    ~ThreadPool();

    // Returns false when the connection limit is reached.
    bool enqueue(int client_socket);

private:
    void worker();
    void print_metrics();

    std::vector<std::thread> workers;
    std::queue<int> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::mutex metrics_mutex;

    const size_t max_connections;

    // Protected by queue_mutex.
    size_t outstanding_connections = 0;
    bool stop = false;

    std::atomic<size_t> active_workers{0};
    std::atomic<uint64_t> completed_tasks{0};
    std::atomic<uint64_t> total_service_time_us{0};
};

#endif
