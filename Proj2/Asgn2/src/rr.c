#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"

typedef struct Node {
    thread t;
    struct Node* next;
} Node;

Node* head = NULL;

/* function to add a thread to the scheduler */
void rr_admit(thread new_thread) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        exit(EXIT_FAILURE);
    }

    new_node->t = new_thread;

    if (head == NULL) {
        head = new_node;
        new_node->next = head; // Pointing to itself
    } else {
        Node* temp = head;
        while (temp->next != head) {
            temp = temp->next;
        }
        temp->next = new_node;
        new_node->next = head; // Closing the circle
    }
}

/* function to remove a specific thread from the scheduler */
void rr_remove(thread victim) {
    if (head == NULL) return;

    Node* temp = head;
    Node* prev = NULL;

    do {
        if (temp->t == victim) break;
        prev = temp;
        temp = temp->next;
    } while (temp != head);

    if (temp->t != victim) return; // Victim not found

    if (prev == NULL) { // Removing head
        if (temp->next == head) {
            head = NULL; // Only one node in the list
        } else {
            Node* last = head;
            while (last->next != head) {
                last = last->next;
            }
            head = head->next;
            last->next = head;
        }
    } else {
        prev->next = temp->next;
    }

    free(temp->t);
    free(temp);
}

/* function to select the next thread to be scheduled */
thread rr_next(void) {
    if (head == NULL) {
        return NULL;
    }

    thread t = head->t;
    head = head->next; // Move to the next thread in the circle

    return t;
}

/* function to return the number of threads in the scheduler */
int rr_qlen(void) {
    if (head == NULL) return 0;

    int count = 0;
    Node* temp = head;
    do {
        count++;
        temp = temp->next;
    } while (temp != head);

    return count;
}

struct scheduler rr_publish = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen}; 
scheduler sched = &rr_publish;
