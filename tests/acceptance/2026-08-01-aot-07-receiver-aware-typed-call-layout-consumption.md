# 2026-08-01 AOT 07 Receiver-Aware Typed-Call Layout Consumption

## Scope

This A7.2I sub-milestone lets generic/shared inline-struct `CALL_TYPED` selection consume a canonical receiver row at
parameter index zero. The receiver is already part of `argumentCount`, so the caller source remains
`operand0 + 1 + argumentIndex`; no runtime API, dictionary ABI, artifact, manifest, or reachability schema changes.

Source instance-method calls currently lower to `DYN_CALL`. This acceptance covers the AOT consumer contract once a
verified receiver-bearing `CALL_TYPED` exists; it does not claim that instance-method producer gap is closed.

## RED And Review Evidence

- The first proposed source fixture used an instance generic method. Generated SemIR showed `DYN_CALL`, so it never
  exercised the target selector; that assumption and its temporary production change were reverted.
- The effective RED starts from a free generic call that already emits `CALL_TYPED`, then marks its first projected
  reference parameter as the canonical receiver. With unchanged production code, GCC reported 12 passes and one
  failure because the shared callsite marker remained absent.
- Independent review confirmed that compiler and runtime both include receiver at argument index zero, that runtime
  stages exactly `argumentCount` slots from `functionSlot + 1`, and that the dictionary `staticMethod` thunk ABI must
  remain unchanged.
- Final review found that the original one-argument unknown receiver negative stayed green only because no other
  reference argument could select the route. Replacing it with `[receiver/unknown, explicit/OBJECT, scalar]` made the
  intermediate A7.2I implementation fail at 13/14. Requiring `isReferenceParameter` in the receiver branch restored
  14/14, and the dynamic instance boundary now directly asserts the generated `DYN_CALL exec=` row. Re-review found no
  remaining correctness, ABI, or test issue.

## Coverage Inventory

- Accepts role-free parameters or one exact `ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER` at parameter index zero.
- Rejects unknown/combined roles and any receiver role outside index zero.
- Requires receiver TypeRef to independently project as OBJECT/ARRAY and its caller source to remain a full VALUE slot.
- Preserves exact equality among parameter sidecar count, frame parameter count, and runtime argument count.
- Keeps receiver and explicit arguments in the same `operand0 + 1 + argumentIndex` window.
- Compiles and loads generated C for a receiver/reference/scalar three-argument window and compares interpreter/AOT
  result 73, proving the scalar remains at its original index after receiver consumption.
- Retains A7.2E producer tests for unknown role bits, misplaced/duplicate receivers, missing identity, and slot drift.

## Tooling Evidence

Frozen effective source is committed HEAD `ec7c978e1366b819fe455a4d42635f1f479af7c1` plus the exact A7.2I four-file
production/test overlay:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72d-3d67352`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72d-3d67352`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug

All four implementation/test files match across main, WSL, and Windows:

- `81e57bd6ae49fa36e618d408cd046ca0290272241a451a476fc513d908ea971d`
- `07f603a38781bd74800d07a44023c0d519305ccd5544a35ceee58f1c6ec35150`
- `fa649d813681aaf749eb725762eb38e9315ce7d1e7550082c62dd09be5501317`
- `f66847035beda7207b241e589bd208e3824d90065aa35a37a2c14293c17d26c5`

## Results

- WSL GCC: generic typed-call 14/0; value-SemIR contracts 8/0; MethodInfo 3/0; code stripping 37/0; SemIR 10/0;
  generic sharing 9/0; debug metadata 6/0; typed-call contracts 4/0.
- WSL Clang: the same eight groups pass with counts 14/0, 8/0, 3/0, 37/0, 10/0, 9/0, 6/0, and 4/0.
- Windows MSVC x64 Debug: all eight targets build and pass; generic typed-call reports ten passes and four expected
  Unix-only ignores. Existing temporary-directory MSB8029 warnings remain non-blocking.

## Acceptance Decision

Accepted at `2026-08-01 09:47:07 +08:00` as AOT 07 A7.2I's receiver-aware `CALL_TYPED` parameter-layout consumer.
A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain active.
