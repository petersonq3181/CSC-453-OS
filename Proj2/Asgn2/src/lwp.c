#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"
#include "schedulers.h"

tid_t tid_counter = 1;
thread thread_main = NULL;

tid_t tid_cur = 0;
thread thread_cur = NULL;


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

    if(q->rear == NULL) {
        q->front = q->rear = newNode;
        return;
    }

    q->rear->next = newNode;
    q->rear = newNode;
}

thread dequeue(Queue* q) {
    if(q->front == NULL) {
        printf("Queue is empty.\n");
        exit(1);
    }

    Node* temp = q->front;
    thread item = temp->data;

    q->front = q->front->next;

    if(q->front == NULL) {
        q->rear = NULL;
    }

    free(temp);
    return item;
}

thread peek(Queue* q) {
    if(q->front == NULL) {
        printf("Queue is empty.\n");
        exit(1);
    }
    return q->front->data;
}

int isEmpty(Queue* q) {
    return q->front == NULL;
}

void printContext(thread c) {
    if (c == NULL) {
        printf("NULL thread\n");
        return;
    }
    printf("TID: %d\n", c->tid);
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

    /* for now allocate just a bit of bytes for testing */
    int howbig = 2 * 1024 * 1024; 

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
        w->exited = thread_cur->tid;
        sched->admit(w);
    } else {
        enqueue(&terminated, thread_cur);
        sched->remove(thread_cur);
    }

    /* record termination status of caller */
    int low8bits = status & 0xFF;

    lwp_yield();
}

tid_t lwp_wait(int *in) {
    if (!isEmpty(&terminated)) {
        thread w = dequeue(&terminated);
        tid_t ret = w->tid;

        /* deallocate resources */
        free(w);
        w = NULL;

        return ret;
    }

    sched->remove(thread_cur);
    enqueue(&waiting, thread_cur);
    lwp_yield();
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
        /* ?? need to fix the exit status */
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

    swap_rfiles(&thread_cur->state, &nxt->state);
}

void lwp_start(void) {

    /* init queues */
    initializeQueue(&waiting);
    initializeQueue(&terminated);

    /* allocate LWP context (not stack though) for original main thread */
    thread new_thread = (thread) malloc(sizeof(new_thread));
    if (!new_thread) {
        perror("malloc failed to allocate new thread context\n");
        return;
    }

    new_thread->tid = tid_counter;
    tid_counter++; 

    /* store in global to keep easy track of main thread
    * and set it to the current thread 
    */
    thread_main = new_thread;
    thread_cur = new_thread;

    /* admit main thread to scheduler */
    sched->admit(new_thread);

    /* yield */
    lwp_yield();
}



/* ----- Additional self-test functions */
void lwp_test(void) {
    printf("Enterting in lwp_test\n");
    fflush(stdout);
    
    printf("sched length: %d\n", sched->qlen());

    int i;
    thread nxt;
    for (i = 0; i < 10; i++) {
        nxt = sched->next();
        printf("next tid: %d \t rsi: %lu \t rsp: %lu \n", nxt->tid, nxt->state.rsi, nxt->state.rsp);
        fflush(stdout);
    }
    printf("exiting lwp_test!\n\n");
}

void lwp_test2(void) {
    printf("in lwp_test2\n");
    thread nxt;
    nxt = sched->next(); 
    printf("nxt: %d\n", nxt->tid);
    if (nxt == NULL) {
        /* ?? need to fix the exit status */
        exit(-1);
    }
    printf("got here in lwp_test2\n");
}

void lwp_test3(void) {
    Queue queue1;
    Queue queue2;

    initializeQueue(&queue1);
    initializeQueue(&queue2);

    // Sample context data for testing
    thread t1 = (thread) malloc(sizeof(context));
    t1->tid = 1;

    thread t2 = (thread) malloc(sizeof(context));
    t2->tid = 2;

    thread t3 = (thread) malloc(sizeof(context));
    t3->tid = 3;

    enqueue(&queue1, t1);
    enqueue(&queue1, t2);
    enqueue(&queue2, t3);

    printf("Peek at front of queue1:\n");
    printContext(peek(&queue1));

    printf("\nDequeuing from queue1:\n");
    while (!isEmpty(&queue1)) {
        thread t = dequeue(&queue1);
        printContext(t);
        free(t);  // Since each node holds a pointer to a context, we need to free the context memory when done.
    }

    printf("\nDequeuing from queue2:\n");
    while (!isEmpty(&queue2)) {
        thread t = dequeue(&queue2);
        printContext(t);
        free(t);  // Free the context memory.
    }

    return 0;
}

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

void print_thread(thread t) {
    if (t) {
        printf("Thread TID: %d\n", t->tid);
    } else {
        printf("NULL Thread\n");
    }
}

void print_queue(Queue* q, const char* queue_name) {
    printf("---- %s Queue ----\n", queue_name);
    if (!q || !q->front) {
        printf("(Empty)\n");
        return;
    }

    Node* temp = q->front;
    while (temp) {
        print_thread(temp->data);
        temp = temp->next;
    }
}

void print_thread_state() {
    printf("========== Thread State ==========\n");

    printf("---- Current Thread ----\n");
    print_thread(thread_cur);

    print_queue(&waiting, "Waiting");

    print_queue(&terminated, "Terminated");

    printf("==================================\n");
    fflush(stdout);
}