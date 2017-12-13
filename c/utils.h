#ifndef GC_UTILS_H
#define GC_UTILS_H

#define ROUND_TO_NEXT_MULTIPLE(size, multiple) \
    (((size) + (multiple) - 1) / (multiple) * (multiple))

#ifdef GC_DEBUG
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG(...)
#endif

#endif
