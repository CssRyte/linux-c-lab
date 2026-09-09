#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    for (int i = 1; i <= 3; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            sleep(i);
            _exit(i);
        }
    }

    int finished_count = 0;

    while (finished_count < 3) {
        int status;

        pid_t finished = waitpid(-1, &status, WNOHANG);

        if (finished > 0) {
            if (WIFEXITED(status)) {
                printf(
                    "Parent: child PID=%d finished with status %d\n",
                    finished,
                    WEXITSTATUS(status)
                );
            }

            finished_count++;
        } else if (finished == 0) {
            printf("Parent: no child finished yet; continuing work...\n");
            usleep(200000);
        } else {
            perror("waitpid");
            return 1;
        }
    }

    printf("Parent: all children finished\n");

    return 0;
}