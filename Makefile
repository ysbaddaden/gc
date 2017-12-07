.POSIX:

CRYSTAL = crystal
CRFLAGS = -Dgc_immix

test: src/ext/gc_immix_ld.o phony
	$(CRYSTAL) run test/*_test.cr -D gc_none -- --verbose --chaos --parallel=4

spec: src/ext/gc_immix_ld.o phony
	$(CRYSTAL) spec $(CRFLAGS)

valgrind:
	$(CRYSTAL) build spec/gc_spec.cr -o gc_spec $(CRFLAGS)
	valgrind ./gc_spec

src/ext/gc_immix_ld.o: src/ext/gc_immix_ld.c
	$(CC) -c src/ext/gc_immix_ld.c -o src/ext/gc_immix_ld.o

phony:
