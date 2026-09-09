# Lab 3 — Processes, Fork, and Exec

## Overview

This lab explores Linux process management in C using `fork()`, `exec()`, `wait()`, `waitpid()`, pipes, and process states.

The lab includes demonstration programs and ten coding challenges covering parent/child processes, process replacement, inter-process communication, pipelines, zombies, and limited-concurrency worker pools.

## Build

From the `lab3-fork-exec` directory:

```bash
make clean
make
```

All compiled programs are placed in the `bin/` directory.

## Demo Programs

* `fork_demo` — demonstrates `fork()`, PID/PPID values, `waitpid()`, and child exit status.
* `exec_demo` — demonstrates replacing a child process using `execlp()`.
* `pipe_demo` — demonstrates parent-to-child communication through a pipe.
* `pipeline_demo` — creates a two-process pipeline using `fork()`, `pipe()`, and `exec()`.
* `zombie_demo` — demonstrates creation and removal of a zombie process.

## Completed Challenges

1. `ch1_single_fork` — single fork lifecycle and exit status.
2. `ch2_multi_child` — creates and collects multiple child processes.
3. `ch3_exec_ls` — executes `ls -la` from a child process.
4. `ch4_exec_worker` — executes an internal worker with arguments and `MYVAR=hello`.
5. `ch5_exec_examples` — compares `execl()` and `execv()`.
6. `ch6_pipe_sum` — sends numbers 1–10 through a pipe and calculates `Sum = 55`.
7. `ch7_pipeline` — manually implements `ls | grep pattern`.
8. `ch8_wait_nonblock` — uses `waitpid()` with `WNOHANG`.
9. `ch9_zombie` — creates and observes a zombie process before collecting it.
10. `ch10_pool` — implements a worker pool with a configurable concurrency limit.

## Example Runs

```bash
./bin/fork_demo
./bin/exec_demo
./bin/pipe_demo
./bin/pipeline_demo
./bin/zombie_demo

./bin/ch1_single_fork
./bin/ch2_multi_child 5
./bin/ch3_exec_ls
./bin/ch4_exec_worker
./bin/ch5_exec_examples
./bin/ch6_pipe_sum
./bin/ch7_pipeline "demo"
./bin/ch8_wait_nonblock
./bin/ch9_zombie
./bin/ch10_pool 3 file1 file2 file3 file4 file5
```

## Zombie Verification

In GitHub Codespaces, the zombie process was observed using:

```bash
./bin/ch9_zombie &
ps -eo pid,ppid,state,cmd | grep '[c]h9_zombie'
```

The child appeared with process state `Z` and `<defunct>` until the parent called `waitpid()`.

## Known Issues

The basic `ps -l` command in separate GitHub Codespaces terminals did not reliably display the process from the other terminal. Using `ps -eo pid,ppid,state,cmd` allowed the zombie process to be observed correctly.

## Lab Report

Screenshots of successful compilation, program execution, challenge verification, and zombie-process observation are included in `lab_report.pdf`.