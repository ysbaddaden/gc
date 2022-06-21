#include "segments.h"
#include "utils.h"

void Segments_init() {
    Array_init(&segments, 4);
}

void Segments_add_segment(void* start, void* end, char *name) {
    DEBUG("GC: Adding segment %p, %p, %s\n", start, end, name);
    Segment *segment = malloc(sizeof(Segment));
    segment->start = start;
    segment->end = end;
    segment->name = name;
    Array_push(&segments, segment);
}

void Segments_each(void *data, Segments_iterator_t callback) {
    for (int i = 0; i < Array_size(&segments); i++) {
        Segment *segment = Array_get(&segments, i);
        callback(data, segment);
    }
}
