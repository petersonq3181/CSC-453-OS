#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"
#include "schedulers.h"

tid_t tid_counter = 1;
thread thread_cur = NULL;

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

void freeQueueResources(Queue* q) {
    while (!isEmpty(q)) {
        thread w = dequeue(q);

        /* deallocate resources  */
        if (!(w->stack == NULL)) {
            if (munmap(w->stack, howbig) == -1) {
                perror("munmap failed\n");
                exit(EXIT_FAILURE);
            }   
        }
        free(w);
        w = NULL;
    }
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
        sched->remove(thread_cur);
    }
    
    /* record termination status of caller */
    int low8bits = status & 0xFF;
    // thread_cur->status = MKTERMSTAT(low8bits, thread_cur->status);
    thread_cur->status = low8bits;
    /* sets calling threads status to this new term status? */

    lwp_yield();
}

tid_t lwp_wait(int *status) {
    if (status == NULL) {
        sched->remove(thread_cur);
        enqueue(&terminated, thread_cur);
        lwp_yield();
    }

    if (sched->qlen() < 2) {
        freeQueueResources(&terminated);
        return NO_THREAD;
    }

    lwp_yield();

    if (!isEmpty(&terminated)) {
        thread w = dequeue(&terminated);
        tid_t ret = w->tid;

        *status = w->status;

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
    thread nxt;
    nxt = sched->next(); 

    /* check if more threads in scheduler */
    if (nxt == NULL) {
        /* if no nxt, terminate the program by calling exit(3) w/ term status of caller */
        exit(thread_cur->status);
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
    new_thread->stack = NULL; 

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
