#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "lwp.h"
#include "schedulers.h"

tid_t tid_counter = 1;

tid_t lwp_create(lwpfun function, void *argument) {

    /* malloc for the thread context struct */
    thread new_thread = (thread) malloc(sizeof(new_thread));
    if (!new_thread) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }

    new_thread->tid = tid_counter;
    tid_counter++; 

    /* for now allocate just a bit of bytes for testing */
    int howbig = 8;

    new_thread->stack = mmap(NULL, howbig, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (new_thread->stack == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }
    new_thread->stacksize = howbig;

    unsigned long* endofstack;
    endofstack = new_thread->stack + howbig; 
    /* if end of stack isn't div by 16, adjust it by subtracting */
    if ((unsigned long)endofstack % 16 != 0) {
        endofstack = (unsigned long*)((unsigned long)endofstack - ((unsigned long)endofstack % 16));
    }

    /* push function to stack */
    *(endofstack - 1) = &function; 

    /* setup rfile regsiters */
    new_thread->state.rdi = &function;
    new_thread->state.rsi = argument;
    new_thread->state.fxsave = FPU_INIT;
    new_thread->state.rbp = (unsigned long) endofstack;
    new_thread->state.rsp = (unsigned long) (endofstack - 2);

    /* admit thread to scheduler */
    sched->admit(new_thread);

    return new_thread->tid;
}

static void lwp_wrap(lwpfun fun, void *arg) {
    int rval;
    rval = fun(arg);
    lwp_exit(rval); 
}

void lwp_exit(int status) {


}

tid_t lwp_gettid(void) {
    tid_t res;
    res = 10;
    return res;
}

void lwp_yield(void) {
    /* check if more threads in scheduler */
    thread nxt;
    nxt = sched->next(); 
    if (nxt == NULL) {
        /* ?? need to fix the exit status */
        exit(4);
    }

    /* what to put for old state? */
    swap_rfiles(NULL, &(nxt->state));

    /* should return to next's LWP's function */
    return;
}

void lwp_start(void) {
    /* allocate LWP context (not stack though) for original main thread */
    thread new_thread = (thread) malloc(sizeof(new_thread));
    if (!new_thread) {
        perror("malloc failed to allocate new thread context\n");
        return -1;
    }

    new_thread->tid = tid_counter;
    tid_counter++; 

    
    /* ?? for main LWP's rfile 'state', what do we put? nothing? */

    /* admit main thread to scheduler */
    sched->admit(new_thread);

    /* yield */


}

tid_t lwp_wait(int *in) {
    tid_t res; 
    res = 10;
    return res;
}

int lwpfuntest(void *a) {
    int res = (int) a + 1;
    printf("in lwpfuntext: %d\n", res);
    return res;
}
/* 
int main() {
    
    int value = 10;
    void *arg = &value;
    tid_t ret = lwp_create((lwpfun) lwpfuntest, arg);

    return 0;
}
*/
