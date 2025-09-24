# Mtrace
This library is an ad-hoc tool that helped me identify a memory leak in one of the NIFs within a large OTP application.

## Disclaimer
1. This is rather a POC than a universal solution at the moment. It is expected to work in Debian Linux, see requirements.
2. The build is based on Elixir Mix. If one needs this tool for a pure Erlang application, it is encouraged to create a separate project for this purpose. This POC is Elixir-based because the author wanted to play with building NIFs in a pure Elixir project.
3. The author intentionally avoided the well-known elixir_make project because it depends on GNU make/compiler tools. One of the goals of this work was to prototype a portable build system for Elixir with NIFs.
4. The build portability requirement resulted in avoiding a common, well-known GNU make-targeted template and creating a makevars-based solution, ensuring the NIF can be built using both BSD make and GNU make.
5. Still, the implementation of intercepting malloc/free calls is very platform-specific and tested only for a specific OS release.

## Requirements
It was tested in Docker containers based on elixir:1.17.3-otp-25-slim, and elixir:1.17.2-slim-otp-27, both running Debian GNU/Linux 11 (bullseye) with gcc version 10.2.1 20210110 (Debian 10.2.1-6). The docker.sh and Dockerfile were added to ensure test runs in a specific environment with required dependencies. It may run on other Linux distributions, and maybe on Free/Open/Net BSDs with GCC/GLIBC.

## Building
Ensure your platform has `libunwind-dev` installed.

Add dependency to the `mix.exs`:
```
[
  ...,
  mtrace: [github: "x0id/mtrace"]
]
```

## Usage
You need to preload `mtrace.so` library when starting the application, for example, when creating Elixir release, add the following line into `rel/env.sh.eex`:
```
export LD_PRELOAD=`ls $RELEASE_ROOT/lib/mtrace-*/priv/mtrace.so`
```

Run `Mtrace.olds()` to see age-sorted list of allocated chunks in format `[{age, size, addr, us, stack}, ...]`, where:
- age: life span of the memory chunk in seconds
- size: memory chunk size in bytes
- addr: memory chunk address
- ns: nanoseconds spent in retrieving backtrace symbols
- stack: backtrace of the memory accocation call
```
$ ./docker.sh
root@mtrace:/app# bin/run
make[1]: Entering directory '/app/c_src'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '/app/c_src'
Erlang/OTP 25 [erts-13.2.2.16] [source] [64-bit] [smp:16:16] [ds:16:16:10] [async-threads:1] [jit:ns]

make[1]: Entering directory '/app/c_src'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '/app/c_src'
Interactive Elixir (1.17.3) - press Ctrl+C to exit (type h() ENTER for help)
iex(1)> Mtrace.olds(3)
[
  {8, 32, 139656566418608, 198197,
   [
     {"/app/_build/dev/lib/mtrace/priv/mtrace.so", "malloc"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "_Z23beamasm_metadata_updateNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPKvmRKSt6vectorIN13BeamAssembler8AsmRangeESaIS9_EE"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "_ZN19BeamModuleAssembler17register_metadataEPK16beam_code_header"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "beam_load_finalize_code"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "erts_finish_loading"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "finish_loading_1"},
     nil
   ]},
  {8, 131072, 139658290642944, 536113,
   [
     {"/app/_build/dev/lib/mtrace/priv/mtrace.so", "posix_memalign"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "erts_sys_aligned_alloc"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", nil},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", nil},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "erts_alcu_start"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "erts_afalc_start"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", nil},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "erts_alloc_init"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "erl_start"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "main"},
     {"/lib/x86_64-linux-gnu/libc.so.6", "__libc_start_main"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "_start"}
   ]},
  {8, 72, 93893602472272, 198170,
   [
     {"/app/_build/dev/lib/mtrace/priv/mtrace.so", "malloc"},
     {"/usr/lib/x86_64-linux-gnu/libstdc++.so.6", "_Znwm"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "_ZNSt3mapIN19BeamGlobalAssembler12GlobalLabelsEKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4lessIS1_ESaISt4pairIKS1_S8_EEEC2ESt16initializer_listISD_ERKSA_RKSE_"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", nil},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "__libc_csu_init"},
     {"/lib/x86_64-linux-gnu/libc.so.6", "__libc_start_main"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "_start"}
   ]}
]
```

Memory allocation statistics, where `*_cnt` counters show count of corresponding memory allocation/releasing function interceptions, other values are from the result of glibc `mallinfo()` call:
```
iex(2)> Mtrace.stats  
%{
  allocated_bytes: 21475328,
  calloc_cnt: 64,
  free_bytes: 12085264,
  free_cnt: 609674,
  malloc_cnt: 603398,
  mmap_bytes: 14622720,
  mmap_chunks: 52,
  posix_memalign_cnt: 152,
  realloc_cnt: 300,
  strdup_cnt: 60,
  strndup_cnt: 0,
  used_bytes: 9390064
}
iex(3)> 

```

To demangle C++ symbols add `true` as the second arg to `olds`:
```
iex(3)> Mtrace.olds(3, true)
[
  {761, 32, 139656566418608, 299345,
   [
     {"/app/_build/dev/lib/mtrace/priv/mtrace.so", "malloc"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "beamasm_metadata_update(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, void const*, unsigned long, std::vector<BeamAssembler::AsmRange, std::allocator<BeamAssembler::AsmRange> > const&)"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "BeamModuleAssembler::register_metadata(beam_code_header const*)"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "beam_load_finalize_code"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp",
      "erts_finish_loading"},
     {"/usr/local/lib/erlang/erts-13.2.2.16/bin/beam.smp", "finish_loading_1"},
     nil
   ]},
...
```

To reset the accumulated interceptions call `Mtrace.reset()`.
```
iex(4)> Mtrace.reset        
61570
iex(5)> Mtrace.olds(3, true)
[]
```

The reset function allows you to clear the internal table of the data accumulated during system startup.
The internal table has a limited size (4096), and a hash calculated from the memory chunk address is used as an index to the table.
Once the table cell is occupied with data, other intercepted calls with the same hash function are ignored. That's why repeated reset invocation can help in catching new allocations.
