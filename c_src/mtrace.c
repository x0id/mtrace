#define _GNU_SOURCE
#include <dlfcn.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <sys/param.h>
#include <malloc.h>
#include <string.h>

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
int __posix_memalign(void **memptr, size_t alignment, size_t size);

static atomic_size_t malloc_cnt;
static atomic_size_t calloc_cnt;
static atomic_size_t realloc_cnt;
static atomic_size_t free_cnt;
static atomic_size_t posix_memalign_cnt;
static atomic_size_t strdup_cnt;
static atomic_size_t strndup_cnt;

#define DEEP 20
#define SIZE 4096

typedef struct {
    _Atomic(void *) ptr;
    size_t size;
    void *stack[DEEP];
    time_t ts;
} elem;

static elem tab[SIZE];

int hash_index(size_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= (x >> 31);
    return x % SIZE;
}

#define ZERO ((void *) 0)
#define LOCK ((void *) 1)

static void *hash(void *ptr, size_t size) {
    elem *p = &tab[hash_index((size_t)ptr)];
    void *zero = ZERO;
    // lock record only if it is empty
    if (atomic_compare_exchange_weak(&p->ptr, &zero, LOCK)) {
        // populate data
        int n = unw_backtrace(p->stack, DEEP);
        for (int i=n; i<DEEP; i++) p->stack[i] = 0;
        p->size = size;
        time(&p->ts);
        // release record
        atomic_store(&p->ptr, ptr);
    }
    return ptr;
}

static void dehash(void *ptr) {
    elem *p = &tab[hash_index((size_t)ptr)];
    void *expected = ptr;
    do {
        if (atomic_compare_exchange_weak(&p->ptr, &expected, ZERO))
            break;
        if (expected == LOCK) {
            expected = ptr;
            continue;
        }
    } while (ptr == expected);
}

static void *rehash(void *old, void *ptr, size_t size) {
    dehash(old);
    hash(ptr, size);
    return ptr;
}

void *malloc(size_t size) {
    atomic_fetch_add(&malloc_cnt, 1);
    return hash(__libc_malloc(size), size);
}

void *calloc(size_t count, size_t size) {
    atomic_fetch_add(&calloc_cnt, 1);
    return hash(__libc_calloc(count, size), count * size);
}

void *realloc(void *ptr, size_t size) {
    atomic_fetch_add(&realloc_cnt, 1);
    return rehash(ptr, __libc_realloc(ptr, size), size);
}

void free(void *ptr) {
    atomic_fetch_add(&free_cnt, 1);
    dehash(ptr);
    __libc_free(ptr);
}

void* __libc_memalign(size_t, size_t);

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    void *mem;
    if (alignment % sizeof (void *) != 0
        || !powerof2 (alignment / sizeof (void *))
        || alignment == 0)
        return EINVAL;
    mem = __libc_memalign(alignment, size);
    if (mem != NULL) {
        *memptr = hash(mem, size);
        atomic_fetch_add(&posix_memalign_cnt, 1);
        return 0;
    }
    return ENOMEM;
}

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    atomic_fetch_add(&strdup_cnt, 1);
    void *new = __libc_malloc(len);
    if (new == NULL)
        return NULL;
    return (char *)hash(memcpy(new, s, len), len);
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    atomic_fetch_add(&strndup_cnt, 1);
    char *new = (char *)__libc_malloc(len + 1);
    if (new == NULL)
        return NULL;
    new[len] = '\0';
    return (char *)hash(memcpy(new, s, len), len + 1);
}

static long elapsed(struct timespec *start) {
    struct timespec end;
    timespec_get(&end, TIME_UTC);
    return (end.tv_sec - start->tv_sec) * 1000000000LL + (end.tv_nsec - start->tv_nsec);
}

static ERL_NIF_TERM batch_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct timespec start;
    timespec_get(&start, TIME_UTC);

    ERL_NIF_TERM *keys = __libc_calloc(SIZE, sizeof(ERL_NIF_TERM));
    ERL_NIF_TERM *vals = __libc_calloc(SIZE, sizeof(ERL_NIF_TERM));
    void *ptr;
    int k = 0;
    for (int i=0; i<SIZE; i++) {
        elem *p = &tab[i];
        // lock non-empty unlocked record
        for (;;) {
            ptr = atomic_load(&p->ptr);
          inner:
            if (ptr == LOCK) continue;
            if (ptr == ZERO) break;
            if (atomic_compare_exchange_weak(&p->ptr, &ptr, LOCK)) break;
            goto inner;
        }
        // skip empty record
        if (ptr == ZERO) continue;
        // collect data
        keys[k] = enif_make_uint64(env, (size_t)ptr);
        vals[k] = enif_make_tuple2(env, enif_make_uint64(env, p->size), enif_make_uint64(env, p->ts));
        k++;
        // release record
        atomic_store(&p->ptr, ptr);
    }

    ERL_NIF_TERM map;
    enif_make_map_from_arrays(env, keys, vals, k, &map);
    __libc_free(vals);
    __libc_free(keys);

    return enif_make_tuple2(env, enif_make_uint64(env, elapsed(&start)), map);
}

