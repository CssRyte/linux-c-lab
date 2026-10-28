
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
    vector<bool> completed(n, false);
    vector<pair<string, int>> gantt;

    int current_time = 0;
    int completed_count = 0;

    while (completed_count < n) {
        int shortest_index = -1;
        int shortest_burst = INT_MAX;

        // Find shortest available job
        for (int i = 0; i < n; i++) {
            if (!completed[i] &&
                processes[i].arrival_time <= current_time &&
                processes[i].burst_time < shortest_burst) {

                shortest_burst = processes[i].burst_time;
                shortest_index = i;
            }
        }

        // If no process has arrived, advance time
        if (shortest_index == -1) {
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

        Process& p = processes[shortest_index];

        p.waiting_time =
            current_time - p.arrival_time;

        gantt.push_back({p.id, p.burst_time});

        current_time += p.burst_time;

        p.turnaround_time =
            current_time - p.arrival_time;

        completed[shortest_index] = true;
        completed_count++;
    }

    cout << "SJF Scheduling Results\n";
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
