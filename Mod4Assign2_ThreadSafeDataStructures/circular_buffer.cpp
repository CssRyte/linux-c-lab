
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

#define BUFFER_SIZE 5

class ThreadSafeCircularBuffer {
private:
    int buffer[BUFFER_SIZE];
    int in = 0;
    int out = 0;
    int count = 0;

    std::mutex mtx;
    std::condition_variable not_full;
    std::condition_variable not_empty;

public:
    void push(int value) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait until there is room in the buffer.
        not_full.wait(lock, [this] {
            return count < BUFFER_SIZE;
        });

        buffer[in] = value;
        in = (in + 1) % BUFFER_SIZE;
        ++count;

        lock.unlock();
        not_empty.notify_one();
    }

    bool pop(int& value) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait until an item is available.
        not_empty.wait(lock, [this] {
            return count > 0;
        });

        value = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        --count;

        lock.unlock();
        not_full.notify_one();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return count == 0;
    }

    bool full() {
        std::lock_guard<std::mutex> lock(mtx);
        return count == BUFFER_SIZE;
    }
};

int main() {
    ThreadSafeCircularBuffer cb;
    std::mutex outputMutex;

    const int NUM_ITEMS = 10;

    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            int value = i * 10;
            cb.push(value);

            {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Produced: " << value << "\n";
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100));
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            int value;

            if (cb.pop(value)) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Consumed: " << value << "\n";
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(150));
        }
    });

    producer.join();
    consumer.join();

    std::cout << "Circular buffer test complete.\n";
    return 0;
}
