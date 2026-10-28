
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <map>
#include <stdexcept>
#include <cmath>
using namespace std;

// Process representation for the modular simulator
struct Process {
    string id;
    int arrival_time;
    int burst_time;
    int priority;

    int remaining_time = 0;
    int waiting_time = 0;
    int turnaround_time = 0;

    double vruntime = 0.0;
    int deadline = 0;
};

// Represents one segment of CPU execution
struct GanttEntry {
    string process_id;
    int start_time;
    int end_time;
};

// Abstract base class for all schedulers
class Scheduler {
public:
    virtual ~Scheduler() = default;

    virtual void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) = 0;
};


class RoundRobinScheduler : public Scheduler {
private:
    int quantum;

public:
    explicit RoundRobinScheduler(int q)
        : quantum(q) {}

    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        if (quantum <= 0) {
            cerr << "Error: Quantum must be positive.\n";
            return;
        }

        queue<int> ready_queue;
        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        vector<bool> added(n, false);

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;
        }

        auto addArrivals = [&]() {
            vector<int> arrivals;

            for (int i = 0; i < n; i++) {
                if (!added[i] &&
                    processes[i].arrival_time <= current_time) {
                    arrivals.push_back(i);
                }
            }

            stable_sort(
                arrivals.begin(), arrivals.end(),
                [&](int a, int b) {
                    return processes[a].arrival_time <
                           processes[b].arrival_time;
                }
            );

            for (int i : arrivals) {
                ready_queue.push(i);
                added[i] = true;
            }
        };

        addArrivals();

        while (completed < n) {
            if (ready_queue.empty()) {
                int next_arrival = -1;

                for (int i = 0; i < n; i++) {
                    if (!added[i] &&
                        (next_arrival == -1 ||
                         processes[i].arrival_time < next_arrival)) {
                        next_arrival = processes[i].arrival_time;
                    }
                }

                if (next_arrival == -1) break;

                gantt.push_back({
                    "Idle", current_time, next_arrival
                });

                current_time = next_arrival;
                addArrivals();
                continue;
            }

            int index = ready_queue.front();
            ready_queue.pop();

            Process& p = processes[index];

            int run_time = min(quantum, p.remaining_time);
            int start_time = current_time;

            current_time += run_time;
            p.remaining_time -= run_time;

            gantt.push_back({
                p.id, start_time, current_time
            });

            addArrivals();

            if (p.remaining_time == 0) {
                p.turnaround_time =
                    current_time - p.arrival_time;

                p.waiting_time =
                    p.turnaround_time - p.burst_time;

                completed++;
            } else {
                ready_queue.push(index);
            }
        }

        total_time = current_time;
    }
};


class FCFSScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {
        vector<int> order;

        for (int i = 0; i < (int)processes.size(); i++) {
            order.push_back(i);
        }

        stable_sort(order.begin(), order.end(),
            [&](int a, int b) {
                return processes[a].arrival_time <
                       processes[b].arrival_time;
            });

        int current_time = 0;

        for (int i : order) {
            Process& p = processes[i];

            if (current_time < p.arrival_time) {
                gantt.push_back({
                    "Idle",
                    current_time,
                    p.arrival_time
                });
                current_time = p.arrival_time;
            }

            int start_time = current_time;
            current_time += p.burst_time;

            gantt.push_back({
                p.id, start_time, current_time
            });

            p.remaining_time = 0;
            p.turnaround_time =
                current_time - p.arrival_time;
            p.waiting_time =
                p.turnaround_time - p.burst_time;
        }

        total_time = current_time;
    }
};

class SJFScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {
        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        vector<bool> finished(n, false);

        while (completed < n) {
            int selected = -1;

            for (int i = 0; i < n; i++) {
                if (finished[i] ||
                    processes[i].arrival_time > current_time) {
                    continue;
                }

                if (selected == -1 ||
                    processes[i].burst_time <
                    processes[selected].burst_time) {
                    selected = i;
                }
            }

            if (selected == -1) {
                int next_arrival = -1;

                for (int i = 0; i < n; i++) {
                    if (!finished[i] &&
                        (next_arrival == -1 ||
                         processes[i].arrival_time < next_arrival)) {
                        next_arrival = processes[i].arrival_time;
                    }
                }

                gantt.push_back({
                    "Idle", current_time, next_arrival
                });

                current_time = next_arrival;
                continue;
            }

            Process& p = processes[selected];

            int start_time = current_time;
            current_time += p.burst_time;

            gantt.push_back({
                p.id, start_time, current_time
            });

            p.remaining_time = 0;
            p.turnaround_time =
                current_time - p.arrival_time;
            p.waiting_time =
                p.turnaround_time - p.burst_time;

            finished[selected] = true;
            completed++;
        }

