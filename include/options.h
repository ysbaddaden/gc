#ifndef GC_OPTIONS_H
#define GC_OPTIONS_H

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include "constants.h"
#include "memory.h"

static inline size_t GC_getSizeFromEnvironmentVariable(const char *name, long default_value) {
    char *str = getenv(name);

    if (str != NULL) {
        char *endptr;
        size_t value = strtoul(str, &endptr, 10);

        if ((errno == ERANGE && (value == ULONG_MAX || value == 0))
                || (errno != 0 && value == 0)
                || endptr == str) {
            fprintf(stderr, "GC: ignoring invalid %s value", name);
            return default_value;
        }

        switch (*endptr) {
            case 'k':
            case 'K':
                return value * 1024;
            case 'm':
            case 'M':
                return value * 1024 * 1024;
            case 'g':
            case 'G':
                return value * 1024 * 1024 * 1024;
        }
        return value;
    }

    return default_value;
}

static inline size_t GC_getIntegerFromEnvironmentVariable(const char *name, long default_value) {
    char *str = getenv(name);

    if (str != NULL) {
        size_t value = strtoul(str, NULL, 10);

        if ((errno == ERANGE && (value == ULONG_MAX || value == 0))
                || (errno != 0 && value == 0)) {
            fprintf(stderr, "GC: ignoring invalid %s value", name);
            return default_value;
        }
    }
    return default_value;
}

static inline size_t GC_initialHeapSize() {
    return GC_getSizeFromEnvironmentVariable("GC_INITIAL_HEAP_SIZE", GC_INITIAL_HEAP_SIZE);
}

static inline size_t GC_maximumHeapSize() {
    return GC_getSizeFromEnvironmentVariable("GC_MAXIMUM_HEAP_SIZE", GC_getMemoryLimit());
}

static inline size_t GC_freeSpaceDivisor() {
    return GC_getIntegerFromEnvironmentVariable("GC_FREE_SPACE_DIVISOR", GC_FREE_SPACE_DIVISOR);
}

#endif
