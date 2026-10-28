# Module 4 Assignment 3 — Task Scheduling Simulator

## Overview

This project implements and compares ten CPU scheduling algorithms in C++. It includes individual scheduling exercises and a modular simulator that allows users to select algorithms, load process workloads, generate random tasks, and analyze scheduling performance.

## Scheduling Algorithms

1. First-Come, First-Served (FCFS)
2. Shortest Job First (SJF)
3. Shortest Remaining Time First (SRTF)
4. Priority Scheduling (Non-Preemptive)
5. Round Robin
6. Multilevel Queue Scheduling
7. Multilevel Feedback Queue Scheduling (MLFQ)
8. Lottery Scheduling
9. Completely Fair Scheduler (Simplified)
10. Earliest Deadline First (EDF)

## Features

- Interactive and command-line scheduler selection
- Configurable Round Robin time quantum
- CSV process input
- Random process generation using a reproducible seed
- Gantt chart generation
- Average waiting and turnaround time calculations
- CPU utilization and throughput measurements
- MLFQ aging-based priority promotion
- Text-file output logging
- Input validation and error handling

## Compilation

Compile the modular simulator:

```bash
g++ -std=c++17 -Wall -Wextra simulator.cpp -o simulator
```

Compile an individual exercise:

```bash
g++ -std=c++17 -Wall -Wextra fcfs.cpp -o fcfs
```

## Usage

Run the interactive menu:

```bash
./simulator
```

Run a specific scheduler:

```bash
./simulator --scheduler fcfs
./simulator --scheduler sjf
./simulator --scheduler rr --quantum 4
```

Load processes from CSV:

```bash
./simulator --scheduler fcfs --input processes.csv
```

Generate a random workload:

```bash
./simulator --scheduler rr --quantum 4 --random 10
```

Save simulation results:

```bash
./simulator --scheduler rr --quantum 4 --random 10 --output results.txt
```

Display help:

```bash
./simulator --help
```

## Input Format

CSV input uses four columns:

```csv
id,arrival,burst,priority
P1,0,8,2
P2,1,4,1
P3,2,9,3
P4,3,5,4
```

Each process has a unique identifier, arrival time, CPU burst time, and priority.

## Testing

The project was tested using:

- Four-process baseline workloads
- Ten-process CSV workloads
- Ten-process randomly generated workloads
- Round Robin quantum values of 1, 4, and 8
- Scheduling algorithm performance comparisons
- MLFQ aging and promotion tests
- Short CPU-burst workloads
- Invalid scheduler, quantum, process-count, and missing-file inputs
- Text output logging

## Limitations

The simulator models CPU scheduling using discrete simulated time units. It does not model real operating-system context-switch overhead or actual I/O blocking.

The Completely Fair Scheduler is a simplified educational approximation.

The MLFQ implementation supports aging-based promotions but does not implement every feature of a production operating-system scheduler.

## Project Structure

- `common.h` — Shared structures and metric functions for standalone exercises
- `fcfs.cpp` through `edf.cpp` — Individual scheduling implementations
- `simulator.cpp` — Modular scheduling application
- `processes.csv` — Ten-process test dataset
- `short_burst_workload.csv` — Short CPU-burst workload
- `mixed_workload.csv` — Mixed-length CPU workload
- `results.txt` — Example simulation output

## Environment

Language: C++17

Compiler: GNU g++

Platform: Linux

## References

- Operating Systems course lecture materials provided for the assignment
- Module 4 Assignment 3: Lab — Implementing and Enhancing Task Schedulers in C++
