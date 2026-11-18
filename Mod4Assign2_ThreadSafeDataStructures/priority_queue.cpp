
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>

template <typename T>
class ThreadSafePriorityQueue {
private:
    std::priority_queue<T> pq;
    std::mutex mtx;

public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        pq.push(value);
    }

    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx);

        if (pq.empty()) {
            return false;
        }

        value = pq.top();
        pq.pop();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return pq.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return pq.size();
    }
};

int main() {
    ThreadSafePriorityQueue<int> pq;
    std::vector<std::thread> threads;
    std::mutex outputMutex;

    // Four threads insert values concurrently.
    auto pusher = [&](int id) {
        for (int i = 0; i < 5; ++i) {
            int priority = id * 100 + i;
            pq.push(priority);

            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "Thread " << id
                      << " pushed: " << priority << "\n";
        }
    };

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(pusher, i);
    }

    // Wait until all insertions finish.
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\nTotal items: " << pq.size() << "\n";
    std::cout << "\nPopping by priority:\n";

    int value;
    while (pq.pop(value)) {
        std::cout << "Popped: " << value << "\n";
    }

    std::cout << "\nFinal size: " << pq.size() << "\n";
    std::cout << "Priority queue test complete.\n";

    return 0;
}
