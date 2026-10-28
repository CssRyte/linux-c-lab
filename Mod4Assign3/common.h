
#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
using namespace std;

// Represents one process in the scheduler
struct Process {
    string id;
    int arrival_time;
    int burst_time;
    int priority;
    int remaining_time = 0;
    int waiting_time = 0;
    int turnaround_time = 0;
};

// Calculate and display scheduling metrics
inline void calculateMetrics(
    const vector<Process>& processes,
    int total_time
) {
    double avg_wait = 0;
    double avg_turnaround = 0;
    int total_burst = 0;

    for (const auto& p : processes) {
        avg_wait += p.waiting_time;
        avg_turnaround += p.turnaround_time;
        total_burst += p.burst_time;
    }

    if (processes.empty()) return;

    avg_wait /= processes.size();
    avg_turnaround /= processes.size();

    double cpu_util = total_time > 0
        ? (double)total_burst / total_time * 100
        : 0;

    cout << fixed << setprecision(2);
    cout << "\nAverage Waiting Time: "
         << avg_wait << "\n";
    cout << "Average Turnaround Time: "
         << avg_turnaround << "\n";
    cout << "CPU Utilization: "
         << cpu_util << "%\n";
}

// Print Gantt chart
inline void printGantt(
    const vector<pair<string, int>>& gantt
) {
    cout << "\nGantt Chart:\n";

    for (const auto& entry : gantt) {
        cout << "| " << entry.first
             << " (" << entry.second << ") ";
    }

    cout << "|\n";
}

#endif
