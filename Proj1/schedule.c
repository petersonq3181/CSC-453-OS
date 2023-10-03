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

typedef struct {
  char *cmd;
  char *args[MAX_ARGUMENTS + 1];
  pid_t pid;
} Process;


void print_processes(Process processes[], int n_processes);

void signal_handler(int signum, siginfo_t *si, void *unused);


int main(int argc, char *argv[]) {

  /* ----- parsing, and create and pause each process */
  if (argc < 3) {
    printf("Usage: %s quantum prog1 [args] : prog2 [args] ...\n", argv[0]);
    exit(-1);
  }

  char *endptr; 
  long quantum = strtol(argv[1], &endptr, 10);

  int arg_cursor = 2;

  Process processes[MAX_PROCESSES];
  int n_processes = 0;

  int live_processes = 0;

  while (arg_cursor < argc && n_processes < MAX_PROCESSES) {
    if (strcmp(argv[arg_cursor], ":") == 0) {
      arg_cursor++;
      continue;
    }

    processes[n_processes].cmd = argv[arg_cursor];
    processes[n_processes].args[0] = argv[arg_cursor];

    int n_args = 1;
    arg_cursor++;
    while (arg_cursor < argc && strcmp(argv[arg_cursor], ":") != 0) {
      if (n_args < MAX_ARGUMENTS) {
        processes[n_processes].args[n_args] = argv[arg_cursor];
        n_args++;
      }
      arg_cursor++;
    }

    processes[n_processes].args[n_args] = NULL;

    processes[n_processes].pid = fork();
    if (processes[n_processes].pid == 0) { 
      pause();
      execvp(processes[n_processes].cmd, processes[n_processes].args);
      printf("execvp failed\n");
      exit(EXIT_FAILURE); 
    } else if (processes[n_processes].pid > 0) { 
      live_processes++;
      kill(processes[n_processes].pid, SIGSTOP);  
    } else {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    n_processes++;
  }

  print_processes(processes, n_processes);
  printf("done printing processes\n\n");


  /* Config and start timer */
  struct itimerval timer;
  timer.it_interval.tv_sec = quantum / 1000;
  timer.it_interval.tv_usec = (quantum % 1000) * 1000;
  timer.it_value = timer.it_interval;
  if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
    perror("setitimer");
    exit(EXIT_FAILURE);
  }


  /* Config signal logic */
  sigset_t waitset;
  int sig;

  sigemptyset(&waitset);
  sigaddset(&waitset, SIGALRM);
  sigaddset(&waitset, SIGCHLD);

  /* block */
  if (sigprocmask(SIG_BLOCK, &waitset, NULL) == -1) {
    perror("sigprocmask\n");
    exit(EXIT_FAILURE);
  }

  kill(processes[0].pid, SIGCONT);


  /* wait for signals */
  if (sigwait(&waitset, &sig) != 0) {
    perror("sigwait\n");
    exit(EXIT_FAILURE);
  }

  if (sig == SIGALRM) {
    printf("entered SIGALRM branch\n");
  } else if (sig == SIGCHLD) {
    printf("entered SIGCHLD branch\n");
  }

  
  return 0;
}

void print_processes(Process processes[], int n_processes) {
  printf("Number of Processes: %d\n", n_processes);

  int i;
  for (i = 0; i < n_processes; i++) {
    printf("Process %d\n", i+1);
    printf("\tCommand: %s\n", processes[i].cmd);
    printf("\tPID: %d\n", processes[i].pid);

    if (processes[i].args[0] != NULL) {
      printf("\tArguments: ");
      int j = 0;
      while (processes[i].args[j] != NULL) {
        printf("%s ", processes[i].args[j]);
        j++;
      }
      printf("\n");
    }
  }
}

void signal_handler(int signum, siginfo_t *si, void *unused) { 
  if (signum == SIGALRM) { 
    printf("IN signal_handler, SIGALRM\n");
    fflush(stdout);
  }
}
