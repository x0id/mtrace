
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

void *malloc(size_t n) {
  fprintf(stderr, "malloc %zu bytes\n", n);
  void *(*real_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
  return real_malloc(n);
}