        total_time = current_time;
    }
};


class SRTFScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;
        }

        while (completed < n) {
            int selected = -1;

            for (int i = 0; i < n; i++) {
                if (processes[i].arrival_time > current_time ||
                    processes[i].remaining_time <= 0) {
                    continue;
                }

                if (selected == -1 ||
                    processes[i].remaining_time <
                    processes[selected].remaining_time) {
                    selected = i;
                }
            }

            string running = "Idle";

            if (selected != -1) {
                Process& p = processes[selected];
                running = p.id;

                p.remaining_time--;
            }

            // Merge consecutive execution units
            if (!gantt.empty() &&
                gantt.back().process_id == running &&
                gantt.back().end_time == current_time) {

                gantt.back().end_time++;
            } else {
                gantt.push_back({
                    running,
                    current_time,
                    current_time + 1
                });
            }

            current_time++;

            if (selected != -1 &&
                processes[selected].remaining_time == 0) {

                Process& p = processes[selected];

                p.turnaround_time =
                    current_time - p.arrival_time;

                p.waiting_time =
                    p.turnaround_time - p.burst_time;

                completed++;
            }
        }

        total_time = current_time;
    }
};


class PriorityScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        vector<bool> finished(n, false);

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;
        }

        while (completed < n) {
            int selected = -1;

            // Find highest-priority available process
            for (int i = 0; i < n; i++) {
                if (finished[i] ||
                    processes[i].arrival_time > current_time) {
                    continue;
                }

                if (selected == -1 ||
                    processes[i].priority <
                    processes[selected].priority ||
                    (processes[i].priority ==
                     processes[selected].priority &&
                     processes[i].arrival_time <
                     processes[selected].arrival_time)) {
                    selected = i;
                }
            }

            // Move time forward if no process is ready
            if (selected == -1) {
                int next_arrival = -1;

                for (int i = 0; i < n; i++) {
                    if (!finished[i] &&
                        (next_arrival == -1 ||
                         processes[i].arrival_time < next_arrival)) {
                        next_arrival = processes[i].arrival_time;
                    }
                }

                gantt.push_back({
                    "Idle", current_time, next_arrival
                });

                current_time = next_arrival;
                continue;
            }

            Process& p = processes[selected];

            int start_time = current_time;

            // Non-preemptive execution
            current_time += p.burst_time;

            gantt.push_back({
                p.id, start_time, current_time
            });

            p.remaining_time = 0;

            p.turnaround_time =
                current_time - p.arrival_time;

            p.waiting_time =
                p.turnaround_time - p.burst_time;

            finished[selected] = true;
            completed++;
        }

        total_time = current_time;
    }
};


class MultilevelQueueScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        const int quantum = 4;
        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        queue<int> high_queue;
        queue<int> low_queue;
        vector<bool> added(n, false);

        int high_current = -1;
        int high_slice = 0;
        int low_current = -1;

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;
        }

        auto addArrivals = [&]() {
            vector<int> arrivals;

            for (int i = 0; i < n; i++) {
                if (!added[i] &&
                    processes[i].arrival_time <= current_time) {
                    arrivals.push_back(i);
                }
            }

            stable_sort(arrivals.begin(), arrivals.end(),
                [&](int a, int b) {
                    return processes[a].arrival_time <
                           processes[b].arrival_time;
                });

            for (int i : arrivals) {
                if (processes[i].priority < 3)
                    high_queue.push(i);
                else
                    low_queue.push(i);

                added[i] = true;
            }
        };

        auto record = [&](const string& id) {
            if (!gantt.empty() &&
                gantt.back().process_id == id &&
                gantt.back().end_time == current_time) {
                gantt.back().end_time++;
            } else {
                gantt.push_back({
                    id, current_time, current_time + 1
                });
            }
        };

        while (completed < n) {
            addArrivals();

            int selected = -1;

            if (high_current != -1 ||
                !high_queue.empty()) {

                if (high_current == -1) {
                    high_current = high_queue.front();
                    high_queue.pop();
                    high_slice = 0;
                }

                selected = high_current;
            }
            else {
                if (low_current == -1 &&
                    !low_queue.empty()) {
                    low_current = low_queue.front();
                    low_queue.pop();
                }

                selected = low_current;
            }

            if (selected == -1) {
                record("Idle");
                current_time++;
                continue;
            }

            Process& p = processes[selected];

            record(p.id);
            p.remaining_time--;
            current_time++;

            if (selected == high_current)
                high_slice++;

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
            }
            else if (selected == high_current &&
                     high_slice == quantum) {
                // Add new arrivals before requeuing.
                addArrivals();

                high_queue.push(high_current);
                high_current = -1;
                high_slice = 0;
            }
        }

        total_time = current_time;
    }
};


class MLFQScheduler : public Scheduler {
private:
    const int quantum[2] = {2, 4};
    const int aging_threshold = 10;

public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        deque<int> queues[3];

        vector<bool> added(n, false);
        vector<int> waiting_age(n, 0);
        vector<int> current_level(n, 0);
        vector<int> slice_used(n, 0);

        int running = -1;

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;
        }

        // Record execution in the Gantt chart
        auto record = [&](const string& id) {
            if (!gantt.empty() &&
                gantt.back().process_id == id &&
                gantt.back().end_time == current_time) {
                gantt.back().end_time++;
            } else {
                gantt.push_back({
                    id, current_time, current_time + 1
                });
            }
        };

        // Newly arrived processes enter Q0
        auto addArrivals = [&]() {
            for (int i = 0; i < n; i++) {
                if (!added[i] &&
                    processes[i].arrival_time <= current_time) {

                    queues[0].push_back(i);
                    added[i] = true;
                    current_level[i] = 0;
                    waiting_age[i] = 0;
                }
            }
        };

        // Promote processes that waited too long
        auto applyAging = [&]() {
            for (int level = 1; level <= 2; level++) {
                size_t count = queues[level].size();

                for (size_t j = 0; j < count; j++) {
                    int index = queues[level].front();
                    queues[level].pop_front();

                    if (waiting_age[index] >= aging_threshold) {
                        int new_level = level - 1;

                        current_level[index] = new_level;
                        waiting_age[index] = 0;
                        slice_used[index] = 0;

                        queues[new_level].push_back(index);

                        cout << "[AGING] Time " << current_time
                             << ": " << processes[index].id
                             << " promoted Q" << level
                             << " -> Q" << new_level << "\n";
                    } else {
                        queues[level].push_back(index);
                    }
                }
            }
        };

        while (completed < n) {
            addArrivals();

            // Preempt if higher-priority work is ready
            if (running != -1) {
                int level = current_level[running];

                bool higher_ready = false;

                for (int q = 0; q < level; q++) {
                    if (!queues[q].empty()) {
                        higher_ready = true;
                        break;
                    }
                }

                if (higher_ready) {
                    queues[level].push_front(running);
                    running = -1;
                }
            }

            // Select next process if CPU is free
            if (running == -1) {
                for (int level = 0; level < 3; level++) {
                    if (!queues[level].empty()) {
                        running = queues[level].front();
                        queues[level].pop_front();
                        waiting_age[running] = 0;
                        break;
                    }
                }
            }

            if (running == -1) {
                record("Idle");
                current_time++;
                continue;
            }

            int index = running;
            int level = current_level[index];

            Process& p = processes[index];

            // Execute one simulated time unit
            record(p.id);
            p.remaining_time--;
            slice_used[index]++;

            // Increase consecutive waiting time
            for (int q = 0; q < 3; q++) {
                for (int waiting_index : queues[q]) {
                    waiting_age[waiting_index]++;
                }
            }

            current_time++;

            // Check for completion
            if (p.remaining_time == 0) {
                p.turnaround_time =
                    current_time - p.arrival_time;

                p.waiting_time =
                    p.turnaround_time - p.burst_time;

                completed++;
                running = -1;
            }
            else if (level < 2 &&
                     slice_used[index] >= quantum[level]) {

                // Demote after a full quantum
                current_level[index] = level + 1;
                slice_used[index] = 0;
                waiting_age[index] = 0;

                queues[level + 1].push_back(index);
                running = -1;
            }

            // Apply aging to waiting processes
            applyAging();
        }

        total_time = current_time;
    }
};


