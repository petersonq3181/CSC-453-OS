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

int ALRM = 0;
int CHLD = 0;

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
  if (signum == SIGALRM)
  {
    /*
    printf("in signal_hanlder SIGALRM\n");
    fflush(stdout);
    */
    ALRM = 1;
  }
  else if (signum == SIGCHLD)
  {
    /*
    printf("in signal_hanlder SIGCHLD\n");
    fflush(stdout);
    */
    CHLD = 1;
  }
}

int main(int argc, char *argv[])
{

  /* ----- parsing, and create and pause each process */
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

    processes[n_processes].pid = fork();
    if (processes[n_processes].pid == 0)
    {
      /* pause(); */
      execvp(processes[n_processes].cmd, processes[n_processes].args);
      exit(EXIT_FAILURE);
    }
    else if (processes[n_processes].pid > 0)
    {
      live_processes++;
      kill(processes[n_processes].pid, SIGSTOP);
    }
    else
    {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    n_processes++;
  }

  /*
  print_processes(processes, n_processes);
  printf("done printing processes\n\n");
  */

  sigset_t mask;
  struct itimerval timer;
  struct sigaction sa;

  /* Set up signal handler */
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &signal_handler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGCHLD, &sa, NULL) == -1 || sigaction(SIGALRM, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  sigemptyset(&mask);

  /* Configure timer */
  timer.it_interval.tv_sec = quantum / 1000;
  timer.it_interval.tv_usec = (quantum % 1000) * 1000;
  timer.it_value = timer.it_interval;

  /* Starting timer */
  if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
  {
    perror("setitimer");
    exit(EXIT_FAILURE);
  }

  int i;
  int j;
  while (live_processes > 0)
  {
    for (i = 0; i < n_processes; i++)
    {
      if (processes[i].pid > 0)
      {
        printf("starting process: %d\n", processes[i].pid);
        kill(processes[i].pid, SIGCONT);
        ALRM = 0;
        CHLD = 0;

        while (1)
        {

          if (ALRM)
          {
            kill(processes[i].pid, SIGSTOP);
            break;
          }
          else if (CHLD)
          {
            pid_t pid;
            while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
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

            if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
            {
              perror("setitimer");
              exit(EXIT_FAILURE);
            }
            break;
          }
          /*sleep(2)*/
        }
      }
    }
  }

  /* cleanup children */
  for (i = 0; i < n_processes; i++)
  {
    if (processes[i].pid > 0)
    {
      kill(processes[i].pid, SIGKILL);
    }
  }

  return 0;
}