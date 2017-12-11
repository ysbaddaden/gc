.POSIX:

CFLAGS = -g -fPIC -O3 -Wall -pedantic -std=c99 -D_BSD_SOURCE $(CUSTOM)
LDFLAGS = $(PWD)/immix.a -lm

OBJECTS = build/immix.o \
		  build/global_allocator.o

all: immix.a

immix.a: $(OBJECTS)
	$(AR) -rc immix.a build/immix.o build/global_allocator.o

build/%.o: %.c *.h
	@mkdir -p build
	$(CC) -c $(CFLAGS) -o $@ $<

clean: phony
	rm -rf immix.a build

setup: phony
	[ -f test/greatest.h ] && wget https://raw.githubusercontent.com/silentbicycle/greatest/v1.3.1/greatest.h -O test/greatest.h
	[ -f test/greenest ] && wget https://raw.githubusercontent.com/silentbicycle/greatest/v1.3.1/contrib/greenest -O test/greenest
	chmod +x test/greenest

test: phony immix.a
	$(CC) -rdynamic $(CFLAGS) -o build/test-runner test/runner.c $(LDFLAGS)
	./build/test-runner | ./test/greenest

phony:
