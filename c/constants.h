#ifndef GC_CONSTANTS_H
#define GC_CONSTANTS_H

#define BLOCK_SIZE ((size_t)32768)
#define BLOCK_SIZE_IN_BYTES_INVERSE_MASK (~(BLOCK_SIZE - 1))

#define LARGE_OBJECT_SIZE (size_t)8192

#endif
