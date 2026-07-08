# AOT 07-S6 GC reference register stage acceptance

Timestamp: 2026-07-06 07:35:26 +08:00

Plan slice: M1.5 / 07-S6 GC reference registers and frame root table

Status: 07-S6 plan acceptance completed for the generated conversion-reference-local path; the broader 07~12 goal
continues with 07-S7 and later stages.

## Completed Scope

- Generated conversion reference locals use `SZrRawObject *` fields in `SZrAotReferenceLocals_<flatIndex>` instead of
  truthiness-time `SZrTypeValue` rereads.
- Generated C emits an independent `LOCAL_ADDRESS` root map and pushes a separate `zr_aot_ref_gc_root_frame` whose base
  is `&zr_aot_ref_locals`; ordinary VM-stack `FRAME_BYTE_OFFSET` roots remain on the ordinary root frame.
- `TO_STRING` / `TO_OBJECT` reference-local writeback is followed by a safepoint before immediate truthiness consumes
  the raw object, so GC can observe and update the local-address root under pending debt.
- Runtime root-frame coverage includes frame-byte roots, local-address raw-object roots, safepoint debt, write barrier,
  and native-call pin behavior.

## Verification

- WSL GCC generated-C smoke/contracts: logical-not 8/0, jump-if 9/0, source contracts 24/0, logical contracts 4/0,
  frame setup contracts 1/0.
- WSL Clang generated-C smoke/contracts: logical-not 8/0, jump-if 9/0, source contracts 24/0, logical contracts 4/0,
  frame setup contracts 1/0. Existing smoke-helper `const char *` to `TZrNativeString` warnings remain.
- Windows MSVC Debug generated-C smoke/contracts: build passes; Unix-only smoke bodies are expected ignored with zero
  failures; source/logical/frame setup contracts pass 24/0, 4/0, and 1/0.
- AOT GC root-frame runtime matrix: WSL GCC 6/0, WSL Clang 6/0, Windows MSVC Debug 6/0.

## Status

- 07-S6 acceptance gates are GREEN for the implemented generated path: GC pressure/safepoint work is exercised and the
  reference registers are visible through the AOT root frame.
- Remaining work moves to 07-S7 anti-regression gates, then stages 08 through 12.
- Longer soak/stress runs and broader object-allocation patterns remain useful follow-up coverage, but are not blocking
  this 07-S6 plan acceptance record.
