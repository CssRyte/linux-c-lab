
#include "common.h"
#include <climits>

int main() {
    vector<Process> processes = {
        {"P1", 0, 8, 2},
        {"P2", 1, 4, 1},
        {"P3", 2, 9, 3},
        {"P4", 3, 5, 4}
    };

    int n = processes.size();
    int current_time = 0;
    int completed_count = 0;

    vector<bool> completed(n, false);
    vector<pair<string, int>> gantt;

    while (completed_count < n) {
        int selected = -1;

        // Select the highest-priority available process
        for (int i = 0; i < n; i++) {
            if (completed[i] ||
                processes[i].arrival_time > current_time) {
                continue;
            }

            if (selected == -1 ||
                processes[i].priority < processes[selected].priority ||
                (processes[i].priority == processes[selected].priority &&
                 processes[i].arrival_time <
                 processes[selected].arrival_time)) {

                selected = i;
            }
        }

        // Advance to the next arrival if CPU is idle
        if (selected == -1) {
            int next_arrival = INT_MAX;

            for (int i = 0; i < n; i++) {
                if (!completed[i]) {
                    next_arrival = min(
                        next_arrival,
                        processes[i].arrival_time
                    );
                }
            }

            gantt.push_back({
                "Idle",
                next_arrival - current_time
            });

            current_time = next_arrival;
            continue;
        }

        Process& p = processes[selected];

        // Non-preemptive: run until completion
        p.waiting_time = current_time - p.arrival_time;

        gantt.push_back({p.id, p.burst_time});
        current_time += p.burst_time;

        p.turnaround_time = current_time - p.arrival_time;

        completed[selected] = true;
        completed_count++;
    }

    cout << "Priority Scheduling Results\n";
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
