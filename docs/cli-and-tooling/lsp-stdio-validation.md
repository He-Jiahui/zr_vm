---
related_code:
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
tests:
  - tests/language_server/stdio_smoke.js
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

## Position-Encoding Handshake

The position-encoding smoke uses a request/response client. It waits for the
`initialize` response before sending `initialized` and `didOpen`, then requests
hover and waits for `shutdown` before `exit`. A one-shot batch can let the
reader thread observe `didOpen` before initialize activates; the resulting
`ContentModified` response correctly enforces the input-generation fence but
does not represent a legal LSP client handshake.

## Matrix

The L6 final matrix runs every CTest whose name begins
`language_server_stdio` or `cli_`: five stdio protocol smokes and 28 CLI
smokes/integration suites. Run GCC and Clang CTest from WSL so the configured
`/usr/bin/node` is valid; run MSVC CTest from the native Visual Studio shell.

## Scope

This test budgets the native stdio server process only. The context-local
semantic-cache LRU separately reports exact `SZrAnalysisCache` storage and
must not be described as RSS, allocator peak, or total process memory.
