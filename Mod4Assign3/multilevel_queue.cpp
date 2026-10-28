
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

    queue<int> high_queue;
    queue<int> low_queue;

    vector<bool> added(n, false);
    vector<pair<string, int>> gantt;

    int high_current = -1;
    int high_slice = 0;
    int low_current = -1;

    for (auto& p : processes) {
        p.remaining_time = p.burst_time;
    }

    while (completed < n) {

        // Add processes that have arrived
        for (int i = 0; i < n; i++) {
            if (!added[i] &&
                processes[i].arrival_time <= current_time) {

                if (processes[i].priority < 3) {
                    high_queue.push(i);
                } else {
                    low_queue.push(i);
                }

                added[i] = true;
            }
        }

        int selected = -1;

        // High queue always has scheduling priority
        if (high_current != -1 || !high_queue.empty()) {

            if (high_current == -1) {
                high_current = high_queue.front();
                high_queue.pop();
                high_slice = 0;
            }

            selected = high_current;

        } else {

            // Low-priority FCFS queue
            if (low_current == -1 &&
                !low_queue.empty()) {

                low_current = low_queue.front();
                low_queue.pop();
            }

            selected = low_current;
        }

        // CPU is idle if no process has arrived
        if (selected == -1) {
            if (!gantt.empty() &&
                gantt.back().first == "Idle") {
                gantt.back().second++;
            } else {
                gantt.push_back({"Idle", 1});
            }

            current_time++;
            continue;
        }

        Process& p = processes[selected];

        // Record one unit of execution
        if (!gantt.empty() &&
            gantt.back().first == p.id) {
            gantt.back().second++;
        } else {
            gantt.push_back({p.id, 1});
        }

        p.remaining_time--;
        current_time++;

        if (selected == high_current) {
            high_slice++;
        }

        // Process has finished
        if (p.remaining_time == 0) {
            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;

            if (selected == high_current) {
                high_current = -1;
                high_slice = 0;
            } else {
                low_current = -1;
            }

        } else if (selected == high_current &&
                   high_slice == quantum) {

            // Requeue high-priority process after quantum
            // Newly arrived processes will join on next tick.
            high_queue.push(high_current);
            high_current = -1;
            high_slice = 0;
        }
    }

    cout << "Multilevel Queue Scheduling Results\n";
    cout << "High Queue: Round Robin (Quantum = 4)\n";
    cout << "Low Queue: FCFS\n\n";

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
