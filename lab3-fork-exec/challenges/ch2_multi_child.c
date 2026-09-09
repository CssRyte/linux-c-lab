#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_children>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);

    if (n <= 0) {
        fprintf(stderr, "Number of children must be greater than 0\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            printf(
                "Child index %d: PID=%d, PPID=%d\n",
                i,
                getpid(),
                getppid()
            );

            _exit(i + 1);
        }
    }

    for (int i = 0; i < n; i++) {
        int status;

        pid_t finished = wait(&status);

        if (finished == -1) {
            perror("wait");
            return 1;
        }

        if (WIFEXITED(status)) {
            printf(
                "Parent: child PID=%d finished with status %d\n",
                finished,
                WEXITSTATUS(status)
            );
        }
    }

    return 0;
}