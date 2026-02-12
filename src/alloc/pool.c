//
// Created by Leonard on 2026-02-12.
//

#include "pool.h"

#include <assert.h>
#include <sys/mman.h>


#define PAGE_LEVEL 12 // assume page size is 4 KiB = 2^12 bytes
#define PAGE_LEN (1 << PAGE_LEVEL)
#define ONES 0xffffffffffffffff

#define NEW_POOL_ALLOC_FAILED ((pool_alloc_t) { \
    .pool = NULL, \
    .el_len = 0, \
    .first_index = 0 \
})

typedef struct {
    // no next: -1
    // otherwise: offset - 1 in units of el_len
    short next_offset;
} pool_block_t;

pool_block_t *get_last_block(pool_alloc_t const *pool_alloc) {
    size_t index = PAGE_LEN / pool_alloc->el_len - 1;
    size_t offset = index * pool_alloc->el_len;
    return (pool_block_t *) ((size_t) pool_alloc->pool + offset);
}

pool_alloc_t new_pool_alloc(size_t el_len) {
    if (el_len == 0 || el_len > PAGE_LEN) {
        return NEW_POOL_ALLOC_FAILED;
    }
    el_len = el_len < sizeof(pool_block_t) ? sizeof(pool_block_t) : el_len;

    void *pool = mmap(
        NULL, PAGE_LEN, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (pool == MAP_FAILED) {
        return NEW_POOL_ALLOC_FAILED;
    }

    // page-aligned
    assert((((size_t) pool) & ~(ONES << PAGE_LEVEL)) == 0x0);

    // by default filled with free blocks, since mmap initializes to 0
    pool_alloc_t pool_alloc = { pool, el_len, 0 };

    // we just need to unset the last block's next block pointer
    pool_block_t *last_block = get_last_block(&pool_alloc);
    last_block->next_offset = -1;

    return pool_alloc;
}

bool new_pool_alloc_failed(pool_alloc_t const *pool_alloc) {
    return pool_alloc->pool == NULL;
}

int delete_pool_alloc(pool_alloc_t pool_alloc) {
    return munmap(pool_alloc.pool, PAGE_LEN);
}

short offset_to_index(short block_index, short offset) {
    if (offset == -1) {
        return -1;
    }
    return offset + block_index + 1;
}

short index_to_offset(short block_index, short index) {
    if (index == -1) {
        return -1;
    }
    return index - block_index - 1;
}

pool_block_t *get_block(pool_alloc_t *pool_alloc, short index) {
    if (index == -1) {
        return NULL;
    }
    size_t offset = index * pool_alloc->el_len;
    return (pool_block_t *) ((size_t) pool_alloc->pool + offset);
}

short get_index(pool_alloc_t *pool_alloc, pool_block_t *block) {
    if (block == NULL) {
        return -1;
    }
    size_t offset = (size_t) block - (size_t) pool_alloc->pool;
    return offset / pool_alloc->el_len;
}

pool_block_t *consume_first_block(pool_alloc_t *pool_alloc) {
    pool_block_t *first = get_block(pool_alloc, pool_alloc->first_index);
    if (first == NULL) {
        return NULL;
    }

    short new_first = offset_to_index(pool_alloc->first_index, first->next_offset);
    pool_alloc->first_index = new_first;

    return first;
}

void release_block(pool_alloc_t *pool_alloc, pool_block_t *block) {
    pool_block_t *new_first = block;
    short new_first_index = get_index(pool_alloc, new_first);

    *new_first = (pool_block_t) {
        index_to_offset(new_first_index, pool_alloc->first_index)
    };
    pool_alloc->first_index = new_first_index;
}

void *pool_alloc(pool_alloc_t *pool_alloc) {
    pool_block_t *first_block = consume_first_block(pool_alloc);
    return first_block;
}

void pool_free(pool_alloc_t *pool_alloc, void *ptr) {
    if (ptr == NULL) {
        return;
    }
    pool_block_t *block = ptr;
    release_block(pool_alloc, block);
}

void pool_test() {
    // create/delete allocator

    {
        // test invalid element sizes

        pool_alloc_t failed_1 = new_pool_alloc(0);
        pool_alloc_t failed_2 = new_pool_alloc(PAGE_LEN + 1);
        assert(new_pool_alloc_failed(&failed_1));
        assert(new_pool_alloc_failed(&failed_2));
    }

    {
        // test valid element sizes

        pool_alloc_t alloc_1 = new_pool_alloc(1);
        pool_alloc_t alloc_2 = new_pool_alloc(sizeof(pool_block_t));
        pool_alloc_t alloc_3 = new_pool_alloc(
            sizeof(pool_block_t) + 1);

        assert(alloc_1.el_len == sizeof(pool_block_t));
        assert(alloc_2.el_len == sizeof(pool_block_t));
        assert(alloc_3.el_len == sizeof(pool_block_t) + 1);

        assert(delete_pool_alloc(alloc_1) == 0);
        assert(delete_pool_alloc(alloc_2) == 0);
        assert(delete_pool_alloc(alloc_3) == 0);
    }

    {
        // test block offset initialization

        pool_alloc_t alloc = new_pool_alloc(4);
        assert(!new_pool_alloc_failed(&alloc));

        assert(alloc.el_len == 4);
        for (size_t i = 0; i < PAGE_LEN - 4; i += 4) {
            pool_block_t *block =
                (pool_block_t *) ((size_t) alloc.pool + i);
            assert(block->next_offset == 0);
        }
        pool_block_t *last_block =
            (pool_block_t *) ((size_t) alloc.pool + PAGE_LEN - 4);
        assert(last_block->next_offset == -1);

        assert(delete_pool_alloc(alloc) == 0);
    }

    // util functions

    {
        // test offset/index conversion

        assert(offset_to_index(2, -2) == 1);
        assert(offset_to_index(2, -1) == -1);
        assert(offset_to_index(2, 0) == 3);
        assert(index_to_offset(3, 2) == -2);
        assert(index_to_offset(3, -1) == -1);
        assert(index_to_offset(3, 3) == -1);
        assert(index_to_offset(3, 4) == 0);
    }

    {
        // test block referencing/dereferencing

        pool_alloc_t alloc = new_pool_alloc(4);

        assert(get_block(&alloc, -1) == NULL);
        assert(get_block(&alloc, 0) == alloc.pool);
        assert(get_block(&alloc, 20) == alloc.pool + 80);

        assert(get_index(&alloc, NULL) == -1);
        assert(get_index(&alloc, get_block(&alloc, 0)) == 0);
        assert(get_index(&alloc, get_block(&alloc, 5)) == 5);

        assert(delete_pool_alloc(alloc) == 0);
    }

    {
        // test linked list behavior

        pool_alloc_t alloc = new_pool_alloc(4);

        pool_block_t *block_1 = consume_first_block(&alloc);
        assert(alloc.first_index == 1);
        pool_block_t *block_2 = consume_first_block(&alloc);
        assert(alloc.first_index == 2);
        pool_block_t *block_3 = consume_first_block(&alloc);
        assert(alloc.first_index == 3);
        pool_block_t *block_4 = consume_first_block(&alloc);
        assert(alloc.first_index == 4);

        assert(block_1->next_offset == 0);
        assert(block_2->next_offset == 0);
        assert(block_3->next_offset == 0);
        assert(block_4->next_offset == 0);

        release_block(&alloc, block_1);
        assert(alloc.first_index == 0);
        assert(block_1->next_offset == 3);

        release_block(&alloc, block_4);
        assert(alloc.first_index == 3);
        assert(block_4->next_offset == -4);
        assert(block_1 ==
            get_block(&alloc, offset_to_index(
                get_index(&alloc, block_4), block_4->next_offset)));

        release_block(&alloc, block_2);
        assert(alloc.first_index == 1);
        assert(block_2->next_offset == 1);
        assert(block_4 ==
            get_block(&alloc, offset_to_index(
                get_index(&alloc, block_2), block_2->next_offset)));

        release_block(&alloc, block_3);
        assert(alloc.first_index == 2);
        assert(block_3->next_offset == -2);
        assert(block_2 ==
            get_block(&alloc, offset_to_index(
                get_index(&alloc, block_3), block_3->next_offset)));

        assert(delete_pool_alloc(alloc) == 0);
    }

    // alloc/free

    {
        // test that allocations don't overlap, and that behavior is
        // consistent regardless of deallocation order

        pool_alloc_t alloc = new_pool_alloc(sizeof(int));

        {
            int *ints[PAGE_LEN];
            for (int i = 0; i < PAGE_LEN; i++) {
                ints[i] = pool_alloc(&alloc);
                if (ints[i] != NULL) {
                    *ints[i] = i;
                }
            }

            int *null = pool_alloc(&alloc);
            assert(null == NULL);

            for (int i = PAGE_LEN; i >= 0; i--) {
                if (ints[i] != NULL) {
                    assert(*ints[i] == i);
                }
                pool_free(&alloc, ints[i]);
            }
        }

        {
            int *ints[PAGE_LEN];
            for (int i = 0; i < 30; i++) {
                ints[i] = pool_alloc(&alloc);
                assert(ints[i] != NULL);
                *ints[i] = 1 << i;
            }

            for (int i = 29; i >= 15; i--) {
                assert(ints[i] != NULL);
                assert(*ints[i] == 1 << i);
                pool_free(&alloc, ints[i]);
            }

            for (int i = 15; i < 30; i++) {
                ints[i] = pool_alloc(&alloc);
                assert(ints[i] != NULL);
                *ints[i] = 1 << i;
            }

            for (int i = 0; i < 30; i++) {
                assert(ints[i] != NULL);
                assert(*ints[i] == 1 << i);
                pool_free(&alloc, ints[i]);
            }
        }

        assert(delete_pool_alloc(alloc) == 0);
    }
}
