#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        printf("Fork failed!\n");
        return 1;
    }

    if (pid == 0) {
        printf("Child process is running ls -l...\n");

        execlp("ls", "ls", "-l", NULL);

        // This only runs if exec fails
        perror("execlp failed");
        exit(1);
    }
    else {
        wait(NULL);
        printf("Parent process finished waiting for child.\n");
    }

    return 0;
}