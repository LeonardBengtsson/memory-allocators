//
// Created by Leonard on 2026-02-10.
//

#pragma once

#include <stddef.h>


void *buddy_alloc(size_t len);

void buddy_free(void *ptr);

void buddy_test();
