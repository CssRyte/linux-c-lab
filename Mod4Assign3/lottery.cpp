
#include "common.h"
#include <random>
#include <numeric>

int main() {
    vector<Process> processes = {
        {"P1", 0, 8, 2},
        {"P2", 1, 4, 1},
        {"P3", 2, 9, 3},
        {"P4", 3, 5, 4}
    };

    const int n = processes.size();
    const unsigned int seed = 42;

    // Fixed seed makes our test reproducible.
    mt19937 rng(seed);

    vector<int> tickets(n);
    vector<pair<string, int>> gantt;

    int current_time = 0;
    int completed = 0;

    for (int i = 0; i < n; i++) {
        processes[i].remaining_time = processes[i].burst_time;

        // Lower priority number means higher priority.
        tickets[i] = 10 * (5 - processes[i].priority);
    }

    while (completed < n) {
        vector<int> ready;
        int total_tickets = 0;

        // Collect arrived, unfinished processes.
        for (int i = 0; i < n; i++) {
            if (processes[i].arrival_time <= current_time &&
                processes[i].remaining_time > 0) {

                ready.push_back(i);
                total_tickets += tickets[i];
            }
        }

        if (ready.empty()) {
            if (!gantt.empty() &&
                gantt.back().first == "Idle") {
                gantt.back().second++;
            } else {
                gantt.push_back({"Idle", 1});
            }

            current_time++;
            continue;
        }

        // Draw a winning ticket.
        uniform_int_distribution<int> distribution(
            1, total_tickets
        );

        int winning_ticket = distribution(rng);

        int selected = -1;
        int cumulative = 0;

        for (int index : ready) {
            cumulative += tickets[index];

            if (winning_ticket <= cumulative) {
                selected = index;
                break;
            }
        }

        Process& p = processes[selected];

        // Run the winner for one time unit.
        if (!gantt.empty() &&
            gantt.back().first == p.id) {
            gantt.back().second++;
        } else {
            gantt.push_back({p.id, 1});
        }

        p.remaining_time--;
        current_time++;

        if (p.remaining_time == 0) {
            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            completed++;
        }
    }

    cout << "Lottery Scheduling Results\n";
    cout << "Random Seed: " << seed << "\n";

    cout << "\nProcess\tTickets\tWaiting\tTurnaround\n";

    for (int i = 0; i < n; i++) {
        cout << processes[i].id << "\t"
             << tickets[i] << "\t"
             << processes[i].waiting_time << "\t"
             << processes[i].turnaround_time << "\n";
    }

    calculateMetrics(processes, current_time);
    printGantt(gantt);

    return 0;
}
