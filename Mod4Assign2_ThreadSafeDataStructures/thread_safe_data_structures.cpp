
#include <iostream>
#include <queue>
#include <stack>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <chrono>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> q;
    std::mutex mtx;
    std::condition_variable cv;
    bool finished = false;

public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(value);
        }
        cv.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] {
            return !q.empty() || finished;
        });

        if (q.empty()) return false;

        value = q.front();
        q.pop();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            finished = true;
        }
        cv.notify_all();
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return q.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return q.size();
    }
};

template <typename T>
class ThreadSafeStack {
private:
    std::stack<T> s;
    std::mutex mtx;

public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        s.push(value);
    }

    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (s.empty()) return false;
        value = s.top();
        s.pop();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return s.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return s.size();
    }
};

void producerConsumerProblem() {
    ThreadSafeQueue<std::string> messages;
    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 2;
    const int ITEMS = 5;

    std::mutex outputMutex;
    std::vector<std::thread> producers, consumers;

    auto producer = [&](int id) {
        for (int i = 0; i < ITEMS; ++i) {
            std::string msg = "Producer " +
                std::to_string(id) + " Message " +
                std::to_string(i);

            messages.push(msg);

            {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Produced: " << msg << "\n";
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(50));
        }
    };

    auto consumer = [&](int id) {
        std::string msg;
        while (messages.pop(msg)) {
            {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Consumer " << id
                          << " processed: " << msg << "\n";
            }
        }
    };

    for (int i = 0; i < NUM_CONSUMERS; ++i)
        consumers.emplace_back(consumer, i);

    for (int i = 0; i < NUM_PRODUCERS; ++i)
        producers.emplace_back(producer, i);

    for (auto& t : producers) t.join();

    messages.close();

    for (auto& t : consumers) t.join();

    std::cout << "Producer-Consumer complete.\n";
}

void undoRedoProblem() {
    ThreadSafeStack<std::string> undoStack;
    ThreadSafeStack<std::string> redoStack;
    std::string currentText;

    auto edit = [&](const std::string& text) {
        undoStack.push(currentText);

        std::string discarded;
        while (redoStack.pop(discarded)) {}

        currentText = text;
        std::cout << "Text updated: " << currentText << "\n";
    };

    auto undo = [&]() {
        std::string previous;
        if (undoStack.pop(previous)) {
            redoStack.push(currentText);
            currentText = previous;
            std::cout << "Undo: " << currentText << "\n";
        }
    };

    auto redo = [&]() {
        std::string next;
        if (redoStack.pop(next)) {
            undoStack.push(currentText);
            currentText = next;
            std::cout << "Redo: " << currentText << "\n";
        }
    };

    edit("Hello");
    edit("Hello World");
    edit("Hello Universe");
    undo();
    undo();
    redo();
    edit("Hello Galaxy");
}

int main() {
    std::cout << "Problem 1: Producer-Consumer\n";
    producerConsumerProblem();

    std::cout << "\nProblem 2: Undo-Redo\n";
    undoRedoProblem();

    return 0;
}
