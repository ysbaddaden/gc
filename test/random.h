#ifndef GC_RANDOM_H
#define GC_RANDOM_H

#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <errno.h>

static int GC_random_fd;

static void GC_random_init() {
    GC_random_fd = open("/dev/urandom", O_RDONLY);
    if (GC_random_fd == -1) {
        perror("open(/dev/urandom)");
        abort();
    }
}

static void GC_random(char *data, size_t count) {
    size_t len = 0;
    while (len < count) {
        ssize_t result = read(GC_random_fd, data + len, count - len);
        if (result == -1) {
            perror("read(/dev/urandom)");
            abort();
        }
        len += result;
    }
}

#endif
