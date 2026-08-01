# 2026-08-01 AOT 07 Parameter Source Passing-Form Projection

## Scope

This A7.2M sub-milestone preserves all seven source parameter passing forms in the existing typed-local role carrier,
projects them into canonical ExecIR, validates the complete function table before stripping, and permits the current
aggregate typed-call path only for explicit VALUE parameters. It does not implement reference storage, readonly or
scoped address semantics, `out` initialization/writeback, or a complete aggregate callable ABI.

## Baseline

Frozen effective source is committed HEAD `b968f2d3038bfdd1dade3349a3a243131bdbde8a` plus the exact A7.2M
five-production/seven-test overlay. The initial RED failed to compile because the passing-form role flags, ExecIR
sidecar, and VALUE-only helper did not exist. A review-strengthened RED then demonstrated that name-only producer
matching incorrectly marked a later same-name shadow local as VALUE.

## Test Inventory

- Compiles VALUE, IN, REF, REF_READONLY, SCOPED_REF, SCOPED_REF_READONLY, and OUT from real source and verifies exact
  typed-local role flags before and after `.zro` serialization.
- Compiles a real instance method and verifies receiver-only slot 0 followed by VALUE and IN explicit parameters.
- Verifies current-parameter-prefix matching leaves a later same-name local role-free and prevents nested callables
  from inheriting an outer member receiver role.
- Projects all seven forms plus legacy UNKNOWN into canonical ExecIR, validates sidecar boolean/enum bounds, and keeps
  raw passing bits out of the projected receiver role.
- Accepts only exact-arity VALUE layouts in the aggregate typed-call consumer; UNKNOWN and all six non-VALUE forms
  preserve fallback behavior.
- Proves legal 3-to-2 stripping, then rejects partial known/unknown parameter sets, multiple passing bits, unknown role
  bits, receiver/passing combinations, and passing bits outside the parameter prefix before stripping.

## Tooling Evidence

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72m-b968f2d-final-r1`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72m-b968f2d-final-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug
- SHA-256 matched all twelve controlled implementation/test files between main and both frozen trees.
- Independent review identified the now-canonical raw `2u` fixture, requested the real receiver and malformed-owner
  matrices, and reported no Critical or Important findings after correction.

## Results

- WSL GCC and Clang each pass SemIR 13/0, generic typed-call 24/0, code stripping 37/0, MethodInfo 11/0, generic
  sharing 9/0, debug metadata 6/0, value-SemIR 8/0, typed-call contracts 4/0, and source contracts 24/0.
- The first Clang build invocation reached its external time limit after 582 objects; an incremental continuation built
  the remaining 86 steps successfully. This was a runner timeout, not a compiler or test failure.
- Windows MSVC passes SemIR 12/0, code stripping 37/0, MethodInfo 8/0, generic sharing 9/0, debug metadata 6/0,
  value-SemIR 8/0, typed-call contracts 4/0, and source contracts 24/0. Generic typed-call has 24 total with five
  expected Unix-only ignores and zero failures.
- Three existing multiline source-contract checks initially false-failed against CRLF files. Only the frozen Windows
  copies of `backend_aot_c_lowering_values.c`, `backend_aot_c_scalar_binary.c`, and `backend_aot_c_emitter.c` were
  normalized to LF before the final 24/0 run; main and assertions were unchanged.

## Acceptance Decision

Accepted at `2026-08-01 22:26:51 +08:00` as AOT 07 A7.2M parameter source passing-form projection and VALUE-only
aggregate typed-call gating. Physical reference/address/writeback ABI, full A7.2, AOT 07, AOT 12, and the broader
AOT 07-12 goal remain active.
