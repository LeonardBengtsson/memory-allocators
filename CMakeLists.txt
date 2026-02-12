cmake_minimum_required(VERSION 4.1)
project(leben_allocators C)

set(CMAKE_C_STANDARD 17)

add_executable(leben_allocators src/main.c
        src/alloc/buddy.c
        src/alloc/buddy.h
        src/alloc/pool.c
        src/alloc/pool.h)
