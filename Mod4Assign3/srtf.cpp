
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
    int completed = 0;

    vector<pair<string, int>> gantt;

    // Initially, remaining time equals burst time
    for (auto& p : processes) {
        p.remaining_time = p.burst_time;
    }

    while (completed < n) {
        int shortest = -1;
        int shortest_remaining = INT_MAX;

        // Find available process with shortest remaining time
        for (int i = 0; i < n; i++) {
            if (processes[i].arrival_time <= current_time &&
                processes[i].remaining_time > 0 &&
                processes[i].remaining_time < shortest_remaining) {

                shortest_remaining = processes[i].remaining_time;
                shortest = i;
            }
        }

        string running = "Idle";

        if (shortest != -1) {
            Process& p = processes[shortest];
            running = p.id;

            // Execute for one time unit
            p.remaining_time--;
        }

        // Record execution, merging adjacent equal entries
        if (!gantt.empty() && gantt.back().first == running) {
            gantt.back().second++;
        } else {
            gantt.push_back({running, 1});
        }

        current_time++;

        // Check whether the running process finished
        if (shortest != -1 &&
            processes[shortest].remaining_time == 0) {

            Process& p = processes[shortest];

            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;
        }
    }

    cout << "SRTF Scheduling Results\n";
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