class LotteryScheduler : public Scheduler {
private:
    unsigned int seed;

public:
    explicit LotteryScheduler(unsigned int s = 42)
        : seed(s) {}

    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        mt19937 rng(seed);
        vector<int> tickets(n);

        for (int i = 0; i < n; i++) {
            Process& p = processes[i];

            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;

            // Higher-priority processes get more tickets.
            tickets[i] = max(1, 10 * (5 - p.priority));
        }

        while (completed < n) {
            vector<int> ready;
            int total_tickets = 0;

            // Find ready processes and their ticket totals
            for (int i = 0; i < n; i++) {
                if (processes[i].arrival_time <= current_time &&
                    processes[i].remaining_time > 0) {

                    ready.push_back(i);
                    total_tickets += tickets[i];
                }
            }

            if (ready.empty()) {
                if (!gantt.empty() &&
                    gantt.back().process_id == "Idle" &&
                    gantt.back().end_time == current_time) {
                    gantt.back().end_time++;
                } else {
                    gantt.push_back({
                        "Idle", current_time, current_time + 1
                    });
                }

                current_time++;
                continue;
            }

            // Draw one random ticket
            uniform_int_distribution<int> distribution(
                1, total_tickets
            );

            int winning_ticket = distribution(rng);
            int cumulative = 0;
            int selected = -1;

            for (int index : ready) {
                cumulative += tickets[index];

                if (winning_ticket <= cumulative) {
                    selected = index;
                    break;
                }
            }

            Process& p = processes[selected];

            // Execute the selected process for one unit
            if (!gantt.empty() &&
                gantt.back().process_id == p.id &&
                gantt.back().end_time == current_time) {
                gantt.back().end_time++;
            } else {
                gantt.push_back({
                    p.id, current_time, current_time + 1
                });
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

        total_time = current_time;
    }
};


class CFSScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;
            p.vruntime = 0.0;
        }

        while (completed < n) {
            int selected = -1;

            // Select available process with smallest vruntime
            for (int i = 0; i < n; i++) {
                if (processes[i].arrival_time > current_time ||
                    processes[i].remaining_time <= 0) {
                    continue;
                }

                if (selected == -1 ||
                    processes[i].vruntime <
                    processes[selected].vruntime - 1e-9) {
                    selected = i;
                }
            }

            if (selected == -1) {
                if (!gantt.empty() &&
                    gantt.back().process_id == "Idle" &&
                    gantt.back().end_time == current_time) {
                    gantt.back().end_time++;
                } else {
                    gantt.push_back({
                        "Idle", current_time, current_time + 1
                    });
                }

                current_time++;
                continue;
            }

            Process& p = processes[selected];

            // Record one unit of CPU execution
            if (!gantt.empty() &&
                gantt.back().process_id == p.id &&
                gantt.back().end_time == current_time) {
                gantt.back().end_time++;
            } else {
                gantt.push_back({
                    p.id, current_time, current_time + 1
                });
            }

            p.remaining_time--;
            current_time++;

            // Assignment's simplified vruntime formula
            p.vruntime += 1.0 / max(1, p.priority);

            if (p.remaining_time == 0) {
                p.turnaround_time =
                    current_time - p.arrival_time;

                p.waiting_time =
                    p.turnaround_time - p.burst_time;

                completed++;
            }
        }

        total_time = current_time;
    }
};


