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
