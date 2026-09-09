#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <max_workers> <task1> [task2 ...]\n", argv[0]);
        return 1;
    }

    int max_workers = atoi(argv[1]);

    if (max_workers <= 0) {
        fprintf(stderr, "Concurrency limit must be greater than 0\n");
        return 1;
    }

    int total_tasks = argc - 2;
    int next_task = 0;
    int active_workers = 0;

    while (next_task < total_tasks || active_workers > 0) {

        while (next_task < total_tasks &&
               active_workers < max_workers) {

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                printf(
                    "[worker PID=%d] starting task: %s\n",
                    getpid(),
                    argv[next_task + 2]
                );

                sleep(2);

                printf(
                    "[worker PID=%d] finished task: %s\n",
                    getpid(),
                    argv[next_task + 2]
                );

                _exit(0);
            }

            active_workers++;
            next_task++;
        }

        int status;
        pid_t finished = waitpid(-1, &status, 0);

        if (finished > 0) {
            printf(
                "[parent] worker PID=%d finished\n",
                finished
            );

            active_workers--;
        }
    }

    printf("[parent] all tasks completed\n");

    return 0;
}