#include <stdio.h>

#include "alloc/buddy.h"
#include "alloc/pool.h"

int main(void) {
    printf("Buddy allocator tests...\n");
    buddy_test();

    printf("Pool allocator tests...\n");
    pool_test();

    printf("All tests successful!\n");
    return 0;
}
