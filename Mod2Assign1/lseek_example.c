#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    write(fd, "Hello", 5);

    lseek(fd, 10, SEEK_SET);

    write(fd, "World", 5);

    close(fd);

    return 0;
}