
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

    vector<int> deadlines(n);
    vector<int> completion_times(n, 0);
    vector<pair<string, int>> gantt;

    for (int i = 0; i < n; i++) {
        processes[i].remaining_time =
            processes[i].burst_time;

        deadlines[i] =
            processes[i].arrival_time +
            processes[i].burst_time + 2;
    }

    while (completed < n) {
        int selected = -1;

        // Select arrived process with earliest deadline
        for (int i = 0; i < n; i++) {
            if (processes[i].arrival_time > current_time ||
                processes[i].remaining_time <= 0) {
                continue;
            }

            if (selected == -1 ||
                deadlines[i] < deadlines[selected] ||
                (deadlines[i] == deadlines[selected] &&
                 processes[i].arrival_time <
                 processes[selected].arrival_time)) {

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

        // Run for one time unit, allowing preemption
        if (!gantt.empty() &&
            gantt.back().first == p.id) {
            gantt.back().second++;
        } else {
            gantt.push_back({p.id, 1});
        }

        p.remaining_time--;
        current_time++;

        if (p.remaining_time == 0) {
            completion_times[selected] = current_time;

            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;
        }
    }

    cout << "EDF Scheduling Results\n";
    cout << "Process\tDeadline\tWaiting\tTurnaround\tStatus\n";

    int missed = 0;

    for (int i = 0; i < n; i++) {
        bool deadline_met =
            completion_times[i] <= deadlines[i];

        if (!deadline_met) missed++;

        cout << processes[i].id << "\t"
             << deadlines[i] << "\t"
             << processes[i].waiting_time << "\t"
             << processes[i].turnaround_time << "\t"
             << (deadline_met ? "Met" : "Missed")
             << "\n";
    }

    calculateMetrics(processes, current_time);
    printGantt(gantt);

    cout << "\nDeadlines Met: " << n - missed << "\n";
    cout << "Deadlines Missed: " << missed << "\n";

    return 0;
}
