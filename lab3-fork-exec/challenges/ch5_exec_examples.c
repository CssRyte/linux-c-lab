#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    pid_t child_a = fork();

    if (child_a < 0) {
        perror("fork");
        return 1;
    }

    if (child_a == 0) {
        execl(
            "/bin/echo",
            "echo",
            "one",
            "two",
            (char *)NULL
        );

        perror("execl");
        _exit(1);
    }

    waitpid(child_a, NULL, 0);

    pid_t child_b = fork();

    if (child_b < 0) {
        perror("fork");
        return 1;
    }

    if (child_b == 0) {
        char *args[] = {
            "echo",
            "one",
            "two",
            NULL
        };

        execv("/bin/echo", args);

        perror("execv");
        _exit(1);
    }

    waitpid(child_b, NULL, 0);

    return 0;
}