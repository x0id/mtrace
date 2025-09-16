#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <stdatomic.h>
#include <string.h>
#include <threads.h>

#include <erl_nif.h>

#define COMPILE_TIME_ASSERT(condition) \
    enum { COMPILE_TIME_ASSERT_##__LINE__ = 1 / (!!(condition)) }

void checks() {
    COMPILE_TIME_ASSERT(sizeof(atomic_size_t) == 8);
}

void *__libc_malloc(size_t size);
void *__libc_calloc(size_t nmemb, size_t size);
void *__libc_realloc(void *ptr, size_t size);
void __libc_free(void* ptr);

// static atomic_size_t allocated;
static atomic_size_t m_cnt;
static atomic_size_t c_cnt;
static atomic_size_t r_cnt;
static atomic_size_t f_cnt;

/* const size_t magic = 0x635727321;

typedef struct {
    size_t size;
    size_t mnum;
} hdr_t; */

#define DEEP 12
#define SIZE 4096

typedef struct {
    _Atomic void *ptr;
    void *stack[DEEP];
    time_t ts;
} elem;

static elem tab[SIZE];

int hash_index(void *addr) { return (size_t)addr / 16 % SIZE; }

static void *hash(void *ptr) {
    static thread_local int flag;
    if (flag == 0) {
        flag = 1;
        elem *p = &tab[hash_index(ptr)];
        void *expected = 0;
        if (atomic_compare_exchange_weak(&p->ptr, &expected, ptr)) {
            int n = backtrace(p->stack, DEEP);
            if (n < DEEP) memset(&p->stack[n], 0, DEEP - n);
            time(&p->ts);
        }
        flag = 0;
    }
    return ptr;
}

static void dehash(void *ptr) {
    elem *p = &tab[hash_index(ptr)];
    void *expected = ptr;
    do {
        if (atomic_compare_exchange_weak(&p->ptr, &expected, (void *)0))
            break;
        if (expected == (void *)1) {
            expected = ptr;
            continue;
        }
    } while (ptr == expected);
}

static void *rehash(void *old, void *ptr) {
    if (ptr != old) {
        dehash(old);
        hash(ptr);
    }
    return ptr;
}

void *malloc(size_t size) {
    // void *(*real_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    // assert(real_malloc != NULL);
    atomic_fetch_add(&m_cnt, 1);
    return hash(__libc_malloc(size));
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
    // void *(*real_calloc)(size_t, size_t) = dlsym(RTLD_NEXT, "calloc");
    // assert(real_calloc != NULL);
    atomic_fetch_add(&c_cnt, 1);
    return hash(__libc_calloc(count, size));
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
    // void *(*real_realloc)(void *, size_t) = dlsym(RTLD_NEXT, "realloc");
    // assert(real_realloc != NULL);
    atomic_fetch_add(&r_cnt, 1);
    return rehash(ptr, __libc_realloc(ptr, size));
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
    // void (*real_free)(void *) = dlsym(RTLD_NEXT, "free");
    // assert(real_free != NULL);
    atomic_fetch_add(&f_cnt, 1);
    dehash(ptr);
    __libc_free(ptr);
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
    return enif_make_uint64(env, allocated);
} */

static ERL_NIF_TERM batch_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct timespec start, end;
    timespec_get(&start, TIME_UTC);

    ERL_NIF_TERM acc = enif_make_new_map(env);
    for (int i=0; i<SIZE; i++) {
        elem *p = &tab[i];
        // "lock" entry
        void *ptr;
        do ptr = atomic_load(&p->ptr);
        while (!atomic_compare_exchange_weak(&p->ptr, &ptr, (void *)1));
        // collect data
        ERL_NIF_TERM key = enif_make_uint64(env, (size_t)ptr);
        ERL_NIF_TERM val = enif_make_uint64(env, p->ts);
        enif_make_map_put(env, acc, key, val, &acc);
        // "release" entry
        atomic_store(&p->ptr, ptr);
    }

    timespec_get(&end, TIME_UTC);
    ERL_NIF_TERM elapsed = enif_make_uint64(env,
        (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec)
    );

    return enif_make_tuple2(env, elapsed, acc);
}

static ERL_NIF_TERM erase_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    size_t addr;
    if (!enif_get_uint64(env, argv[0], &addr))
        return enif_make_badarg(env);
    dehash((void *)addr);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM stack_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    size_t addr;
    if (!enif_get_uint64(env, argv[0], &addr))
        return enif_make_badarg(env);
    elem *ep = &tab[hash_index((void *)addr)];
    Dl_info info;
    ERL_NIF_TERM arr[DEEP];
    int i;
    for (i=0; i<DEEP; i++) {
        void *addr = ep->stack[i];
        if (0 == addr)
            break;
        if (0 == dladdr(addr, &info))
            arr[i] = enif_make_atom(env, "nil");
        else
            arr[i] = enif_make_tuple2(env,
                enif_make_string(env, info.dli_fname, ERL_NIF_LATIN1),
                info.dli_sname == NULL
                    ? enif_make_atom(env, "nil")
                    : enif_make_string(env, info.dli_sname, ERL_NIF_LATIN1)
            );
    }
    return enif_make_list_from_array(env, arr, i);
}

static ERL_NIF_TERM stats_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    return enif_make_tuple4(env,
      enif_make_uint64(env, m_cnt),
      enif_make_uint64(env, c_cnt),
      enif_make_uint64(env, r_cnt),
      enif_make_uint64(env, f_cnt)
    );
}

static ErlNifFunc nif_funcs[] = {
    // {"allocated", 0, allocated_nif},
    {"batch", 0, batch_nif},
    {"erase", 1, erase_nif},
    {"stack", 1, stack_nif},
    {"stats", 0, stats_nif}
};

ERL_NIF_INIT(Elixir.Mtrace, nif_funcs, NULL, NULL, NULL, NULL)
