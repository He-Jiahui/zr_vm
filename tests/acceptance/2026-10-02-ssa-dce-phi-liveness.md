---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - tests/parser/test_ssa_dce_phi_liveness.c
  - tests/parser/ssa_dce_phi_liveness_cases.inc
  - tests/cmake/ssa-sccp-conversion-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_dce_phi_liveness.c
  - tests/parser/ssa_dce_phi_liveness_cases.inc
doc_type: acceptance
status: scoped-accepted-msvc-clang-sanitizer
---

# DCE PHI Liveness Acceptance

Evidence recorded on 2026-10-02 UTC (implementation checkpoint 18:16:44 UTC),
which is 2026-10-03 locally in Asia/Shanghai. This record covers the separate
02.01 DCE support repair after representation commit `d785697d`.

## Problem And Scope

A fully verified diamond with branch-local constants, a PHI, ADD, and RETURN
executes as signed 223 through Oracle and ExecBC. Previous DCE deleted PHI
incoming definitions during the instruction scan before the next PHI liveness
iteration. The full pipeline rejected its own output with INVALID_VALUE (13).
The same failure occurs with an ordinary signed-zero condition, independently
of the separately documented pooled floating SCCP analysis facts.

Files:

- `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c`
- `tests/parser/test_ssa_dce_phi_liveness.c`
- `tests/parser/ssa_dce_phi_liveness_cases.inc`
- `tests/cmake/ssa-sccp-conversion-tests.cmake`
- `docs/instruction-generation/execir-pass-pipeline.md`
- `tests/acceptance/2026-10-02-ssa-dce-phi-liveness.md`

DCE now computes instruction/PHI liveness to a fixed point before any deletion.
Retained unused PHIs keep their incoming definitions as structural uses.
The complete deletion sweep budget is reserved before publication. Existing
observable and metadata roots are preserved, including explicit effect/memory
token carriers. SCCP numeric and truthiness transfers are outside this change.

## Coverage

The dedicated `ssa_dce_phi_liveness` CTest and executable contain 16 groups:

- Four direct/transitive incoming diamond cases across DCE and SCCP+DCE.
- Two chained-PHI cases and two loop-backedge cases across both pipelines.
- A retained unused PHI with defined incoming values and unrelated dead chains.
- Five root/boundary cases: deopt values, GC roots, debug state maps, owner MOVE,
  and throw effects.
- Transaction rollback after DCE followed by an intentionally invalid pass.
- Exhaustive direct DCE cancellation across all 43 smaller nonzero work budgets.

Successful fixtures fully verify and execute through actual Oracle and ExecBC
before and after optimization, then repeat the pipeline and require an identical
function hash. Diamonds return 223, chained PHIs return 223, the finite loop
returns 3, and the unused-PHI fixture returns 2. Unrelated pure chains become
NOPs. Every canceled budget preserves instruction bytes, function hash,
verification, runtime results, `changed == false`, `lastSourceId == 0`, and an
empty remark sink.

## Linux Sanitizer Evidence

