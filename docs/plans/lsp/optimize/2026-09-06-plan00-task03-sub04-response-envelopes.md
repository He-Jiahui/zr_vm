---
related_code:
  - tests/language_server/stdio_protocol_client.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_envelope_mutations.js
  - tests/CMakeLists.txt
implementation_files:
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_envelope_mutations.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_protocol_envelope_mutations
doc_type: milestone-record
---

# Plan 00 Task 3 Sub04: Response Envelopes

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-06 11:07 +08:00 | 2026-09-06 11:20 +08:00 | completed (test driver subitem; parent phase pending) | Complete response envelope checks; real decoded-response mutation runner; three-toolchain replay. | GCC/Clang/MSVC conformance 30/30 each; six unmodified cases and 11/11 rejected mutations each; regenerated CTest 1/1; syntax and scoped diff checks pass. |

## Problem And Change

The driver checked complete error envelopes on most negative requests, but
duplicate-ID errors bypassed that helper. Several success paths checked only
the result or ID, so malformed JSON-RPC versions and mixed result/error objects
were accepted. The existing error helper also allowed a missing or non-string
message.

The common response assertion now checks object shape, `jsonrpc: "2.0"`, exact
typed ID and exactly the three expected envelope keys. Error assertions add the
expected code and string message. Every success response, including shutdown,
trace, work-done and partial results, passes that assertion before its semantic
checks. Duplicate requests require one complete `-32600` error and one complete
success; numeric and string IDs stay distinct. The shared frame/client harness
and deadlines are unchanged.

The new mutation runner imports the same cases and mutates one decoded real
server response per run. It restores the shared dispatch method in `finally`
and requires an envelope failure, so a timeout cannot masquerade as rejection.
This is test-harness validation; it introduces no runtime behavior or canonical
semantic identity changes. Module documentation is
[LSP stdio validation](../../../cli-and-tooling/lsp-stdio-validation.md).

## RED And GREEN

Before strengthening the checks, six unchanged fixtures passed and all 11
mutations escaped their production assertions. After the change, all six
controls still pass and all 11 mutations are rejected on GCC, Clang and MSVC.

| Mutation | Original result | Corrected result |
| --- | --- | --- |
| Duplicate error without jsonrpc | accepted | rejected |
| Duplicate error with result | accepted | rejected |
| Duplicate success with jsonrpc 1.0 | accepted | rejected |
| Duplicate success with error null | accepted | rejected |
| Numeric-ID success without jsonrpc | accepted | rejected |
| String-ID success without result | accepted | rejected |
| Shutdown success with jsonrpc 1.0 | accepted | rejected |
| Error without message | accepted | rejected |
| Error with numeric message | accepted | rejected |
| Work-done response without jsonrpc | accepted | rejected |
| Partial-result response with error null | accepted | rejected |

The full conformance driver also passes 30/30 on each toolchain. The new runner
and driver pass `node --check`.

## Commands And Source Boundary

The source baseline is `57cd9b78` plus this test/documentation change. GCC uses
the current-checkout build from Task 1 Sub04, including the explicitly retained
uncommitted overlays. Clang and MSVC use the existing isolated protocol replay
builds; no claim is made that all three contain the same semantic overlay.
The changed JavaScript driver is the current checkout in every run.

```text
<Node 22> tests/language_server/stdio_protocol_conformance.js <server>
<Node 22> tests/language_server/stdio_protocol_envelope_mutations.js <server>

GCC server:
  /home/hejiahui/.codex-builds/lsp-plan00-envelope-20260906/bin/zr_vm_language_server_stdio
GCC LD_LIBRARY_PATH:
  /home/hejiahui/.codex-builds/lsp-plan00-envelope-20260906/lib
Clang server:
  /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang/bin/zr_vm_language_server_stdio
Clang LD_LIBRARY_PATH:
  /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang/lib
MSVC server:
  E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
```

Linux Node is
`/home/hejiahui/.codex-tools/node-22.13.1/node-v22.13.1-linux-x64/bin/node`.
The GCC server and shared libraries were copied from `.codex/build-lsp-opt-gcc`
to ext4. Every copied library compares byte-for-byte equal. Both server copies
have SHA-256
`b86b8c82fde1b4934c9bb56f573521c4349521187fda7b3c047117067bf9192a`.

The first RED run on `/mnt/e` observed nine escaped mutations and two fixtures
that failed before injection. The ext4 replay reproduced all 11 escaped
mutations without those setup failures. A later `/mnt/e` CTest conformance run
reported four initialization response timeouts: request-after-shutdown, missing
jsonrpc, trace and work-done setup. That run started before CMake generation
finished and therefore used the prior CTest inventory. It is retained as failed
environment evidence, not accepted as a test registration run or hidden by
larger deadlines. The ext4 direct GREEN run passes 30/30 and 11/11.

After generation completed, the new registered CTest ran against the original
GCC server on `/mnt/e` and passed 1/1 in 2.32 seconds:

```text
ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure \
  -R '^language_server_stdio_protocol_envelope_mutations$'
  100% tests passed, 0 tests failed out of 1
```

This verifies the registration and its command independently of the earlier
concurrent configure/conformance attempt. The failed attempt is not a stable
filesystem-performance diagnosis.

## Remaining Gates

This subitem closes response-envelope assertions and their mutation evidence.
Plan 00 still requires the frozen current-source baseline and linked WASM
assets. Plan 01 owns active-query cancellation latency, full fault injection
and stable sanitizer/teardown acceptance. The current aggregate semantic
failures remain assigned to their existing support layers and are not waived.
