#!/bin/sh

case `uname -s` in
  Darwin)
    echo "CC ?= cc"
    echo "LDFLAGS = -undefined dynamic_lookup"
    ;;
  FreeBSD)
    echo "CC ?= cc"
    ;;
  Linux)
    echo "CC ?= gcc"
    # bullseye needs the following for dlsym(3)
    # echo "CFLAGS += -D_GNU_SOURCE"
    # echo "LDFLAGS += -Wl,--no-as-needed -ldl"
    echo "LIBUNWIND = /usr/lib/`uname -m`-linux-gnu/libunwind.so"
    ;;
esac

erl -noshell -eval '
io:format(
  "ERTS_INCLUDE_DIR = ~ts/erts-~ts/include~n"
  "ERL_INTERFACE_INCLUDE_DIR = ~ts~n"
  "ERL_INTERFACE_LIB_DIR = ~ts~n",
  [code:root_dir(), erlang:system_info(version),
   code:lib_dir(erl_interface, include),
   code:lib_dir(erl_interface, lib)]).
' -s init stop
