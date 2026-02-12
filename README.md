# leben-allocators

Implementations of two standard memory allocator types.

## Buddy allocator

Interface is declared in `buddy.h`. `buddy_alloc` and `buddy_free` follow the same contracts as `malloc` and `free`, respectively.

Implemented in `buddy.c`.

## Usage

Setup CMake: `cmake -B build/`

Run tests: `cmake --build build/ && build/leben-allocators`

