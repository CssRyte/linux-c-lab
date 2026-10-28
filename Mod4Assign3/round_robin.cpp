
#include "common.h"
#include <queue>
#include <climits>

int main() {
    vector<Process> processes = {
        {"P1", 0, 8, 2},
        {"P2", 1, 4, 1},
        {"P3", 2, 9, 3},
        {"P4", 3, 5, 4}
    };

    const int quantum = 4;
    int n = processes.size();
    int current_time = 0;
    int completed = 0;

    queue<int> ready_queue;
    vector<bool> added(n, false);
    vector<pair<string, int>> gantt;

    for (auto& p : processes) {
        p.remaining_time = p.burst_time;
    }

    // Add newly arrived processes to ready queue
    auto addArrivals = [&]() {
        vector<int> arrivals;

        for (int i = 0; i < n; i++) {
            if (!added[i] &&
                processes[i].arrival_time <= current_time) {
                arrivals.push_back(i);
            }
        }

        stable_sort(arrivals.begin(), arrivals.end(),
            [&](int a, int b) {
                return processes[a].arrival_time <
                       processes[b].arrival_time;
            });

        for (int i : arrivals) {
            ready_queue.push(i);
            added[i] = true;
        }
    };

    addArrivals();

    while (completed < n) {
        if (ready_queue.empty()) {
            int next_arrival = INT_MAX;

            for (int i = 0; i < n; i++) {
                if (!added[i]) {
                    next_arrival = min(
                        next_arrival,
                        processes[i].arrival_time
                    );
                }
            }

            if (next_arrival == INT_MAX) break;

            gantt.push_back({
                "Idle", next_arrival - current_time
            });

            current_time = next_arrival;
            addArrivals();
            continue;
        }

        int index = ready_queue.front();
        ready_queue.pop();

        Process& p = processes[index];

        int run_time = min(quantum, p.remaining_time);

        gantt.push_back({p.id, run_time});

        p.remaining_time -= run_time;
        current_time += run_time;

        // New arrivals join before the running process
        // is returned to the back of the queue.
        addArrivals();

        if (p.remaining_time == 0) {
            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;
        } else {
            ready_queue.push(index);
        }
    }

    cout << "Round Robin Scheduling Results\n";
    cout << "Time Quantum: " << quantum << "\n";
    cout << "Process\tWaiting\tTurnaround\n";

    for (const auto& p : processes) {
        cout << p.id << "\t"
             << p.waiting_time << "\t"
             << p.turnaround_time << "\n";
    }

    calculateMetrics(processes, current_time);
    printGantt(gantt);

    return 0;
}
