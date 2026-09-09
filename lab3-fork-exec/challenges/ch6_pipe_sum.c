#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        close(fd[1]);

        int number;
        int sum = 0;

        while (read(fd[0], &number, sizeof(number)) > 0) {
            sum += number;
        }

        close(fd[0]);

        printf("Sum = %d\n", sum);
        _exit(0);
    }

    close(fd[0]);

    for (int i = 1; i <= 10; i++) {
        write(fd[1], &i, sizeof(i));
    }

    close(fd[1]);

    waitpid(pid, NULL, 0);

    return 0;
}