#include <erl_nif.h>

#include <stdatomic.h>
#include <assert.h>
#include <dlfcn.h>

// LD_PRELOAD=lib/mtrace-0.1.0/priv/mtrace.so bin/sandbox start_iex

// static atomic_size_t allocated;
static atomic_size_t m_cnt;
static atomic_size_t c_cnt;
static atomic_size_t r_cnt;
static atomic_size_t f_cnt;

// const size_t magic = 0x635727321;

typedef struct {
    size_t size;
    size_t mnum;
} hdr_t;

void *malloc(size_t size) {
    void *(*real_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    assert(real_malloc != NULL);
    atomic_fetch_add(&m_cnt, 1);
    return real_malloc(size);
/*
    void *ptr = real_malloc(size + sizeof(hdr_t));
    if (ptr == NULL) return NULL;
    ((hdr_t *)ptr)->size = size;
    ((hdr_t *)ptr)->mnum = magic;
    atomic_fetch_add(&allocated, size);
    return (char *)ptr + sizeof(hdr_t);
*/
}

void *calloc(size_t count, size_t size) {
    void *(*real_calloc)(size_t, size_t) = dlsym(RTLD_NEXT, "calloc");
    assert(real_calloc != NULL);
    atomic_fetch_add(&c_cnt, 1);
    return real_calloc(count, size);
/*
    // return malloc(count * size);
    size *= count;
    void *(*real_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    assert(real_malloc != NULL);
    void *ptr = real_malloc(size + sizeof(hdr_t));
    if (ptr == NULL) return NULL;
    ((hdr_t *)ptr)->size = size;
    ((hdr_t *)ptr)->mnum = magic;
    atomic_fetch_add(&allocated, size);
    return (char *)ptr + sizeof(hdr_t);
*/
}

void *realloc(void *ptr, size_t size) {
    void *(*real_realloc)(void *, size_t) = dlsym(RTLD_NEXT, "realloc");
    assert(real_realloc != NULL);
    atomic_fetch_add(&r_cnt, 1);
    return real_realloc(ptr, size);
/*
    if (ptr == NULL) return malloc(size);
    void *p = (char *)ptr - sizeof(hdr_t);
    void *(*real_realloc)(void *, size_t) = dlsym(RTLD_NEXT, "realloc");
    assert(real_realloc != NULL);
    if (((hdr_t *)p)->mnum != magic)
        return real_realloc(ptr, size);
    size_t size_ = ((hdr_t *)p)->size;
    ((hdr_t *)p)->mnum = 0;
    p = real_realloc(p, size + sizeof(hdr_t));
    if (p == NULL) return NULL;
    ((hdr_t *)p)->size = size;
    ((hdr_t *)p)->mnum = magic;
    atomic_fetch_add(&allocated, size - size_);
    return (char *)p + sizeof(hdr_t);
*/
}

/*
void *valloc(size_t size) {
    assert(0);
    return NULL;
}

void *reallocf(void *ptr, size_t size) {
    assert(0);
    return NULL;
}

void *aligned_alloc(size_t alignment, size_t size) {
    assert(0);
    return NULL;
}

char *strdup(const char *str) {
    size_t n = strlen(str);
    char *dup = malloc(n + 1);
    if (dup == NULL) return NULL;
    return strcpy(dup, str);
}

char *strndup(const char *str, size_t n) {
    size_t l = strlen(str);
    if (l < n) n = l;
    char *dup = malloc(n + 1);
    if (dup == NULL) return NULL;
    strncpy(dup, str, n);
    dup[n] = 0;
    return dup;
}
*/

void free(void *ptr) {
    void *(*real_free)(void *) = dlsym(RTLD_NEXT, "free");
    assert(real_free != NULL);
    atomic_fetch_add(&f_cnt, 1);
    real_free(ptr);
/*
    if (ptr == NULL) return;
    void *p = (char *)ptr - sizeof(hdr_t);
    void (*real_free)(void *) = dlsym(RTLD_NEXT, "free");
    if (((hdr_t *)p)->mnum != magic) {
        real_free(ptr);
        return;
    }
    size_t size = ((hdr_t *)p)->size;
    atomic_fetch_sub(&allocated, size);
    ((hdr_t *)p)->mnum = 0;
    real_free(p);
*/
}

/* static ERL_NIF_TERM allocated_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    return enif_make_int(env, allocated);
} */

static ERL_NIF_TERM stats_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    return enif_make_tuple4(env,
      enif_make_int(env, m_cnt),
      enif_make_int(env, c_cnt),
      enif_make_int(env, r_cnt),
      enif_make_int(env, f_cnt)
    );
}

static ErlNifFunc nif_funcs[] = {
    // {"allocated", 0, allocated_nif},
    {"stats", 0, stats_nif}
};

ERL_NIF_INIT(Elixir.Mtrace, nif_funcs, NULL, NULL, NULL, NULL)
