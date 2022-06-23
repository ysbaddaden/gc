#ifndef __SEGMENTS_H__
#define __SEGMENTS_H__

#include "array.h"

typedef struct GC_segment {
  void *start;
  void *end;
  char *name;
} Segment;

typedef void (Segments_iterator_t)(void *, Segment *);

static Array segments;

void Segments_init();

void Segments_add_segment(void* start, void* end, char *name);

void Segments_each(void *data, Segments_iterator_t callback);
#endif
