# 2026-07-30 AOT 11/12 Native Import Contract Reachability

## Scope

This sub-milestone connects canonical `SZrNativeImportContract` rows to AOT 12 owner-linked trimming:

- every contract in the original function tree is validated before ExecIR and code stripping;
- retained rows publish stable owner, contract index, symbol/hash identity, `edge.native_import`, and predecessor;
- before/after/removed counters expose contract trimming independently from function counts;
- an invalid contract on an unreachable owner fails closed and leaves no generated file;
- native import reason names are shared with the reachability schema but rejected as function-to-function edges.

This is an AOT C consumer/validation slice for AOT 11 A11.2 and does not claim a new `.zro/.zrm` section schema.

## RED Evidence

The frozen WSL GCC build compiled and linked the expanded code-stripping suite against unchanged production. All 26
existing checks passed. The new manifest/count check failed because no native import reachability output existed, and
the malformed-unreachable check failed because filtering removed the bad owner before the old validation point.

## Coverage Inventory

- Retained flat owner 0 owns symbols `0x101` and `0x102`; unreachable owner 1 owns `0x202`; sparse retained owner 2
  owns `0x301`.
- Counts report four contracts before filtering, three after filtering, and one removed.
- Manifest rows are ordered `(owner,contract) = (0,0), (0,1), (2,0)` with canonical symbol/hash identity and owner
  predecessors. The exact owner ranges are `(start,count) = (0,2), (2,0), (2,1)`.
- The generated immutable table preserves `retained_native_a`, `retained_native_b`, `retained_sparse_native` order and
  omits `trimmed_native`.
- Corrupting the unreachable contract schema version returns false before ExecIR and leaves no artifact.
- A function table with `indexSpace > capacity` is rejected before it can drive a manifest scan.
- The reason schema returns `edge.native_import`, rejects a later unknown enum, and rejects native import as a function
  graph edge.

## Validation

Effective source is commit `873791e` plus the exact eight code/test overlays for this sub-milestone. Validation reused:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: reachability 34/0 and code stripping 28/0.
- WSL Clang: reachability 34/0 and code stripping 28/0.
- Windows MSVC: reachability 34/0 and code stripping 28/0.
- SHA-256 for all eight production/test files matched the main worktree in both frozen trees.
- Generated C contained the 4-to-3 delta, the three ordered retained rows and exact sparse range table, and no
  unreachable entry point. The malformed output path was absent.
- Independent review found both explicit and flat-index-derived malformed index spaces could drive an over-capacity
  scan, plus missing sparse multi-contract order coverage. All three findings were fixed and regression-tested; final
  re-review returned no findings.

The adjacent GCC `native_extern_contract` target built and passed 22 of 27 checks. Five callback source cases failed
only because the active syntax cutover rejects their legacy `%import` and lambda-without-`fn` text before reaching the
writer. No parser, fixture, CMake, or FFI runtime change is included here.

MSVC retained only the existing MSB8029/MSB8064 warnings caused by the isolated build being below `%TEMP%`.

## Acceptance Decision

Accepted as the native import contract reachability sub-milestone. Canonical AOT C contract retention is now
validated, auditable, and fail closed; artifact schema/provider parity, native thunk behavior, complete AOT 11/12,
and the broader AOT 07-12 goal remain active.
