//
// Created by Leonard on 2026-02-12.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>


typedef struct {
    void *pool;
    size_t el_len;
    short first_index;
} pool_alloc_t;

pool_alloc_t new_pool_alloc(size_t el_len);

bool new_pool_alloc_failed(pool_alloc_t const *pool_alloc);

int delete_pool_alloc(pool_alloc_t pool_alloc);

void *pool_alloc(pool_alloc_t *alloc, size_t len);

void pool_free(pool_alloc_t *alloc, void *ptr);

void pool_test();
