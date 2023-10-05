#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

#define MAX_PROCESSES 150
#define MAX_ARGUMENTS 10

typedef struct
{
  char *cmd;
  char *args[MAX_ARGUMENTS + 1];
  pid_t pid;
} Process;

void print_processes(Process processes[], int n_processes)
{
  printf("Number of Processes: %d\n", n_processes);

  int i;
  for (i = 0; i < n_processes; i++)
  {
    printf("Process %d\n", i + 1);
    printf("\tCommand: %s\n", processes[i].cmd);
    printf("\tPID: %d\n", processes[i].pid);

    if (processes[i].args[0] != NULL)
    {
      printf("\tArguments: ");
      int j = 0;
      while (processes[i].args[j] != NULL)
      {
        printf("%s ", processes[i].args[j]);
        j++;
      }
      printf("\n");
    }
  }
}

void signal_handler(int signum, siginfo_t *si, void *unused)
{
}

int main(int argc, char *argv[])
{

  /* ----- parsing: create and pause each process */
  if (argc < 3)
  {
    printf("Usage: %s quantum prog1 [args] : prog2 [args] ...\n", argv[0]);
    exit(-1);
  }

  char *endptr;
  long quantum = strtol(argv[1], &endptr, 10);

  int arg_cursor = 2;

  Process processes[MAX_PROCESSES];
  int n_processes = 0;

  int live_processes = 0;

  while (arg_cursor < argc && n_processes < MAX_PROCESSES)
  {
    if (strcmp(argv[arg_cursor], ":") == 0)
    {
      arg_cursor++;
      continue;
    }

    /* add process args */
    processes[n_processes].cmd = argv[arg_cursor];
    processes[n_processes].args[0] = argv[arg_cursor];

    int n_args = 1;
    arg_cursor++;
    while (arg_cursor < argc && strcmp(argv[arg_cursor], ":") != 0)
    {
      if (n_args < MAX_ARGUMENTS)
      {
        processes[n_processes].args[n_args] = argv[arg_cursor];
        n_args++;
      }
      arg_cursor++;
    }

    processes[n_processes].args[n_args] = NULL;

    /* fork child processes, immediately stop, and add pids to processes stuct */
    processes[n_processes].pid = fork();
    if (processes[n_processes].pid == 0) /* child */
    {
      /* immediately stop self */
      kill(getpid(), SIGSTOP);

      /* execute specified process */
      execvp(processes[n_processes].cmd, processes[n_processes].args);
      exit(EXIT_FAILURE);
    }
    else if (processes[n_processes].pid > 0) /* parent */
    {
      /* wait for the child to stop itself */
      if (waitpid(processes[n_processes].pid, NULL, WUNTRACED) < 0)
      {
        printf("waitpid error\n");
        exit(EXIT_FAILURE);
      }

      live_processes++;
    }
    else
    {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    n_processes++;
  }

  /* print process array to validate the parsing */
  /*
  print_processes(processes, n_processes);
  printf("done printing processes\n\n");
  */

  /* congig signal handler */
  sigset_t mask;
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &signal_handler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGALRM, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
  sigemptyset(&mask);

  /* config timer */
  struct itimerval timer;
  timer.it_interval.tv_sec = quantum / 1000;
  timer.it_interval.tv_usec = (quantum % 1000) * 1000;
  timer.it_value = timer.it_interval;
  if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
  {
    perror("setitimer");
    exit(EXIT_FAILURE);
  }

  /* ----- main scheduling loop */
  int i;
  int j;

  while (live_processes > 0)
  {
    for (i = 0; i < n_processes; i++)
    {
      if (processes[i].pid > 0)
      {
        /* continue and stop the process */
        kill(processes[i].pid, SIGCONT);

        usleep(quantum * 1000);
        kill(processes[i].pid, SIGSTOP);

        /* remove process from schedule if it has terminated */
        pid_t pid;
        while ((pid = waitpid(processes[i].pid, NULL, WNOHANG)) > 0)
        {
          live_processes--;
          for (j = 0; j < n_processes; j++)
          {
            if (processes[j].pid == pid)
            {
              processes[j].pid = -1;
              break;
            }
          }
        }
      }
    }
  }

  return 0;
}
