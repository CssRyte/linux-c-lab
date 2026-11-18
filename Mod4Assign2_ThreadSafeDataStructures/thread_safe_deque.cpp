
#include <iostream>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

template <typename T>
class ThreadSafeDeque {
private:
    std::deque<T> dq;
    std::mutex mtx;

public:
    void push_front(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        dq.push_front(value);
    }

    void push_back(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        dq.push_back(value);
    }

    bool pop_front(T& value) {
        std::lock_guard<std::mutex> lock(mtx);

        if (dq.empty()) return false;

        value = dq.front();
        dq.pop_front();
        return true;
    }

    bool pop_back(T& value) {
        std::lock_guard<std::mutex> lock(mtx);

        if (dq.empty()) return false;

        value = dq.back();
        dq.pop_back();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return dq.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return dq.size();
    }
};

int main() {
    ThreadSafeDeque<int> dq;
    std::vector<std::thread> threads;
    std::mutex outputMutex;

    auto pushFront = [&]() {
        for (int i = 0; i < 5; ++i) {
            dq.push_front(i);

            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "Pushed front: " << i << "\n";
        }
    };

    auto pushBack = [&]() {
        for (int i = 100; i < 105; ++i) {
            dq.push_back(i);

            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "Pushed back: " << i << "\n";
        }
    };

    threads.emplace_back(pushFront);
    threads.emplace_back(pushBack);

    for (auto& t : threads) t.join();
    threads.clear();

    std::cout << "\nSize after insertion: "
              << dq.size() << "\n";

    auto popFront = [&]() {
        int value;
        for (int i = 0; i < 5; ++i) {
            if (dq.pop_front(value)) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Popped front: " << value << "\n";
            }
        }
    };

    auto popBack = [&]() {
        int value;
        for (int i = 0; i < 5; ++i) {
            if (dq.pop_back(value)) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Popped back: " << value << "\n";
            }
        }
    };

    threads.emplace_back(popFront);
    threads.emplace_back(popBack);

    for (auto& t : threads) t.join();

    std::cout << "\nFinal size: " << dq.size() << "\n";
    std::cout << "Deque test complete.\n";

    return 0;
}
