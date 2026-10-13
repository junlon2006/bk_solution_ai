# BK7259 HTTPS transport tests

Run from the firmware repository root:

```sh
bash bk_solution_ai/projects/mybot/tests/https/run.sh
ASAN_OPTIONS=detect_leaks=0 HTTPS_TEST_SANITIZERS=1 bash bk_solution_ai/projects/mybot/tests/https/run.sh
```

The tests compile the native adapter against the pinned MbedTLS public headers.
Socket and RTOS stubs cover independent connection errors despite unrelated writes
to global `errno`, partial I/O, EOF, hard errors, TCP peer confirmation, select
bounds, deadline expiry, and millisecond-clock wraparound. The five cases include
the adapter's send/receive retry loops and TCP setup cleanup.

TLS handshake and certificate validation remain outside this narrow harness;
the firmware build uses the real vendor libraries. The optional GCC ASAN/UBSAN run
disables instrumentation of globals so linker garbage collection can discard the
unused full TLS setup/ops table. Heap and stack instrumentation remain enabled.
The command above disables LeakSanitizer because it cannot run in the current
ptrace environment; leak detection has not been validated by this harness.