static ERL_NIF_TERM erase_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    size_t addr;
    if (!enif_get_uint64(env, argv[0], &addr))
        return enif_make_badarg(env);
    dehash((void *)addr);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM reset_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct timespec start;
    timespec_get(&start, TIME_UTC);

    void *ptr;
    for (int i=0; i<SIZE; i++) {
        elem *p = &tab[i];
        // lock non-empty unlocked record
        for (;;) {
            ptr = atomic_load(&p->ptr);
          inner:
            if (ptr == LOCK) continue;
            if (ptr == ZERO) break;
            if (atomic_compare_exchange_weak(&p->ptr, &ptr, LOCK)) break;
            goto inner;
        }
        // skip empty record
        if (ptr == ZERO) continue;
        // reset/release non-empty record
        atomic_store(&p->ptr, ZERO);
    }
    return enif_make_uint64(env, elapsed(&start));
}

static ERL_NIF_TERM stack_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct timespec start;
    timespec_get(&start, TIME_UTC);

    size_t addr;
    if (!enif_get_uint64(env, argv[0], &addr))
        return enif_make_badarg(env);
    elem *ep = &tab[hash_index(addr)];
    Dl_info info;
    ERL_NIF_TERM arr[DEEP];
    int i;
    for (i=0; i<DEEP; i++) {
        void *ptr = ep->stack[i];
        if (0 == ptr)
            break;
        if (0 == dladdr(ptr, &info))
            arr[i] = enif_make_atom(env, "nil");
        else
            arr[i] = enif_make_tuple3(env,
                enif_make_uint64(env, (size_t)ptr),
                enif_make_string(env, info.dli_fname, ERL_NIF_LATIN1),
                info.dli_sname == NULL
                    ? enif_make_atom(env, "nil")
                    : enif_make_string(env, info.dli_sname, ERL_NIF_LATIN1)
            );
    }

    ERL_NIF_TERM ret = enif_make_uint64(env, elapsed(&start));
    return enif_make_tuple2(env, ret, enif_make_list_from_array(env, arr, i));
}

static ERL_NIF_TERM stats_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ERL_NIF_TERM keys[] = {
        enif_make_atom(env, "malloc_cnt"),
        enif_make_atom(env, "posix_memalign_cnt"),
        enif_make_atom(env, "calloc_cnt"),
        enif_make_atom(env, "realloc_cnt"),
        enif_make_atom(env, "free_cnt"),
        enif_make_atom(env, "strdup_cnt"),
        enif_make_atom(env, "strndup_cnt"),
        enif_make_atom(env, "allocated_bytes"),
        enif_make_atom(env, "used_bytes"),
        enif_make_atom(env, "free_bytes"),
        enif_make_atom(env, "mmap_chunks"),
        enif_make_atom(env, "mmap_bytes")
    };

    struct mallinfo x = mallinfo();

    ERL_NIF_TERM vals[] = {
      enif_make_uint64(env, malloc_cnt),
      enif_make_uint64(env, posix_memalign_cnt),
      enif_make_uint64(env, calloc_cnt),
      enif_make_uint64(env, realloc_cnt),
      enif_make_uint64(env, free_cnt),
      enif_make_uint64(env, strdup_cnt),
      enif_make_uint64(env, strndup_cnt),
      enif_make_int(env, x.arena),
      enif_make_int(env, x.uordblks),
      enif_make_int(env, x.fordblks),
      enif_make_int(env, x.hblks),
      enif_make_int(env, x.hblkhd)
    };

    int n = sizeof(keys) / sizeof(ERL_NIF_TERM);
    ERL_NIF_TERM map;
    enif_make_map_from_arrays(env, keys, vals, n, &map);
    return map;
}

static ERL_NIF_TERM malloc_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct timespec start;
    timespec_get(&start, TIME_UTC);

    size_t size;
    if (!enif_get_uint64(env, argv[0], &size))
        return enif_make_badarg(env);

    void *ptr = malloc(size);
    ERL_NIF_TERM addr = enif_make_uint64(env, (size_t)ptr);
    return enif_make_tuple2(env, enif_make_uint64(env, elapsed(&start)), addr);
}

static ERL_NIF_TERM free_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    struct timespec start;
    timespec_get(&start, TIME_UTC);

    size_t addr;
    if (!enif_get_uint64(env, argv[0], &addr))
        return enif_make_badarg(env);

    free((void *)addr);
    return enif_make_uint64(env, elapsed(&start));
}

static ERL_NIF_TERM vsn_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    return enif_make_int(env, 5);
}

static ErlNifFunc nif_funcs[] = {
    {"batch", 0, batch_nif, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"erase", 1, erase_nif, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"reset", 0, reset_nif, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"stack", 1, stack_nif, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"stats", 0, stats_nif},
    {"malloc", 1, malloc_nif},
    {"free", 1, free_nif},
    {"vsn", 0, vsn_nif}
};

ERL_NIF_INIT(Elixir.Mtrace, nif_funcs, NULL, NULL, NULL, NULL)
