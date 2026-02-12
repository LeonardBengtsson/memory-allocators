//
// Created by Leonard on 2026-02-12.
//

#include "pool.h"

#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>


#define PAGE_LEVEL 12 // assume page size is 4 KiB = 2^12 bytes
#define PAGE_LEN (1 << PAGE_LEVEL)
#define ONES 0xffffffffffffffff

#define NEW_POOL_ALLOC_FAILED ((pool_alloc_t) { \
    .pool = NULL, \
    .el_len = 0, \
    .first = 0 \
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
    el_len = el_len < sizeof(pool_block_t) ? sizeof(pool_block_t) : el_len;
    if (el_len > PAGE_LEN) {
        return NEW_POOL_ALLOC_FAILED;
    }

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

void pool_test() {
    // create/delete allocator

    {
        pool_alloc_t failed = new_pool_alloc(PAGE_LEN + 1);
        assert(new_pool_alloc_failed(&failed));
    }

    {
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
}