Artifacts are under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/dce-phi-liveness`.
The focused build compiles only the new test and changed DCE source. It borrows
32 support objects from the fresh 34-TU Clang sanitizer build, after checking
their object, source, and actual dependency SHA256 hashes against current files.
Clang 14 uses unchanged shared ASan/UBSan flags. Native LLD 19.1.5 links the same
support order and sanitizer inputs with the previously validated private
path-only libc script. All temporary outputs stay on D:.

The final corrected test and DCE compilations exit 0 with empty compiler logs.
The corrected test includes the existing private pass declaration header, so
there are no implicit-declaration warnings. Actual depfile dependencies for
the changed test and DCE translation units (173 normalized paths) were hashed
after compilation; the private declaration header hash is included. Corrected
link exits 0. The immutable corrected
test object is also linked with the old DCE object and the same 32 support
objects: only the DCE object and output destination differ.

Final bounded old-DCE execution exits 1 with diagnostic 13, block 4,
instruction 12. Final bounded new-DCE execution exits 0 and prints:

```text
DCE budget cancellation: 43 limits preserved IR and remarks
DCE PHI liveness: 16 groups passed (diamonds, chained PHIs, loop, roots, dead chains, transaction and budget)
```

New-DCE binary SHA256:
`4b5b1f461f353d776ef25b4ac03f884623b9dd54e397584efa3aa6c0cf22d31d`.
Old-DCE comparison binary SHA256:
`408b2b0b8aa66bfea40df3373afecb400e765805091d9fb0706f863744e89080`.
The shared corrected test object SHA256 is
`f26e11732663d062dda26eb728ac59892d502df1acbad294c64301b7b282ca03`.

Both executions use `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; instrumentation and leak
detection were not weakened. Final successful corrected runtime elapsed 4.786221744
seconds; comparison runtime elapsed 2.645679742 seconds. No sanitizer error
appeared in these bounded executions.

Final acceptance uses the corrected `declared/compile-receipts.json`,
`declared/actual-dependencies-sha256.json`, `declared/runtime-bounded-receipt.json`,
and `red-declared/comparison-receipt.json` / `runtime-bounded-receipt.json`.
The current test source SHA256 is
`802530f099fde3b8f29190d68e287e6df416e87203d32d08c486094cf3d28023`;
the production DCE source SHA256 is
`70b9e33491a1f69a232998db5243418f6771c162b9464b2d834814e0fc3e507f`.

## Formal MSVC Gate

The root agent's corrected formal MSVC target build exits 0 without C4013.
CTest `ssa_dce_phi_liveness` passes 1/1 in 2.64 seconds. Root evidence is in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/dce-phi-liveness-declared-formal-build.log`
and `control/dce-phi-liveness-formal-ctest.log`. This gate validates the dedicated
target registration and corrected fixture on MSVC independently of Linux
sanitizer execution.

At 2026-10-02 18:43:40 UTC (2026-10-03 in Asia/Shanghai), the root agent also
ran the corrected declared binary independently under Ubuntu-22.04 with the
same shared Clang ASan/UBSan binary and D: temporary directory. The control log
is `D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/dce-phi-liveness-root-clang-sanitizer.log`.
It exits 0 after 3.259 seconds and prints the same 16-group and 43-budget
receipts. This is the independent final Linux sanitizer gate; the local
4.786-second bounded run above remains supporting evidence.

## Failed Attempts And Limits

The first pre-correction final new-DCE execution and old-DCE comparison both repeatedly printed
`AddressSanitizer:DEADLYSIGNAL`, then hit the 120-second bound and recorded exit
124. Their cause is unlocalized; the evidence does not prove an environment
failure or that they occurred before main. Original timeout receipts and the
first 5,000 characters of each failure log remain in `final/runtime*` and
`red-final/runtime*`. The first runner buffered output but persisted only that
prefix, so the remainder is unavailable. A corrected bounded runner limits
captured output and kills only its owned process group. One standalone retry
of each unchanged binary produced supplementary RED/GREEN results. The corrected
declaration-header rebuilds supply the final gates above.
An earlier green run predates both the stronger instruction-byte/remark checks
and the declaration-header correction; it is retained as supplementary evidence only.
The earlier Clang test compilation emitted two implicit-declaration warnings,
and the first formal MSVC build emitted C4013 for the same direct DCE call.
Those builds are superseded and do not count as final acceptance gates. The
fixture now includes its real existing private declaration header. Earlier
`final` binary `7b543e1d...` and `red-final` binary `55b53704...` timeout and
successful retry receipts remain supplementary evidence only.

This record does not claim unused PHI removal, pooled SCCP numeric/truthiness
repair, full SSA milestone completion, GCC validation, or a whole-project green
matrix. Acceptance is limited to the corrected dedicated MSVC build/CTest and
the corrected Clang shared ASan/UBSan witness runs recorded above.