class EDFScheduler : public Scheduler {
public:
    void schedule(
        vector<Process>& processes,
        vector<GanttEntry>& gantt,
        int& total_time
    ) override {

        int n = processes.size();
        int current_time = 0;
        int completed = 0;

        for (auto& p : processes) {
            p.remaining_time = p.burst_time;
            p.waiting_time = 0;
            p.turnaround_time = 0;

            p.deadline =
                p.arrival_time + p.burst_time + 2;
        }

        while (completed < n) {
            int selected = -1;

            // Select available process with earliest deadline
            for (int i = 0; i < n; i++) {
                if (processes[i].arrival_time > current_time ||
                    processes[i].remaining_time <= 0) {
                    continue;
                }

                if (selected == -1 ||
                    processes[i].deadline <
                    processes[selected].deadline ||
                    (processes[i].deadline ==
                     processes[selected].deadline &&
                     processes[i].arrival_time <
                     processes[selected].arrival_time)) {

                    selected = i;
                }
            }

            if (selected == -1) {
                if (!gantt.empty() &&
                    gantt.back().process_id == "Idle" &&
                    gantt.back().end_time == current_time) {
                    gantt.back().end_time++;
                } else {
                    gantt.push_back({
                        "Idle", current_time, current_time + 1
                    });
                }

                current_time++;
                continue;
            }

            Process& p = processes[selected];

            // Execute for one unit, allowing preemption
            if (!gantt.empty() &&
                gantt.back().process_id == p.id &&
                gantt.back().end_time == current_time) {
                gantt.back().end_time++;
            } else {
                gantt.push_back({
                    p.id, current_time, current_time + 1
                });
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

        total_time = current_time;
    }
};


vector<Process> loadProcesses(const string& filename) {
    ifstream file(filename);
    if (!file) {
        throw runtime_error("Cannot open input file: " + filename);
    }

    vector<Process> processes;
    string line;
    int line_number = 0;

    while (getline(file, line)) {
        line_number++;

        // Skip empty lines
        if (line.find_first_not_of(" \t\r\n") == string::npos)
            continue;

        stringstream ss(line);
        string id, arrival, burst, priority;

        if (!getline(ss, id, ',') ||
            !getline(ss, arrival, ',') ||
            !getline(ss, burst, ',') ||
            !getline(ss, priority, ',')) {
            throw runtime_error(
                "Invalid CSV at line " + to_string(line_number)
            );
        }

        // Allow a header row
        if (line_number == 1 && id == "id")
            continue;

        Process p{};
        p.id = id;

        try {
            p.arrival_time = stoi(arrival);
            p.burst_time = stoi(burst);
            p.priority = stoi(priority);
        } catch (...) {
            throw runtime_error(
                "Invalid numeric value at line " +
                to_string(line_number)
            );
        }

        if (p.id.empty() || p.arrival_time < 0 ||
            p.burst_time <= 0 || p.priority <= 0) {
            throw runtime_error(
                "Invalid process at line " +
                to_string(line_number)
            );
        }

        processes.push_back(p);
    }

    if (processes.empty())
        throw runtime_error("No processes loaded.");

    return processes;
}

vector<Process> generateRandomProcesses(
    int count,
    unsigned int seed = 42
) {
    if (count <= 0 || count > 10000)
        throw runtime_error("Random count must be 1-10000.");

    mt19937 rng(seed);
    uniform_int_distribution<int> arrival_dist(0, 10);
    uniform_int_distribution<int> burst_dist(1, 15);
    uniform_int_distribution<int> priority_dist(1, 4);

    vector<Process> processes;

    for (int i = 0; i < count; i++) {
        Process p{};
        p.id = "P" + to_string(i + 1);
        p.arrival_time = arrival_dist(rng);
        p.burst_time = burst_dist(rng);
        p.priority = priority_dist(rng);
        processes.push_back(p);
    }

    return processes;
}


void printSimulationResults(
    ostream& out,
    const string& scheduler_name,
    const string& scheduler_type,
    int quantum,
    const vector<Process>& processes,
    const vector<GanttEntry>& gantt,
    int total_time
) {
    out << "Module 4 Assignment 3\n";
    out << "Modular Task Scheduling Simulator\n\n";

    out << "Scheduler: " << scheduler_name << "\n";

    if (scheduler_type == "rr")
        out << "Time Quantum: " << quantum << "\n";

    out << "Process\tWaiting\tTurnaround\n";

    double total_waiting = 0;
    double total_turnaround = 0;
    int total_burst = 0;

    for (const auto& p : processes) {
        out << p.id << "\t"
            << p.waiting_time << "\t"
            << p.turnaround_time << "\n";

        total_waiting += p.waiting_time;
        total_turnaround += p.turnaround_time;
        total_burst += p.burst_time;
    }

    out << fixed << setprecision(2);

    out << "\nAverage Waiting Time: "
        << total_waiting / processes.size() << "\n";

    out << "Average Turnaround Time: "
        << total_turnaround / processes.size() << "\n";

    double utilization = total_time > 0
        ? 100.0 * total_burst / total_time : 0;

    double throughput = total_time > 0
        ? (double)processes.size() / total_time : 0;

    out << "CPU Utilization: " << utilization << "%\n";
    out << "Throughput: " << throughput
        << " processes/time unit\n";

    out << "\nGantt Chart:\n";

    for (const auto& entry : gantt) {
        out << "| " << entry.process_id
            << " (" << entry.start_time
            << "-" << entry.end_time << ") ";
    }

    out << "|\nTotal Time: " << total_time << "\n";
}


int main(int argc, char* argv[]) {
    // Default process dataset
    vector<Process> original_processes = {
        {"P1", 0, 8, 2},
        {"P2", 1, 4, 1},
        {"P3", 2, 9, 3},
        {"P4", 3, 5, 4}
    };

    // Command-line configuration
    string scheduler_type;
    string input_filename;
    string output_filename;

    int quantum = 4;
    int random_count = 0;

    bool use_random = false;

    // --------------------------------------
    // 1. Parse command-line arguments
    // --------------------------------------
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "--help") {
            cout << "Module 4 Assignment 3\n";
            cout << "Modular Task Scheduling Simulator\n\n";

            cout << "Usage: ./simulator [options]\n\n";

            cout << "Options:\n";
            cout << "  --scheduler NAME  Select scheduler\n";
            cout << "  --quantum N       Round Robin time quantum\n";
            cout << "  --input FILE      Load processes from CSV\n";
            cout << "  --random N        Generate N random processes\n";
            cout << "  --output FILE     Save results to a text file\n";
            cout << "  --help            Display this help message\n\n";

            cout << "Available schedulers:\n";
            cout << "  fcfs, sjf, rr, srtf, priority,\n";
            cout << "  mlq, mlfq, lottery, cfs, edf\n";

            return 0;
        }
        else if (arg == "--scheduler" && i + 1 < argc) {
            scheduler_type = argv[++i];
        }
        else if (arg == "--quantum" && i + 1 < argc) {
            try {
                quantum = stoi(argv[++i]);
            } catch (...) {
                cerr << "Error: Invalid quantum value.\n";
                return 1;
            }
        }
        else if (arg == "--input" && i + 1 < argc) {
            input_filename = argv[++i];
        }
        else if (arg == "--random" && i + 1 < argc) {
            use_random = true;

            try {
                random_count = stoi(argv[++i]);
            } catch (...) {
                cerr << "Error: Invalid random process count.\n";
                return 1;
            }
        }
        else if (arg == "--output" && i + 1 < argc) {
            output_filename = argv[++i];
        }
        else {
            cerr << "Error: Unknown or incomplete argument: "
                 << arg << "\n";
            return 1;
        }
    }

