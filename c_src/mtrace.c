#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdatomic.h>
#include <string.h>
#include <threads.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

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

static atomic_size_t m_cnt;
static atomic_size_t c_cnt;
static atomic_size_t r_cnt;
static atomic_size_t f_cnt;

#define DEEP 20
#define SIZE 1024

typedef struct {
    _Atomic(void *) ptr;
    void *stack[DEEP];
    time_t ts;
} elem;

static elem tab[SIZE];

int hash_index(void *addr) { return (size_t)addr / 16 % SIZE; }

#define ZERO ((void *) 0)
#define LOCK ((void *) 1)

/* typedef struct {
    void **addrs;
    int count;
} trace_arg;

static _Unwind_Reason_Code trace_fn(struct _Unwind_Context *ctx, void *arg) {
    trace_arg *targ = (trace_arg *)arg;
    if (targ->count >= DEEP)
        return _URC_END_OF_STACK;

    void *ip = (void *)_Unwind_GetIP(ctx);
    if (ip) {
        targ->addrs[targ->count++] = ip;
    }
    return _URC_NO_REASON;
} */

static void *hash(void *ptr) {
    // static thread_local int flag;
    // if (flag == 0) {
        // flag = 1;
        elem *p = &tab[hash_index(ptr)];
        void *zero = ZERO;
        // lock record only if it is empty
        if (atomic_compare_exchange_weak(&p->ptr, &zero, LOCK)) {
            // populate data
            // p->stack[0] = __builtin_extract_return_addr(__builtin_frame_address(0));
            // trace_arg targ = { &p->stack[1], 0 };
            // _Unwind_Backtrace(trace_fn, &targ);
            int n = unw_backtrace(p->stack, DEEP);
            for (int i=n; i<DEEP; i++) p->stack[i] = 0;
            /* int n = backtrace(p->stack, DEEP);
            if (n < DEEP) memset(&p->stack[n], 0, DEEP - n); */
            time(&p->ts);
            // release record
            atomic_store(&p->ptr, ptr);
        }
        // flag = 0;
    // }
    return ptr;
}

static void dehash(void *ptr) {
    elem *p = &tab[hash_index(ptr)];
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

static void *rehash(void *old, void *ptr) {
    if (ptr != old) {
        dehash(old);
        hash(ptr);
    }
    return ptr;
}

void *malloc(size_t size) {
    atomic_fetch_add(&m_cnt, 1);
    return hash(__libc_malloc(size));
}

void *calloc(size_t count, size_t size) {
    atomic_fetch_add(&c_cnt, 1);
    return hash(__libc_calloc(count, size));
}

void *realloc(void *ptr, size_t size) {
    atomic_fetch_add(&r_cnt, 1);
    return rehash(ptr, __libc_realloc(ptr, size));
}

void free(void *ptr) {
    atomic_fetch_add(&f_cnt, 1);
    dehash(ptr);
    __libc_free(ptr);
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
        vals[k] = enif_make_uint64(env, p->ts);
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

    ERL_NIF_TERM ret = enif_make_uint64(env, elapsed(&start));
    return enif_make_tuple2(env, ret, enif_make_list_from_array(env, arr, i));
}

static ERL_NIF_TERM stats_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    return enif_make_tuple4(env,
      enif_make_uint64(env, m_cnt),
      enif_make_uint64(env, c_cnt),
      enif_make_uint64(env, r_cnt),
      enif_make_uint64(env, f_cnt)
    );
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
    return enif_make_int(env, 4);
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
