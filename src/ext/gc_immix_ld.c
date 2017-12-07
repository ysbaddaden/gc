// TODO: support platform disparity (works on x86-64-linux-gnu at least):

extern char __data_start[];
extern char __bss_start[];
extern char _end[];

struct gc_immix_ld_symbols {
    void *data_start;
    void *data_end;
    void *bss_start;
    void *bss_end;
} ;

void gc_immix_get_ld_symbols(struct gc_immix_ld_symbols *s) {
    s->data_start = &__data_start;
    s->data_end = &__bss_start;
    s->bss_start = &__bss_start;
    s->bss_end = &_end;
}
