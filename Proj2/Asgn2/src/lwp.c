#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"
#include "schedulers.h"

tid_t tid_counter = 1;
thread thread_cur = NULL;
int first_yield = 1;

/* for now allocate just a bit of bytes for testing */
int howbig = 2 * 1024 * 1024; 


/* ----- Node defs and funcs for FIFO queues of threads */
typedef struct Node {
    thread data;
    struct Node* next;
} Node;

typedef struct Queue {
    Node* front;
    Node* rear;
} Queue;

Queue waiting;
Queue terminated;

void initializeQueue(Queue* q) {
    q->front = NULL;
    q->rear = NULL;
}

void enqueue(Queue* q, thread item) {
    Node* newNode = (Node*) malloc(sizeof(Node));
    newNode->data = item;
    newNode->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
        return;
    }

    q->rear->next = newNode;
    q->rear = newNode;
}

thread dequeue(Queue* q) {
    if (q->front == NULL) {
        printf("Queue is empty.\n");
        exit(1);
    }

    Node* temp = q->front;
    thread item = temp->data;

    q->front = q->front->next;

    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(temp);
    return item;
}

thread peek(Queue* q) {
    if (q->front == NULL) {
        printf("Queue is empty.\n");
        exit(1);
    }
    return q->front->data;
}

int isEmpty(Queue* q) {
    return q->front == NULL;
}

void printQueue(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty.\n\n");
        return;
    }

    Node* currentNode = q->front;
    while (currentNode != NULL) {
        print_lwp(currentNode->data);
        currentNode = currentNode->next;
    }
    printf("\n\n");
}

Queue tq; 
void test_queue() {
    /* init dummy LWPs */
    thread new_thread_a = (thread) malloc(sizeof(*new_thread_a));
    if (!new_thread_a) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }
    new_thread_a->tid = 11;

    thread new_thread_b = (thread) malloc(sizeof(*new_thread_b));
    if (!new_thread_b) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }
    new_thread_b->tid = 12;

    thread new_thread_c = (thread) malloc(sizeof(*new_thread_c));
    if (!new_thread_c) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }
    new_thread_c->tid = 13;

    /* FIFO queue stuff */
    initializeQueue(&tq);

    printf("init queue: \n");
    printQueue(&tq);
    printf("queue is empty? \t\t %d\n", isEmpty(&tq));

    enqueue(&tq, new_thread_a);
    printf("queue after pushing thread_a: \n");
    printQueue(&tq);
    printf("queue is empty? \t\t %d\n", isEmpty(&tq));

    enqueue(&tq, new_thread_b);
    enqueue(&tq, new_thread_c);
    printf("queue after pushing thread_b and thread_c: \n");
    printQueue(&tq);
    printf("queue is empty? \t\t %d\n", isEmpty(&tq));

    thread p_a = dequeue(&tq);
    printf("popped from queue:\n");
    print_lwp(p_a);
    printf("\nqueue after popping:\n");
    printQueue(&tq);
    printf("queue is empty? \t\t %d\n", isEmpty(&tq));

    thread p_b = dequeue(&tq);
    printf("popped from queue:\n");
    print_lwp(p_b);
    printf("\nqueue after popping:\n");
    printQueue(&tq);
    printf("queue is empty? \t\t %d\n", isEmpty(&tq));

    thread p_c = dequeue(&tq);
    printf("popped from queue:\n");
    print_lwp(p_c);
    printf("\nqueue after popping:\n");
    printQueue(&tq);
    printf("queue is empty? \t\t %d\n", isEmpty(&tq));
}



/* ----- main LWP API funcs */
static void lwp_wrap(lwpfun fun, void *arg) {
    int rval;
    rval = fun(arg);
    lwp_exit(rval); 
}

tid_t lwp_create(lwpfun function, void *argument) {

    /* malloc for the thread context struct */
    thread new_thread = (thread) malloc(sizeof(*new_thread));
    if (!new_thread) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }

    new_thread->tid = tid_counter;
    tid_counter++; 

    new_thread->stack = mmap(NULL, howbig, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (new_thread->stack == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }
    new_thread->stacksize = howbig;

    unsigned long* endofstack;
    endofstack = (unsigned long*)((char*)new_thread->stack + howbig);

    /* if end of stack isn't div by 16, adjust it by subtracting */
    if ((unsigned long)endofstack % 16 != 0) {
        endofstack = (unsigned long*)((unsigned long)endofstack - ((unsigned long)endofstack % 16));
    }

    /* push function to stack */
    *(endofstack - 1) = (unsigned long) lwp_wrap;

    /* setup rfile regsiters */
    new_thread->state.rdi = function;
    new_thread->state.rsi = argument;
    new_thread->state.fxsave = FPU_INIT;
    new_thread->state.rbp = (unsigned long) (endofstack - 2);
    new_thread->state.rsp = (unsigned long) (endofstack - 2);

    /* admit thread to scheduler */
    sched->admit(new_thread);

    /* for debugging */
    /* 
    printf("new  tid: %d\n", new_thread->tid);
    printf("new_thread->stack: %p\n", new_thread->stack);
    printf("endofstack (after adjustment): %p\n", endofstack);
    printf("new_thread->state.rdi: %p\n", new_thread->state.rdi);
    printf("new_thread->state.rsi: %p\n", new_thread->state.rsi);
    printf("new_thread->state.rbp: %lu\n", new_thread->state.rbp);
    printf("new_thread->state.rsp: %lu\n\n\n", new_thread->state.rsp);
    fflush(stdout);
    */
    
    return new_thread->tid;
}

