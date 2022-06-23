.POSIX:

CRYSTAL = crystal
CRFLAGS = -Dgc_none --release

CFLAGS = $(CUSTOM) -g -fPIC -O3 -Wall -Wextra -pedantic -std=c99 -Iinclude -funwind-tables
LDFLAGS = $(PWD)/immix.a -lm -lpthread

OBJECTS = build/immix.o \
		  build/global_allocator.o \
		  build/local_allocator.o \
		  build/collector.o \
		  build/hash.o \
		  build/segments.o \
		  build/dynamic_loading.o

all: immix.a

immix.a: $(OBJECTS)
	$(AR) -rc immix.a $(OBJECTS)

build/%.o: src/%.c include/*.h
	@mkdir -p build
	$(CC) -c $(CFLAGS) -o $@ $<

samples/http_server: samples/http_server.cr immix.a src/*.cr
	$(CRYSTAL) build $< -o $@ $(CRFLAGS)

setup: phony
	wget https://raw.githubusercontent.com/silentbicycle/greatest/v1.5.0/greatest.h -O test/greatest.h
	wget https://raw.githubusercontent.com/silentbicycle/greatest/v1.5.0/contrib/greenest -O test/greenest
	chmod +x test/greenest

build/test-runner: immix.a test/*.c test/*.h
	$(CC) -rdynamic $(CFLAGS) -o build/test-runner test/runner.c $(LDFLAGS)

test: phony build/test-runner
	./build/test-runner $(TEST)

spec: phony
	crystal spec -Dgc_none

clean: phony
	rm -rf immix.a build samples/http_server

tasks: phony
	egrep 'TODO|FIXME|OPTIMIZE' include/*.h src/*.c

phony:
