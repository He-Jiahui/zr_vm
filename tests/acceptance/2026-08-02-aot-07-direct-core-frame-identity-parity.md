# 2026-08-02 AOT 07 Direct-Core Frame Identity Parity

## Scope

This A7.2O sub-milestone applies the A7.2N exact generated-frame metadata/thunk identity rule to
`ZrLibrary_AotRuntime_CallStaticDirect` and `ZrLibrary_AotRuntime_CallInlineStruct`. Both entries must reject generation
drift before stack growth or caller call-frame, call-info, and storage mutation. Generated descriptor frames must carry
identity snapshots even when they do not publish exports.

## Baseline And RED

Frozen effective source is committed HEAD `014be1e599d7b07f953ea6c2f05c6272319163de` plus the exact A7.2O
implementation/test overlay. Against the frozen HEAD runtime, the focused suite reported 12 tests with four failures:
each new drift case reached the supplied thunk. The direct-core source-order contract also failed without a pre-growth
identity gate. After the runtime gate, generic typed-call exposed four nested-call failures; a strengthened frame-setup
contract then failed because identity snapshots were still export-only.

## Test Inventory

- Injects metadata-table drift and thunk-table drift independently for static and inline-struct direct-core calls.
- Uses real runtime state, functions, a native closure, caller call-info, generated frame tables, and inline return
  layout metadata; a counter proves the supplied thunk is never invoked on rejection.
- Compares caller call-info base/top, active call-info, stack top, frame base, generated entries, callable object/type,
  and destination payload after each failure.
- Locks callable metadata resolution and exact identity validation before `ZrCore_Function_CheckStackAndGc` in both
  APIs.
- Locks function/thunk tables and both counts before `includeExportContext`, while module and code-registration fields
  remain inside that branch.

## Tooling Evidence

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260802-a72o-014be1e-red-r2`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260802-a72o-014be1e-final-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug
- LF-normalized SHA-256 matched all seven controlled implementation/test files among main and both frozen trees.
- Independent review and final re-review reported no Critical or Important findings; the re-review confirmed direct
  placement coverage for both snapshot counts.

## Results

- WSL GCC and Clang each pass focused compatibility 12/0, source contracts 26/0, frame setup 1/0, generic typed-call
  24/0, call shared-library smoke 5/0, LLVM symbol stripping/receiver alias 3/0, call contracts 8/0, and typed-call
  contracts 4/0.
- Windows MSVC passes focused 12/0, source contracts 26/0, frame setup 1/0, LLVM 3/0, call contracts 8/0, and typed-call
  contracts 4/0. Generic typed-call reports 24 total with five expected Unix-only ignores and zero failures; call smoke
  reports five expected Unix-only ignores and zero failures.
- Four pre-existing multiline source contracts initially false-failed against CRLF AOT backend files. Only backend
  sources in the frozen validation copies were normalized to LF; the main workspace and assertions were unchanged.

## Acceptance Decision

Accepted at `2026-08-02 02:55:46 +08:00` as AOT 07 A7.2O direct-core frame identity parity. Dynamic/meta/cross-module
generation binding, physical ref/out storage and writeback, aggregate destination/return ABI, spill/address-taken slots,
A7.3 environment keys, full A7.2, and the broader AOT 07-12 goal remain active.
