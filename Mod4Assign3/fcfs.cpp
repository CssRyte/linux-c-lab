
#include "common.h"

int main() {
    vector<Process> processes = {
        {"P1", 0, 8, 2},
        {"P2", 1, 4, 1},
        {"P3", 2, 9, 3},
        {"P4", 3, 5, 4}
    };

    // Sort processes by arrival time
    stable_sort(
        processes.begin(),
        processes.end(),
        [](const Process& a, const Process& b) {
            return a.arrival_time < b.arrival_time;
        }
    );

    vector<pair<string, int>> gantt;
    int current_time = 0;

    // First-Come, First-Served scheduling
    for (auto& p : processes) {

        // Wait if the process has not arrived
        if (current_time < p.arrival_time) {
            gantt.push_back({
                "Idle",
                p.arrival_time - current_time
            });
            current_time = p.arrival_time;
        }

        // Waiting time before process executes
        p.waiting_time =
            current_time - p.arrival_time;

        // Execute the process
        gantt.push_back({p.id, p.burst_time});
        current_time += p.burst_time;

        // Turnaround = completion - arrival
        p.turnaround_time =
            current_time - p.arrival_time;
    }

    cout << "FCFS Scheduling Results\n";
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
