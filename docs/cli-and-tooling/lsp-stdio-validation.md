---
related_code:
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/test_stdio_server_lifecycle.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
tests:
  - tests/language_server/stdio_smoke.js
  - tests/language_server/test_stdio_server_lifecycle.c
doc_type: module-guide
---

# LSP Stdio Validation

## Process Peak Budget

`tests/language_server/stdio_smoke.js` measures the language-server child
while it is alive, after the full protocol workload and after the `shutdown`
response but before the `exit` notification. Linux consumes `/proc/<pid>/status`
`VmHWM`; Windows consumes `Get-Process ... PeakWorkingSet64`. Both are OS
high-water metrics for that child process, not cache-storage estimates.

The default `ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES` limit is 512MiB. A supplied
value must be a positive safe integer. The smoke prints the peak and limit in
bytes and MiB, and fails when the peak exceeds the limit. Unsupported host
platforms fail instead of silently reporting an approximation.

## Lifecycle Races

The stdio reader thread may linearize a `workspace/diagnostic` request before
the immediately following cancel, change, or close notification. The churn
test accepts only a lifecycle error when the later input was observed first,
or a workspace report containing the exact document version at the request
linearization point. It rejects a mixed or newer snapshot. A separate request
cancellation case retains the strict 50ms `RequestCancelled` budget.

## Deterministic Teardown

`SZrStdioServer` owns the native LSP runtime. Construction creates the global
state, initializes its registry, creates the LSP context and request registry,
then initializes the request-input synchronization state. `Start` alone owns
reader creation. The input state retains the Win32 `HANDLE` or `pthread_t`; it
never closes or detaches that handle at creation time.

`Free` uses one ordered path for ordinary shutdown and each construction fault:

1. set the reader stop flag and wake waiting consumers;
2. join the reader after the protocol `exit` notification or EOF ends its
   blocking read;
3. delete each queued JSON message and release the request registry;
4. destroy the input condition variable and mutex;
5. release URI and semantic-token caches, then the LSP context and global
   state.

The reader stops naturally after a valid `exit` notification or input EOF.
The stop flag ensures no additional frame is read after a wake-up; the server
does not cancel a thread asynchronously while it may own C runtime input state.
This preserves a joinable reader without introducing an unsafe cancellation
point during teardown. The caller owns the supplied `FILE *`; the server does
not close it.

`test_stdio_server_lifecycle.c` runs 100 same-process
New/Start/Shutdown/Free cycles, an `exit` frame case, and injected failures
after global, context, input initialization, and reader start. This test exists
to catch teardown faults that a one-shot executable process would hide.

## Teardown Acceptance

The deterministic teardown gate was accepted on 2026-08-23. Clang and GCC
ASan+UBSan each pass the lifecycle, smoke, protocol inventory and protocol
conformance CTest set. GCC's uninstrumented rerun passes the same set with the
default 512 MiB peak-process budget. The sanitizer runs set
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824` only to account for ASan
shadow memory; they do not relax the production budget.

The 100-cycle lifecycle executable also passes Valgrind Memcheck with 54,339
allocations and frees, zero live bytes and zero errors, and passes Helgrind
with zero race reports. MSVC Debug passes the four-test stdio CTest set; MSVC
AddressSanitizer passes the lifecycle executable.

## Position-Encoding Handshake

The position-encoding smoke uses a request/response client. It waits for the
`initialize` response before sending `initialized` and `didOpen`, then requests
hover and waits for `shutdown` before `exit`. A one-shot batch can let the
reader thread observe `didOpen` before initialize activates; the resulting
`ContentModified` response correctly enforces the input-generation fence but
does not represent a legal LSP client handshake.

## Matrix

The L6 final matrix runs every CTest whose name begins
`language_server_stdio` or `cli_`: the deterministic lifecycle test, five
stdio protocol smokes and 28 CLI smokes/integration suites. Run GCC and Clang
CTest from WSL so the configured `/usr/bin/node` is valid; run MSVC CTest from
the native Visual Studio shell.

## Scope

This test budgets the native stdio server process only. The context-local
semantic-cache LRU separately reports exact `SZrAnalysisCache` storage and
must not be described as RSS, allocator peak, or total process memory.