    // --------------------------------------
    // 2. Validate arguments
    // --------------------------------------
    if (quantum <= 0) {
        cerr << "Error: Quantum must be positive.\n";
        return 1;
    }

    if (!input_filename.empty() && use_random) {
        cerr << "Error: Use either --input or --random, "
             << "not both.\n";
        return 1;
    }

    // --------------------------------------
    // 3. Load or generate processes
    // --------------------------------------
    try {
        if (!input_filename.empty()) {
            original_processes = loadProcesses(input_filename);
        }
        else if (use_random) {
            original_processes =
                generateRandomProcesses(random_count, 42);
        }
    }
    catch (const exception& e) {
        cerr << "Input error: " << e.what() << "\n";
        return 1;
    }

    if (original_processes.empty()) {
        cerr << "Error: No processes available.\n";
        return 1;
    }

    // --------------------------------------
    // 4. Interactive menu (if no scheduler)
    // --------------------------------------
    if (scheduler_type.empty()) {
        cout << "Module 4 Assignment 3\n";
        cout << "Modular Task Scheduling Simulator\n\n";

        cout << "Choose Scheduler:\n";
        cout << "1. FCFS\n";
        cout << "2. SJF\n";
        cout << "3. Round Robin\n";
        cout << "4. SRTF\n";
        cout << "5. Priority Scheduling\n";
        cout << "6. Multilevel Queue\n";
        cout << "7. Multilevel Feedback Queue\n";
        cout << "8. Lottery Scheduling\n";
        cout << "9. Completely Fair Scheduler\n";
        cout << "10. Earliest Deadline First\n";

        cout << "Enter choice: ";

        int choice;

        if (!(cin >> choice)) {
            cerr << "Error: Invalid menu input.\n";
            return 1;
        }

        vector<string> scheduler_names = {
            "fcfs",
            "sjf",
            "rr",
            "srtf",
            "priority",
            "mlq",
            "mlfq",
            "lottery",
            "cfs",
            "edf"
        };

        if (choice < 1 || choice > 10) {
            cerr << "Error: Invalid scheduler choice.\n";
            return 1;
        }

        scheduler_type = scheduler_names[choice - 1];
    }

