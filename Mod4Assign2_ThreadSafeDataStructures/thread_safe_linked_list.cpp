
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

template <typename T>
class ThreadSafeLinkedList {
private:
    struct Node {
        T data;
        Node* next;

        Node(T value) : data(value), next(nullptr) {}
    };

    Node* head = nullptr;
    std::mutex mtx;

public:
    ~ThreadSafeLinkedList() {
        Node* current = head;

        while (current != nullptr) {
            Node* next = current->next;
            delete current;
            current = next;
        }
    }

    void push_front(T value) {
        std::lock_guard<std::mutex> lock(mtx);

        Node* newNode = new Node(value);
        newNode->next = head;
        head = newNode;
    }

    bool pop_front(T& value) {
        std::lock_guard<std::mutex> lock(mtx);

        if (head == nullptr) return false;

        Node* temp = head;
        value = temp->data;
        head = head->next;

        delete temp;
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return head == nullptr;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);

        size_t count = 0;
        Node* current = head;

        while (current != nullptr) {
            ++count;
            current = current->next;
        }

        return count;
    }
};

int main() {
    ThreadSafeLinkedList<int> list;
    std::vector<std::thread> threads;
    std::mutex outputMutex;

    // Four threads insert five nodes each.
    auto inserter = [&](int id) {
        for (int i = 0; i < 5; ++i) {
            int value = id * 100 + i;
            list.push_front(value);

            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "Thread " << id
                      << " inserted: " << value << "\n";
        }
    };

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(inserter, i);
    }

    for (auto& t : threads) t.join();
    threads.clear();

    std::cout << "\nSize after insertion: "
              << list.size() << "\n";

    // Four threads remove five nodes each.
    auto remover = [&]() {
        for (int i = 0; i < 5; ++i) {
            int value;

            if (list.pop_front(value)) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Removed: " << value << "\n";
            }
        }
    };

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(remover);
    }

    for (auto& t : threads) t.join();

    std::cout << "\nFinal size: " << list.size() << "\n";
    std::cout << "Linked list test complete.\n";

    return 0;
}
