# leben-allocators

Implementations of two standard memory allocator types.

## Buddy allocator

Interface is declared in `src/alloc/buddy.h`. Use `buddy_alloc` and `buddy_free` for memory allocation and deallocation, respectively.

Allows allocation of differently sized regions, and finds regions similar in size to the requested size. `buddy_alloc` will return `NULL` if no free region can be found.

Implementation in `src/alloc/buddy.c`. Loosely based off of [https://people.kth.se/~johanmon/ose/assignments/buddy.pdf](https://people.kth.se/~johanmon/ose/assignments/buddy.pdf).

## Pool allocator

Interface is declared in `src/alloc/pool.h`. Use `pool_alloc` and `pool_free` for memory allocation and deallocation, respectively.

Works by creating an allocator with a specific block size, which can allocate regions guaranteed to be that specific size or larger. Use `new_pool_alloc` to create an allocator, check for success using `new_pool_alloc_failed`, and delete the allocator using `delete_pool_alloc`, freeing all owned memory. `pool_alloc` will return `NULL` if no free block can be found.

Implementation in `src/alloc/pool.c`.

## Usage

Setup CMake: `cmake -B build/`

Run tests: `cmake --build build/ && build/leben-allocators`

## Future developments

Both the buddy allocator and the pool allocator are currently limited to a memory region of one page (4 kiB). They could both be improved by allowing for using multiple pages, and optionally, using 'huge' pages (2 MiB) to facilitate larger allocation sizes.

The buddy allocator could also be refactored to use a `buddy_alloc_t` struct to contain an instance of an allocator, instead of the current implementation which uses a single global pointer.
