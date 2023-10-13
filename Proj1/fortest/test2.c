#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
    int a = 2;

    while (a > 0)
    {
        int ret = fork();
        if (ret == 0)
        {
            a++;
            printf("a=%d\n", a);
            execl("/bin/ls", "/bin/ls", NULL);
        }
        else
        {
            wait(NULL);
            a--;
        }
    }

    return 0;
}