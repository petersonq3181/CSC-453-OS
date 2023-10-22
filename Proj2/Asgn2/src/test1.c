#include <sys/resource.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    struct rlimit rl;
    size_t stackSize;
    const size_t defaultStackSize = 8 * 1024 * 1024; /* 8 MB */ 
    const size_t pageSize = sysconf(_SC_PAGE_SIZE);

    /* Get the current RLIMIT_STACK value */
    if (getrlimit(RLIMIT_STACK, &rl) == -1) {
        perror("getrlimit failed");
        exit(EXIT_FAILURE);
    }

    /* Check if RLIMIT_STACK is infinite or does not exist, use default */
    if (rl.rlim_cur == RLIM_INFINITY) {
        stackSize = defaultStackSize;
    } else {
        stackSize = rl.rlim_cur;
    }

    /* Ensure stackSize is a multiple of the system's page size */
    if (stackSize % pageSize != 0) {
        stackSize = (stackSize + pageSize - 1) & ~(pageSize - 1);
    }

    printf("%d\n", stackSize);
    printf("Chosen stack size: %zu bytes\n", stackSize);


    return 0;
}