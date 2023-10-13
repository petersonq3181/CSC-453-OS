#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    char buff[1024];
    int cnt, i, fd;

    if (argc < 2)
    {
        write(STDERR_FILENO, "Usage: program <count>\n", 23);
        exit(EXIT_FAILURE);
    }

    cnt = atol(argv[1]);

    fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    sprintf(buff, "%%%dd\n", 8 * cnt);
    for (i = 0; i < cnt; i++)
    {
        char formatted[1024];
        snprintf(formatted, sizeof(formatted), buff, cnt);

        write(fd, formatted, strlen(formatted));
        sleep(1);
    }

    close(fd);
    return 0;
}