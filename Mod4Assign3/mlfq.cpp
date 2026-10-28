
#include "common.h"
#include <queue>

int main() {
    vector<Process> processes = {
        {"P1", 0, 8, 2},
        {"P2", 1, 4, 1},
        {"P3", 2, 9, 3},
        {"P4", 3, 5, 4}
    };

    const int quantum[] = {2, 4};
    const int n = processes.size();

    queue<int> queues[3];
    vector<bool> added(n, false);
    vector<pair<string, int>> gantt;

    int current_time = 0;
    int completed = 0;

    for (auto& p : processes) {
        p.remaining_time = p.burst_time;
    }

    // Every new process starts at the highest level
    auto addArrivals = [&]() {
        for (int i = 0; i < n; i++) {
            if (!added[i] &&
                processes[i].arrival_time <= current_time) {
                queues[0].push(i);
                added[i] = true;
            }
        }
    };

    // Record execution and merge adjacent slices
    auto record = [&](const string& id, int duration) {
        if (!gantt.empty() && gantt.back().first == id) {
            gantt.back().second += duration;
        } else {
            gantt.push_back({id, duration});
        }
    };

    while (completed < n) {
        addArrivals();

        // Pick the highest non-empty queue
        int level = -1;

        for (int i = 0; i < 3; i++) {
            if (!queues[i].empty()) {
                level = i;
                break;
            }
        }

        if (level == -1) {
            record("Idle", 1);
            current_time++;
            continue;
        }

        int index = queues[level].front();
        queues[level].pop();

        Process& p = processes[index];

        int time_limit = (level == 2)
            ? p.remaining_time
            : quantum[level];

        int used = 0;
        bool preempted = false;

        while (p.remaining_time > 0 && used < time_limit) {
            p.remaining_time--;
            current_time++;
            used++;

            record(p.id, 1);
            addArrivals();

            // Higher-priority arrivals interrupt lower queues
            if (level > 0 && !queues[0].empty()) {
                preempted = true;
                break;
            }

            if (level == 2 && !queues[1].empty()) {
                preempted = true;
                break;
            }
        }

        if (p.remaining_time == 0) {
            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;
        }
        else if (preempted) {
            // Return to the same queue if interrupted
            queues[level].push(index);
        }
        else if (level < 2) {
            // Full quantum consumed: demote one level
            queues[level + 1].push(index);
        }
        else {
            queues[2].push(index);
        }
    }

    cout << "MLFQ Scheduling Results\n";
    cout << "Q0: RR (Quantum = 2)\n";
    cout << "Q1: RR (Quantum = 4)\n";
    cout << "Q2: FCFS\n\n";

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
