.POSIX:
.SUFFIXES:

CRYSTAL = crystal
CRFLAGS = -Dgc_none

test: phony
	$(CRYSTAL) run test/*_test.cr $(CRFLAGS) -- --verbose --chaos --parallel=4

spec: phony
	$(CRYSTAL) spec $(CRFLAGS)

valgrind:
	$(CRYSTAL) build spec/gc_spec.cr -o gc_spec $(CRFLAGS)
	valgrind ./gc_spec

phony:
