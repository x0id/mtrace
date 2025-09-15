#include <erl_nif.h>

static ERL_NIF_TERM malloc_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    void *p = malloc(100);
    return enif_make_atom(env, p ? "ok" : "null");
}

static ErlNifFunc nif_funcs[] = {
    {"malloc", 0, malloc_nif}
};

ERL_NIF_INIT(Elixir.Nif, nif_funcs, NULL, NULL, NULL, NULL)
