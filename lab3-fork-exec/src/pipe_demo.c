#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Child: read from pipe and run wc -l on stdin
        close(fd[1]);

        dup2(fd[0], STDIN_FILENO);

        close(fd[0]);

        execlp("wc", "wc", "-l", (char *)NULL);

        perror("execlp wc");
        _exit(1);
    } else {
        // Parent: write 5 lines into the pipe
        close(fd[0]);

        FILE *out = fdopen(fd[1], "w");

        for (int i = 0; i < 5; ++i) {
            fprintf(out, "line %d\n", i);
        }

        fclose(out);

        waitpid(pid, NULL, 0);
    }

    return 0;
}