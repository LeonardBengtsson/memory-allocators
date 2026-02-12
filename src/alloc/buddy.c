//
// Created by Leonard on 2026-02-10.
//

#include "buddy.h"

#include <sys/mman.h>
#include <assert.h>
#include <stdbool.h>


#define MIN_LEVEL 3
#define PAGE_LEVEL 12 // assume page size is 4 KiB = 2^12 bytes

#define PAGE_LEN (1 << PAGE_LEVEL)
#define ONES 0xffffffffffffffff

typedef struct {
    bool taken;
    short level;
} head_t;

static_assert(
    1 << MIN_LEVEL >= sizeof(head_t),
    "Min block size should be at least size of block head");

static head_t *g_top_block = NULL;

head_t *new_block() {
    head_t *block = mmap(
        NULL, PAGE_LEN, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (block == MAP_FAILED) {
        return NULL;
    }

    // page-aligned
    assert((((size_t) block) & ~(ONES << PAGE_LEVEL)) == 0x0);

    block->level = PAGE_LEVEL;
    return block;
}

head_t *get_next(head_t const *block) {
    return (head_t *) ((size_t) block + (0x1 << block->level));
}

bool within_block(head_t *block, head_t *ptr) {
    return (size_t) ptr < (size_t) block + PAGE_LEN;
}

head_t *get_buddy(head_t const *block) {
    return (head_t *) ((size_t) block ^ (0x1 << block->level));
}

head_t *split(head_t *block) {
    short new_level = block->level - 1;
    *block = (head_t) { .taken = false, .level = new_level};
    head_t *other = (head_t *) ((size_t) block | (0x1 << new_level));
    *other = *block;
    return other;
}

head_t *merge(head_t *block) {
    long mask = ONES << (block->level + 1);
    head_t *primary = (head_t *) ((size_t) block & mask);
    primary->level++;
    return primary;
}

short get_level(size_t alloc_len) {
    size_t total_len = alloc_len + sizeof(head_t);
    // equivalent to ceil(log2(total_len))
    // total_len - 1 > 0 is guaranteed
    int level = 8 * sizeof(long) - __builtin_clzl((long) total_len - 1);
    return level < MIN_LEVEL ? MIN_LEVEL : level;
}

void *buddy_alloc(size_t len) {
    if (len == 0) {
        return NULL;
    }
    short level = get_level(len);
    if (level > PAGE_LEVEL) {
        // TODO if len > PAGE_LEN/2 - sizeof(head_t), return a whole page
        // TODO if len > PAGE_LEN, use huge pages instead
        return NULL;
    }

    if (g_top_block == NULL) {
        g_top_block = new_block();
        if (g_top_block == NULL) {
            return NULL;
        }
    }

    // find smallest free block that fits the allocation
    head_t *smallest = NULL;
    for (
        head_t *current = g_top_block;
        within_block(g_top_block, current);
        current = get_next(current)
    ) {
        if (!current->taken &&
            current->level >= level &&
            (smallest == NULL || current->level < smallest->level)
        ) {
            smallest = current;
        }
    }
    if (smallest == NULL) {
        // TODO allow multiple top level blocks
        return NULL;
    }

    while (level < smallest->level) {
        split(smallest);
    }

    smallest->taken = true;
    return smallest + 1;
}

void buddy_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    head_t *head = (head_t *) ptr - 1;
    head->taken = false;
    while (head->level < PAGE_LEVEL && !get_buddy(head)->taken) {
        head = merge(head);
        head->taken = false;
    }
}

void buddy_test() {
    // util functions

    {
        // test block splitting and merging

        head_t *block = new_block();
        assert(block != NULL); // required for rest of tests to pass
        assert(block->level == PAGE_LEVEL);

        head_t *buddy_1 = split(block);
        assert(block->level == PAGE_LEVEL - 1);
        assert(buddy_1->level == PAGE_LEVEL - 1);
        assert((size_t) buddy_1 == (size_t) block + PAGE_LEN / 2);
        assert(get_buddy(block) == buddy_1);
        assert(get_buddy(buddy_1) == block);
        assert(get_next(block) == buddy_1);

        head_t *buddy_2 = split(block);
        assert(block->level == PAGE_LEVEL - 2);
        assert(buddy_1->level == PAGE_LEVEL - 1);
        assert(buddy_2->level == PAGE_LEVEL - 2);
        assert((size_t) buddy_2 == (size_t) block + PAGE_LEN / 4);
        assert(get_buddy(block) == buddy_2);
        assert(get_buddy(buddy_1) == block);
        assert(get_next(block) == buddy_2);
        assert(get_next(buddy_2) == buddy_1);

        head_t *block_1 = merge(buddy_2);
        assert(block->level == PAGE_LEVEL - 1);
        assert(buddy_1->level == PAGE_LEVEL - 1);
        assert(block == block_1);

        head_t *block_2 = merge(buddy_1);
        assert(block->level == PAGE_LEVEL);
        assert(block == block_2);
    }

    {
        // test correct block levels for various allocation lengths

        assert(get_level(0) == MIN_LEVEL);
        assert(get_level(sizeof(head_t)) == MIN_LEVEL);
        assert(get_level(200 - sizeof(head_t)) == 8);
        assert(get_level(256 - sizeof(head_t)) == 8);
        assert(get_level(257 - sizeof(head_t)) == 9);
    }

    // alloc/free

    {
        // test invalid allocation lengths

        void *null_1 = buddy_alloc(PAGE_LEN);
        void *null_2 = buddy_alloc(0);
        assert(null_1 == NULL);
        assert(null_2 == NULL);
    }

    {
        // test that allocations don't overlap, and that

        int *ints[10];
        for (int i = 0; i < 10; i++) {
            ints[i] = buddy_alloc(sizeof(int));
            assert(ints[i] != NULL);
            *ints[i] = i;
            for (int j = 0; j < i; j++) {
                assert(*ints[j] == j);
            }
        }

        {
            // test that no SEGFAULTS occur when out of memory

            char *chars[10000];
            for (int i = 0; i < 10000; i++) {
                chars[i] = buddy_alloc(sizeof(char));
            }

            for (int i = 0; i < 10000; i++) {
                buddy_free(chars[i]);
            }
        }

        for (int i = 0; i < 10; i++) {
            buddy_free(ints[i]);
        }
    }

    {
        // test that the expected number of allocations of different sizes
        // fit in memory, and that values are stored and retrieved correctly

        assert(PAGE_LEN >= 4096);
        void *diff_sizes[10];
        for (int i = 0; i < 10; i++) {
            int size = 1 << i;
            diff_sizes[i] = buddy_alloc(size);
            char *char_ptr = diff_sizes[i];
            for (int j = 0; j < size; j++) {
                char_ptr[j] = (char) j;
            }
            assert(diff_sizes[i] != NULL);
            for (int k = 0; k < 1000; k++) {
                void *temp = buddy_alloc(size);
                assert(temp != NULL);
                buddy_free(temp);
            }
        }

        for (int i = 0; i < 10; i++) {
            int size = 1 << i;
            char *char_ptr = diff_sizes[i];
            for (int j = 0; j < size; j++) {
                assert(char_ptr[j] == (char) j);
            }
            buddy_free(diff_sizes[i]);
        }
    }

    {
        // test that allocator is restored after all memory is deallocated

        assert(g_top_block->taken == false);
        assert(g_top_block->level == PAGE_LEVEL);
    }
}
