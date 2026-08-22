<!--
related_code:
  - zr_vm_language_server/stdio/stdio_frame_reader.h
  - zr_vm_language_server/stdio/stdio_frame_reader.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
  - language_server_stdio_position_encoding_smoke
  - language_server_stdio_type_hierarchy_smoke
  - language_server_stdio_protocol_inventory
-->

# LSP Bounded Stdio Frame Reader

## Scope

This record completes Task 3 of
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It separates byte-level stdio frame parsing from JSON-RPC dispatch, validates
headers before allocating payload memory, and keeps JSON payload parse errors
recoverable at the established dispatch boundary.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 20:59 +08:00 | completed | Added a bounded frame reader with centralized header/count/payload limits, injectable lower test limits, strict CRLF framing, unique unsigned `Content-Length`, UTF-8 `Content-Type` handling, and pre-allocation overflow checks. Transport now emits classified stderr diagnostics for malformed/truncated/oversize/I/O frames and only treats clean EOF as normal input closure. The conformance driver covers missing, duplicate, negative, suffixed, overflowing, truncated, malformed-newline, non-UTF-8, oversize, and excessive-header frames. |
| 2026-08-23 03:54 +08:00 | completed | Revalidated frame classification with deterministic server teardown enabled. Oversize and all malformed-frame cases still terminate non-zero with the exact stderr category and do not leak protocol data to stdout. |

## Contract

- `ZR_LSP_MAX_HEADER_BYTES`, `ZR_LSP_MAX_HEADER_COUNT`, and
  `ZR_LSP_MAX_MESSAGE_BYTES` define the production limits. The default payload
  limit is 16 MiB; `SZrStdioFrameReaderLimits` accepts lower limits for tests.
- Header parsing accepts CRLF lines only. Unknown headers are ignored but count
  against the byte and header limits. An explicitly declared non-UTF-8 charset
  is malformed.
- `Content-Length` is mandatory, unique, unsigned decimal, bounded with
  `strtoull`, `errno`, an end pointer, and `SIZE_MAX - 1` before allocating
  `contentLength + 1` bytes.
- The reader distinguishes `EOF`, `MALFORMED_HEADER`, `PAYLOAD_TRUNCATED`,
  `TOO_LARGE`, and `IO_ERROR`. Only clean `EOF` closes the input without an
  error diagnostic. JSON payload syntax errors remain dispatchable and produce
  JSON-RPC `-32700` through the existing envelope path.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and the
matching GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake -S /home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0 \
  -B /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF -DBUILD_WASM=OFF
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_stdio --parallel 8
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_position_encoding_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_type_hierarchy_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_protocol_inventory
```

Results:

- GCC configuration and native stdio link exited `0`.
- The protocol conformance driver passed all Task 3 frame cases, including
  classified process failure and stderr category checks. Its process exits `1`
  only for the deliberate Task 4 RED: duplicate active JSON-RPC request ids
  still receive two success responses.
- `language_server_stdio_smoke`, position encoding, type hierarchy, and
  protocol inventory smokes each passed `1/1`.

## Remaining Work

Task 4 owns active request identity, duplicate-id rejection, and cancellation.
Task 5 owns deterministic teardown. This milestone does not suppress or
whitelist either remaining responsibility.
