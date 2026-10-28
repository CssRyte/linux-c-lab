
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

    vector<double> vruntime(n, 0.0);
    vector<pair<string, int>> gantt;

    for (auto& p : processes) {
        p.remaining_time = p.burst_time;
    }

    while (completed < n) {
        int selected = -1;

        // Select ready process with lowest vruntime
        for (int i = 0; i < n; i++) {
            if (processes[i].arrival_time > current_time ||
                processes[i].remaining_time <= 0) {
                continue;
            }

            if (selected == -1 ||
                vruntime[i] < vruntime[selected] - 1e-9) {
                selected = i;
            }
        }

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

        // Execute for one simulated time unit
        if (!gantt.empty() &&
            gantt.back().first == p.id) {
            gantt.back().second++;
        } else {
            gantt.push_back({p.id, 1});
        }

        p.remaining_time--;
        current_time++;

        // Simplified virtual runtime calculation
        vruntime[selected] +=
            1.0 / max(1, p.priority);

        if (p.remaining_time == 0) {
            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;
        }
    }

    cout << "CFS Scheduling Results\n";
    cout << "Process\tWaiting\tTurnaround\n";

    for (const auto& p : processes) {
        cout << p.id << "\t"
             << p.waiting_time << "\t"
             << p.turnaround_time << "\n";
    }

    calculateMetrics(processes, current_time);
    printGantt(gantt);

    cout << "\nFinal Virtual Runtime:\n";

    for (int i = 0; i < n; i++) {
        cout << processes[i].id
             << ": " << fixed << setprecision(2)
             << vruntime[i] << "\n";
    }

    return 0;
}