    // --------------------------------------
    // 5. Create the selected scheduler
    // --------------------------------------
    unique_ptr<Scheduler> scheduler;
    string scheduler_name;

    if (scheduler_type == "fcfs") {
        scheduler = make_unique<FCFSScheduler>();
        scheduler_name = "FCFS";
    }
    else if (scheduler_type == "sjf") {
        scheduler = make_unique<SJFScheduler>();
        scheduler_name = "SJF";
    }
    else if (scheduler_type == "rr") {
        scheduler = make_unique<RoundRobinScheduler>(quantum);
        scheduler_name = "Round Robin";
    }
    else if (scheduler_type == "srtf") {
        scheduler = make_unique<SRTFScheduler>();
        scheduler_name = "SRTF";
    }
    else if (scheduler_type == "priority") {
        scheduler = make_unique<PriorityScheduler>();
        scheduler_name = "Priority Scheduling";
    }
    else if (scheduler_type == "mlq") {
        scheduler = make_unique<MultilevelQueueScheduler>();
        scheduler_name = "Multilevel Queue";
    }
    else if (scheduler_type == "mlfq") {
        scheduler = make_unique<MLFQScheduler>();
        scheduler_name = "Multilevel Feedback Queue";
    }
    else if (scheduler_type == "lottery") {
        scheduler = make_unique<LotteryScheduler>(42);
        scheduler_name = "Lottery Scheduling";
    }
    else if (scheduler_type == "cfs") {
        scheduler = make_unique<CFSScheduler>();
        scheduler_name = "Completely Fair Scheduler";
    }
    else if (scheduler_type == "edf") {
        scheduler = make_unique<EDFScheduler>();
        scheduler_name = "Earliest Deadline First";
    }
    else {
        cerr << "Error: Unknown scheduler: "
             << scheduler_type << "\n";
        return 1;
    }

    // --------------------------------------
    // 6. Run the simulation
    // --------------------------------------
    vector<Process> processes = original_processes;
    vector<GanttEntry> gantt;

    int total_time = 0;

    scheduler->schedule(
        processes,
        gantt,
        total_time
    );

    // --------------------------------------
    // 7. Print results to terminal
    // --------------------------------------
    printSimulationResults(
        cout,
        scheduler_name,
        scheduler_type,
        quantum,
        processes,
        gantt,
        total_time
    );

    // --------------------------------------
    // 8. Optionally save results to a file
    // --------------------------------------
    if (!output_filename.empty()) {
        ofstream output_file(output_filename);

        if (!output_file) {
            cerr << "Error: Unable to create output file: "
                 << output_filename << "\n";
            return 1;
        }

        printSimulationResults(
            output_file,
            scheduler_name,
            scheduler_type,
            quantum,
            processes,
            gantt,
            total_time
        );

        if (!output_file) {
            cerr << "Error: Failed to write results.\n";
            return 1;
        }

        output_file.close();

        cout << "\nResults saved to: "
             << output_filename << "\n";
    }

    return 0;
}


