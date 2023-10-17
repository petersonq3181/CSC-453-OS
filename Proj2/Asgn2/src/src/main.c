#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"

tid_t tid_counter = 1;

typedef struct Node {
    thread t;
    struct Node* next;
} Node;

static Node* head = NULL;
static Node* tail = NULL;

/* function to initialize scheduler (can be NULL) */
static void fifo_scheduler_init(void) {
    head = tail = NULL;
}

/* function to shutdown scheduler and free memory (can be NULL) */
static void fifo_scheduler_shutdown(void) {
    Node* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp->t); 
        free(temp);    
    }
    tail = NULL;
}

/* function to add a thread to the scheduler */
static void fifo_scheduler_admit(thread new_thread) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        exit(EXIT_FAILURE);
    }

    new_node->t = new_thread;
    new_node->next = NULL;

    if (tail == NULL) {
        head = tail = new_node;
    } else {
        tail->next = new_node;
        tail = new_node;
    }
}

/* function to remove a specific thread from the scheduler */
static void fifo_scheduler_remove(thread victim) {
    Node* temp = head;
    Node* prev = NULL;

    while (temp != NULL && temp->t != victim) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        return;
    }

    if (prev == NULL) {
        head = temp->next;
        if (head == NULL) {
            tail = NULL;
        }
    } else {
        prev->next = temp->next;
        if (temp->next == NULL) {
            tail = prev;
        }
    }
    free(temp->t);
    free(temp);
}

/* function to select the next thread to be scheduled */
static thread fifo_scheduler_next(void) {
    if (head == NULL) {
        return NULL;
    }

    Node* temp = head;
    thread t = temp->t;
    head = head->next;

    if (head == NULL) {
        tail = NULL;
    }
    free(temp);
    return t;
}

/* function to return the number of threads in the scheduler */
static int fifo_scheduler_qlen(void) {
    int count = 0;
    Node* temp = head;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

/* define and initialize the scheduler struct */
static struct scheduler fifo_scheduler = {
    .init = fifo_scheduler_init,
    .shutdown = fifo_scheduler_shutdown,
    .admit = fifo_scheduler_admit,
    .remove = fifo_scheduler_remove,
    .next = fifo_scheduler_next,
    .qlen = fifo_scheduler_qlen
};
scheduler sched = &fifo_scheduler;

tid_t lwp_create(lwpfun function, void *argument) {
    thread new_thread = (thread) malloc(sizeof(new_thread));
    if (!new_thread) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }

    new_thread->tid = tid_counter;
    tid_counter++; 

    int howbig = 8 * 1024 * 1024;

    new_thread->stack = mmap(NULL, howbig, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (new_thread->stack == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }
    new_thread->stacksize = howbig; 

    /* ? push function name (pointer to func body) to LWP's stack */
    *(new_thread->stack) = function;

    /* ? */
    new_thread->state.rdi = function;
    new_thread->state.rsi = argument;
    new_thread->state.fxsave = FPU_INIT;

    /* ? setup rfile to 'return to' LWP's stack */
    new_thread->state.rbp = (unsigned long) new_thread->stack;
    new_thread->state.rsp = (unsigned long) (new_thread->stack) + (howbig - 1);

    /* admit thread to scheduler */
    sched->admit(new_thread);

    return new_thread->tid;
}

int lwpfuntest(void *a) {
    int res = (int) a + 1;
    return res;
}

int main() {
    
    int value = 10;
    void *arg = &value;
    tid_t ret = lwp_create((lwpfun) lwpfuntest, arg);


    return 0;
}