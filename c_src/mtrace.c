
#include <dlfcn.h>
#include <stddef.h>
#include <stdatomic.h>

atomic_size_t allocated;

typedef struct {
    size_t size;
} hdr_t;

void *malloc(size_t size) {
    void *(*real_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    void *ptr = real_malloc(size + sizeof(hdr_t));
    if (ptr == NULL) return NULL;
    ((hdr_t *)ptr)->size = size;
    atomic_fetch_add(&allocated, size);
    return (char *)ptr + sizeof(hdr_t);
}

void *calloc(size_t count, size_t size) {
    return malloc(count * size);
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);
    void *p = (char *)ptr - sizeof(hdr_t);
    size_t size_ = ((hdr_t *)p)->size;
    void *(*real_realloc)(void *, size_t) = dlsym(RTLD_NEXT, "realloc");
    p = real_realloc(p, size + sizeof(hdr_t));
    ((hdr_t *)p)->size = size;
    atomic_fetch_add(&allocated, size - size_);
    return (char *)p + sizeof(hdr_t);
}

void free(void *ptr) {
    if (ptr == NULL) return;
    void *p = (char *)ptr - sizeof(hdr_t);
    size_t size = ((hdr_t *)p)->size;
    atomic_fetch_sub(&allocated, size);
    void (*real_free)(void *) = dlsym(RTLD_NEXT, "free");
    real_free(p);
}
