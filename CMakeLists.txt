cmake_minimum_required(VERSION 4.1)
project(memory_allocators C)

set(CMAKE_C_STANDARD 17)

add_executable(memory_allocators src/main.c
        src/alloc/buddy.c
        src/alloc/buddy.h
        src/alloc/pool.c
        src/alloc/pool.h)
