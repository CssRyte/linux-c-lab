#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void handle_sigint(int sig)
{
    char answer;

    printf("\nDo you really want to quit? [y/n] ");
    scanf(" %c", &answer);

    if (answer == 'y' || answer == 'Y')
    {
        exit(0);
    }
}

int main()
{
    signal(SIGINT, handle_sigint);

    while (1)
    {
        printf("Program is running...\n");
        sleep(2);
    }

    return 0;
}