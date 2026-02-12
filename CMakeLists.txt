cmake_minimum_required(VERSION 4.1)
project(leben_allocators C)

set(CMAKE_C_STANDARD 17)

add_executable(leben_allocators main.c
        buddy.c
        buddy.h
        pool.c
        pool.h)
