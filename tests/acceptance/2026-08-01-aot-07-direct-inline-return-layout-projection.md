# 2026-08-01 AOT 07 Direct Inline Return Layout Projection

## Scope

This A7.2L sub-milestone projects one uniform direct inline `STRUCT` TypeLayout id from typed SemIR return sources
into function-level ExecIR. It gates inline-struct call destinations, return sources, and return-source skip-drop. It
does not define a complete aggregate callable ABI, caller destination storage, serialized metadata, or a new
reachability schema.

## RED And Review Evidence

- The initial frozen-HEAD contract failed on the missing ExecIR sidecar and accessor before production changes.
- Independent review supplied an incompatible-layout RED: two valid TypeLayout ids with differing return-copy shape
  were initially treated as unknown. The projector now rejects that function by comparing the exact runtime-relevant
  byte, alignment, copy, drop, GC, ownership, and field maps.
- Review also required direct skip-drop behavior, null SemIR/type-table inputs, missing/non-inline return sources, and
  a genuinely removed owner. The final fixture proves a legal 3-to-2 trim before corrupting that removed owner and
  requiring pre-strip writer failure.
- Final independent static re-review of the extracted projector, compatibility predicate, orchestration, consumers,
  and focused tests reports `No findings`.

## Coverage Inventory

- Projects one uniform direct `STRUCT` layout id and isolates it from poisoned raw callable ids.
- Preserves unknown for legacy dynamic returns, unknown callable identity, union, indirect alias, no typed return, and
  copy-compatible nonuniform ids.
- Rejects invalid type indexes, null type tables, explicit static-id mismatch, semantic TypeRef mismatch, null SemIR,
  missing/non-inline sources, incompatible layouts, and malformed owners that would otherwise be trimmed.
- Accepts direct aliases and rejects only `INDIRECT_ALIAS` for this direct layout contract.
- Gates `CALL_TYPED`, `RETURN_TYPED`, and skip-drop on sidecar authority and exact frame-layout match.
- Exercises a 140-byte non-interned type name and a current Unix `.zro` roundtrip to prove content comparison and
  projection survive serialization.
- Leaves caller destination storage, nested callable returns, union/indirect storage, parameter direction,
  spill/address-taken slots, and GC/ref provenance open.

## Tooling Evidence

Frozen effective source is committed HEAD `92feb0ce2e306ef6c3b8738487ae0f0d849ae340` plus the exact A7.2L
seven-production/five-test overlay:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72l-92feb0c-r1`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72l-92feb0c-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug

## Results

- WSL GCC and Clang each pass MethodInfo 11/0, return 1/0, SemIR 11/0, load-const scalar 1/0, code stripping
  37/0, generic typed-call 19/0, generic sharing 9/0, debug metadata 6/0, value-SemIR 8/0, typed-call contracts
  4/0, typed scalar 1/0, and call shared-library smoke 5/0.
- Windows MSVC passes MethodInfo 8/0, return 1/0, SemIR 10/0, code stripping 37/0, generic sharing 9/0, debug
  metadata 6/0, value-SemIR 8/0, and typed-call contracts 4/0. Generic typed-call has 19 total with four expected
  Unix-only ignores; load-const scalar, typed scalar, and call smoke contain only expected Unix-only ignores and zero
  failures.
- The current-object roundtrip projection case is Unix-only because private ExecIR symbols are not exported by the
  Windows parser shared library; Windows still builds and runs the source contracts and public writer route.
- MSVC retains the existing temporary-directory MSB8029 warning. An existing multiline return source contract was
  normalized from CRLF to the same frozen WSL LF input before its final 1/0 run.
- SHA-256 matches for all twelve controlled implementation/test files across main, frozen WSL, and frozen Windows.

## Acceptance Decision

Accepted at `2026-08-01 15:26:32 +08:00` as AOT 07 A7.2L direct inline return-layout projection and guarded
call/return consumption. Full aggregate callable ABI, A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain
active.
