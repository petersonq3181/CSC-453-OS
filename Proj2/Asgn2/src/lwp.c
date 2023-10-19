#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"
#include "schedulers.h"

tid_t tid_counter = 1;
thread* thread_main = NULL;

tid_t tid_cur = 0;
thread thread_cur = NULL;

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

    printf("new  tid: %d\n", new_thread->tid);
    printf("new_thread->stack: %p\n", new_thread->stack);


    unsigned long* endofstack;
    endofstack = (unsigned long*)((char*)new_thread->stack + howbig);

    printf("endofstack (before adjustment): %p\n", endofstack);

    /* if end of stack isn't div by 16, adjust it by subtracting */
    if ((unsigned long)endofstack % 16 != 0) {
        endofstack = (unsigned long*)((unsigned long)endofstack - ((unsigned long)endofstack % 16));
    }

    printf("endofstack (after adjustment): %p\n", endofstack);


    /* push function to stack */
    *(endofstack - 1) = (unsigned long) lwp_wrap;

    /* setup rfile regsiters */
    new_thread->state.rdi = function;
    new_thread->state.rsi = argument;
    new_thread->state.fxsave = FPU_INIT;
    new_thread->state.rbp = (unsigned long) (endofstack - 2);
    new_thread->state.rsp = (unsigned long) (endofstack - 2);

    printf("new_thread->state.rdi: %p\n", new_thread->state.rdi);
    printf("new_thread->state.rsi: %p\n", new_thread->state.rsi);
    printf("new_thread->state.rbp: %lu\n", new_thread->state.rbp);
    printf("new_thread->state.rsp: %lu\n\n\n", new_thread->state.rsp);
    fflush(stdout);

    /* admit thread to scheduler */
    sched->admit(new_thread);

    return new_thread->tid;
}

void lwp_exit(int status) {


}

tid_t lwp_gettid(void) {
    tid_t res;
    res = 1;
    return res;
}

void lwp_test(void) {
    printf("YOOO in lwp_test\n");
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

void lwp_yield(void) {

    printf("yield 0 \n");
    fflush(stdout);

    /* check if more threads in scheduler */
    thread nxt;
    nxt = sched->next(); 
    if (nxt == NULL) {
        /* ?? need to fix the exit status */
        exit(-1);
    }
    
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

  
    swap_rfiles(&thread_cur->state, &nxt->state);

    printf("yield 3\n");
    fflush(stdout);

    /* should return to next's LWP's function */
    /* return; */ 
    pause(20);
}

void lwp_start(void) {

    printf("start 0\n");
    fflush(stdout);

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

tid_t lwp_wait(int *in) {
    tid_t res; 
    res = 10;
    return res;
}
