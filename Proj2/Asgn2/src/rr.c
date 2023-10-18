#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"

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

static struct scheduler fifo_scheduler = {
    .init = fifo_scheduler_init,
    .shutdown = fifo_scheduler_shutdown,
    .admit = fifo_scheduler_admit,
    .remove = fifo_scheduler_remove,
    .next = fifo_scheduler_next,
    .qlen = fifo_scheduler_qlen
};

scheduler sched = &fifo_scheduler;