void lwp_exit(int status) {
    if (!isEmpty(&waiting)) {
        thread w = dequeue(&waiting);
        w->exited = thread_cur;
        sched->admit(w);
    } else {
        enqueue(&terminated, thread_cur);

        /* added for debugging purposes, strangely things are wokring better */
        thread g = dequeue(&terminated);

        sched->remove(thread_cur);
    }
    
    /* record termination status of caller */
    int low8bits = status & 0xFF;
    thread_cur->status = MKTERMSTAT(low8bits, thread_cur->status);
    /* sets calling threads status to this new term status? */

    lwp_yield();
}

tid_t lwp_wait(int *status) {
    if (sched->qlen() < 2) {
        return NO_THREAD;
    }

    if (!isEmpty(&terminated)) {
        thread w = dequeue(&terminated);
        tid_t ret = w->tid;

        /* deallocate resources  */
        if (!(w->stack == NULL)) {
            if (munmap(w->stack, howbig) == -1) {
                perror("munmap failed\n");
                exit(EXIT_FAILURE);
            }   
        }
        free(w);
        w = NULL;
        
        return ret;
    }

    sched->remove(thread_cur);
    enqueue(&waiting, thread_cur);

    lwp_yield();

    *status = thread_cur->status;

    return thread_cur->tid;
}

tid_t lwp_gettid(void) {
    if (thread_cur == NULL) {
        return NO_THREAD; 
    }

    tid_t res;
    res = thread_cur->tid;
    return res;
}

thread tid2thread(tid_t tid) {
    if (sched->qlen() == 0) return NULL;

    int original_length = sched->qlen();
    int i = 0;

    thread current_thread = NULL;

    while (i < original_length) {
        current_thread = sched->next();
        if (current_thread->tid == tid) {
            return current_thread;
        }
        i++;
    }

    return NULL;
}

void lwp_yield(void) {

    /* check if more threads in scheduler */
    thread nxt;
    nxt = sched->next(); 
    if (nxt == NULL) {
        /* need to fix the exit status */
        exit(-1);
    }
    
    /* 
    printf("yield 1\n");
    printf("nxt->state.rdi: %p\n", nxt->state.rdi);
    printf("nxt->state.rsi: %p\n", nxt->state.rsi);
    printf("nxt->state.rbp: %lu\n", nxt->state.rbp);
    printf("nxt->state.rsp: %lu\n", nxt->state.rsp);
    printf("yield 2\n");
    printf("thread_cur->state.rdi: %p\n", thread_cur->state.rdi);
    printf("thread_cur->state.rsi: %p\n", thread_cur->state.rsi);
    printf("thread_cur->state.rbp: %lu\n", thread_cur->state.rbp);
    printf("thread_cur->state.rsp: %lu\n", thread_cur->state.rsp);
    fflush(stdout);
    */


    thread tmp = thread_cur;
    thread_cur = nxt;

    // printf("in yield got here\n");
    // print_thread_cur();

    swap_rfiles(&tmp->state, &nxt->state);
}

void lwp_start(void) {

    /* init queues */
    initializeQueue(&waiting);
    initializeQueue(&terminated);

    /* allocate LWP context (not stack though) for original main thread */
    thread new_thread = (thread) malloc(sizeof(*new_thread));
    if (!new_thread) {
        perror("malloc failed to allocate new thread context\n");
        return;
    }

    new_thread->tid = tid_counter;
    tid_counter++; 

    thread_cur = new_thread;

    /* admit main thread to scheduler */
    sched->admit(new_thread);

    /* yield */
    lwp_yield();
}



/* ----- Additional self-test functions */

void lwp_test4(void) {
    printf("Number of threads in the scheduler: %d\n", sched->qlen());

    tid_t tid_search = 2;
    thread result_thread = tid2thread(tid_search);
    if (result_thread) {
        printf("Thread with tid %d found!\n", tid_search);
    } else {
        printf("Thread with tid %d not found.\n", tid_search);
    }

    printf("Current thread tid: %d\n", lwp_gettid());

    sched->remove(result_thread);
    printf("Number of threads after removing: %d\n", sched->qlen());
}


void print_thread_cur() {
    printf("thread_cur tid: %d\n", thread_cur->tid);
    fflush(stdout);
}

void print_lwp(thread t) {
    printf("tid: %d\n", t->tid);
    printf("address pointed by t: %p\n", t);
    printf("thread->stack: %p\n", t->stack);
    printf("thread->state.rdi: %p\n", t->state.rdi);
    printf("thread->state.rsi: %p\n", t->state.rsi);
    fflush(stdout);
}
