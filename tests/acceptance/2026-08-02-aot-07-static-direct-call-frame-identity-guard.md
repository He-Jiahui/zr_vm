# 2026-08-02 AOT 07 Static Direct-Call Frame Identity Guard

## Scope

This A7.2N sub-milestone requires a same-module static direct call to use the exact metadata function and entry thunk
captured by its generated frame at the requested flat index. The runtime rejects null, incomplete, out-of-range, or
drifted identities before callee-frame preparation. Dynamic, meta, and cross-module calls are outside this slice.

## Baseline And RED

Frozen effective source is committed HEAD `8c3ff8a80590af0c543b70ce9f9bf9e8412ea3ec` plus the exact A7.2N
implementation/test overlay. The initial test-only RED failed to link on the missing identity predicate, and the new
source contract failed while the helper and pre-preparation gate were absent.
The strengthened real-API fixture was also rebuilt against the frozen HEAD `aot_runtime.c`; it reported three tests
with one failure on the unguarded drift path. Restoring the A7.2N runtime changed the same target to 3/0 GREEN.

## Test Inventory

- Accepts an exact generated-frame metadata/thunk snapshot and rejects metadata drift, thunk drift, null tables and
  entries, and independent function/thunk count bounds.
- Fixes the existing LLVM static-direct-call fixture to provide the same function/thunk snapshots produced by a real
  generated frame.
- Injects metadata drift and thunk drift independently against a configured runtime record, then compares the entire
  frame, caller call-info, stack top, destination value, source payload, and zeroed direct-call output.
- Restores exact identities and executes the original forced stack-relocation plus borrowed readonly receiver-alias
  success path.
- Locks source order as direct-call reset, identity validation, then frame preparation, and keeps dynamic/meta paths
  outside the changed production surface.

## Tooling Evidence

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72n-8c3ff8a-red-r1`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72m-b968f2d-final-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug
- SHA-256 matched all six controlled implementation/test files between main and both frozen trees.
- Independent review identified the incomplete legacy fixture and missing real-API side-effect proof; both were added
  without weakening the production guard. Final re-review reported no Critical or Important findings.

## Results

- WSL GCC and Clang each pass focused compatibility 8/0, LLVM symbol stripping/receiver alias 3/0, source contracts
  25/0, frame setup 1/0, call contracts 8/0, typed-call contracts 4/0, generic typed-call 24/0, and call shared-library
  smoke 5/0.
- Windows MSVC passes focused 8/0, LLVM 3/0, source contracts 25/0, frame setup 1/0, call contracts 8/0, and typed-call
  contracts 4/0. Generic typed-call reports 24 total with five expected Unix-only ignores and zero failures; call smoke
  reports five expected Unix-only ignores and zero failures.
- Existing multiline source/frame-setup contracts initially false-failed against CRLF files. Only non-owned AOT backend
  sources in the frozen validation copies were normalized to LF; main and assertions were unchanged.
- The pre-existing ExecBC AOT pipeline baseline remains 97 total with eight unrelated historical failures and is not
  used as this focused sub-milestone's acceptance gate.

## Acceptance Decision

Accepted at `2026-08-02 01:09:20 +08:00` as AOT 07 A7.2N same-module static direct-call frame identity guarding.
Physical ref/out storage and writeback, aggregate destination/return ABI, spill/address-taken slots, cross-module
generation binding, A7.3 environment keys, full A7.2, and the broader AOT 07-12 goal remain active.
