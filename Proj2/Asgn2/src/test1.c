#include <stdio.h>

/* for brief simple testing of checking div by 16 and adjusting */

int main() {

    /* ----- test w/ unsigned long (non-pointer )*/
    /*
    unsigned long value = 0x7ffff7ff8019; 

    if (value & 0xF) {
        value = value & ~0xF;
        printf("Adjusted value: %lx\n", value);
    } else {
        printf("Value is already divisible by 16: %lx\n", value);
    }
    */ 

    /* ----- test w/ unsigned long (as pointer) */
    unsigned long* ptr = 0x7ffff7ff8010; 

    if ((unsigned long)ptr % 16 != 0) {
        // The address is not divisible by 16
        ptr = (unsigned long*)((unsigned long)ptr - ((unsigned long)ptr % 16));
    }

    return 0;
}
