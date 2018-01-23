#ifndef GC_CONFIG_H
#define GC_CONFIG_H

#define _BSD_SOURCE

#if defined(__linux__)
extern char __data_start[];
extern char __bss_start[];
extern char _end[];
#define GC_DATA_START &__data_start
#define GC_DATA_END &__bss_start
#define GC_BSS_START &__bss_start
#define GC_BSS_END &_end
#endif

#if defined(__OpenBSD__)
extern char __data_start[];
extern char __bss_start[];
extern char __end[];
#define GC_DATA_START &__data_start
#define GC_DATA_END &__bss_start
#define GC_BSS_START &__bss_start
#define GC_BSS_END &__end
#endif

#if defined(__FreeBSD__)
extern char etext;
extern char edata;
extern char end;
#define GC_DATA_START &etext
#define GC_DATA_END &edata
#define GC_BSS_START &edata
#define GC_BSS_END &end
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/getsect.h>
#define GC_DATA_START get_etext()
#define GC_DATA_END get_edata()
#define GC_BSS_START get_edata()
#define GC_BSS_END get_end()
#endif

#endif
