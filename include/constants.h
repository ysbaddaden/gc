#ifndef GC_CONSTANTS_H
#define GC_CONSTANTS_H

// Each IMMIX block is 32KB, and sliced in 128 lines of 256 bytes each. The
// first line is reserved for block and line metadata, hence 127 lines are
// available.

#define BLOCK_SIZE ((size_t)32768)
#define BLOCK_SIZE_IN_BYTES_INVERSE_MASK (~(BLOCK_SIZE - 1))

#define LINE_SIZE 256
#define LINE_COUNT 127

// Objects of 8192 and more will be allocated to the large object space.
#define LARGE_OBJECT_SIZE (size_t)8192

// Grow the small object space by 30%.
#define GROWTH_RATE 30

#define WORD_SIZE (sizeof(void *))


// The following constants can be defined at runtime as environment variables of
// the same name, optionaly sufixed with a multiplier ('k', 'm' or 'g').

#define GC_INITIAL_HEAP_SIZE (4 * 1024 * 1024)
// #define GC_MAXIMUM_HEAP_SIZE
#define GC_FREE_SPACE_DIVISOR 3

#endif
