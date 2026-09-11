#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    char command[100];

    while (1) {
        printf("mini-shell> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "exit") == 0) {
            printf("Exiting mini shell...\n");
            break;
        }

        pid_t pid = fork();

        if (pid < 0) {
            printf("Fork failed!\n");
        }
        else if (pid == 0) {
            execlp(command, command, NULL);

            perror("Command failed");
            exit(1);
        }
        else {
            wait(NULL);
        }
    }

    return 0;
}