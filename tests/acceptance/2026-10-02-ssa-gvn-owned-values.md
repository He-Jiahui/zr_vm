---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
  - docs/plans/ssa/guides/B-passes-analysis.md
tests:
  - tests/parser/test_ssa_gvn_range.c
  - tests/parser/ssa_gvn_owned_value_cases.inc
doc_type: acceptance-record
status: scoped-accepted
---

# SSA 02.02: ownership availability in GVN

## Scope and decision

Dominance proves a definition exists; it does not prove that an ownership
consumer leaves its payload available. The legal minimal case is
`CONST UNIQUE v1=17; MOVE v2<-v1; CONST UNIQUE v3=17; RETURN v3`.
Full structural/SSA/effect verification succeeds before and after the old
GVN, but replacing the second constant with `COPY v1` changes the return
value's owner state from INITIALIZED to UNINITIALIZED. UNKNOWN results have
the same problem: MOVE clears their payload too, and owner analysis records
the attempted copy of a consumed value as UNINITIALIZED.

The pass now accepts only UNKNOWN result ownership on both sides of CSE and
excludes any value consumed by MOVE, DROP, or DROP_IF_INITIALIZED anywhere in
the function. One value-sized table is prepared before rewriting. Its initial
allocation failure and malformed value metadata/consumer ranges or IDs report
structured diagnostics before mutation. Later rewrite allocation failures
retain the existing pass-manager transaction requirement. The exclusion is
deliberately conservative across blocks and later consumers, without claiming
an edge-sensitive availability analysis.

## Regression coverage

- Twenty cases exercise CONSTANT, COPY, ADD, and CONVERT under all five
  ownership modes. Full verification and owner-state availability are checked
  before optimization and after repeated GVN; successful CSE remarks are absent.
- Twelve cases reject owned COPY results and both mixed ownership directions.
- DROP, conditional cleanup with valid ownership memory/effect tokens, and a
  later MOVE exercise the whole-function consumer boundary.
- Five malformed metadata cases check exact diagnostic attribution and
  unchanged instructions, operand counts, and remarks.
- Existing ordinary scalar COPY, dominating arithmetic reuse, sibling rejection,
  canonical type, alias, and range regressions run in the same executable.

New checks use always-active `GVN_CHECK` and deterministic exit failures.

## RED evidence

On baseline `3a23f5fc`, MSVC built the new availability matrix successfully and
ran with exit 1. Every unoptimized case had expected availability; the first
and second GVN runs changed UNKNOWN state 0 or INITIALIZED state 1 to
UNINITIALIZED state 4 for all twenty cases. Receipt and exact diagnostics are
in `D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-owned-values/msvc-red-cache/`.
Production changes followed this RED. A GNU attempt timed out after 300 seconds
compiling the changed test unit and supplies no semantic RED evidence.

## Validation and artifacts

Artifacts and TMP/TEMP/TMPDIR are contained beneath
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-owned-values/`.
`incremental.py` verifies source hashes against the previous MOVE slice's
nineteen-object receipts, reuses matching objects, and rebuilds changed units.
The new include fragment is recorded in before/after source hashes. Linux
compilation uses a D: snapshot of the changed units and current include trees,
with a per-file SHA manifest, to avoid the E: source I/O timeout. Clang links
shared ASan with UBSan and executes with leak detection and halt-on-error.

All final focused gates built and ran with exit 0. Each final receipt records
unchanged before/after hashes for the nineteen C sources and new `.inc`.
All 210 snapshot files also match their current repository inputs by SHA.

| Gate | Final receipt directory | Recompiled / reused | Build / run |
| --- | --- | --- | --- |
| GCC 11.4 | `gcc-green-accepted` | 1 / 18 | 0 / 0 |
| Clang 14 shared ASan + UBSan | `clang-green-accepted` | 2 / 17 | 0 / 0 |
| MSVC 19.44 | `msvc-green-accepted` | 1 / 18 | 0 / 0 |

The GNU and MSVC final runs reused the already built GVN object from the
preceding fixture-validation attempts because its production source hash was
unchanged. The final test unit was rebuilt after correcting the later-MOVE
fixture's definition IDs. No full configure or shared build-tree mutation was
performed. Earlier green-named attempts are not acceptance evidence: the
additional DROP fixture initially lacked ownership memory/effect tokens, and
the later-consumption fixture initially had stale Value definition IDs. Their
exact failing receipts and logs remain available. The initial MSVC environment
bootstrap timed out after 60 seconds; subsequent native runs used the root's
verified toolchain environment cache.

The native final command was
`python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-owned-values/incremental.py msvc green-accepted msvc-green-final`.
The Linux commands use `wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env` with
`PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`, task-local
TMPDIR, and `/usr/bin/python3` running the same script under `/mnt/d/`.
GNU arguments are `gcc green-accepted gcc-green-snapshot`; Clang arguments are
`clang green-accepted`. Receipts contain every exact compile/link command.
Clang runs with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, and the shared runtime in
`/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux` on LD_LIBRARY_PATH.

## Previous artifact cleanup

The previous MOVE artifact root was resolved and checked before deleting any
file. `/proc` reported zero writable descriptors into that root; each selected
file also passed an exclusive Windows open check. Eighteen discarded RED,
static-link, and compiler-temporary files released 8,032,206 bytes. All 119
support objects retained their hashes, as did the three final representative
binaries. Logs and receipts remain. The cleanup manifest and writer census are
`cleanup-manifest.json` and `cleanup-active-writers.json` under this task root.

## Independent root verification

Root rebuilt the current registered MSVC target successfully (exit 0).
The formal `ssa_gvn_range` CTest passed in 42.24 seconds. Its two-test command
exited 8 because the separate generation test timed out; that command is not
claimed as a two-test PASS. The generation test subsequently passed on its own
without changing its source or timeout. Build and CTest output are retained in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/gvn-owned-discard-root-build.log`
and `gvn-owned-discard-root-ctest.log` in the same directory.

The first independent run of the final Clang shared-ASan/UBSan executable
failed after 36.464 seconds and emitted repeated `AddressSanitizer:DEADLYSIGNAL`
messages. Its cause remains unknown. The complete output is retained as
`root-sanitizer-diagnostic/root-failure.log.gz`, with a receipt confirming that
decompression matches the original log's SHA256. Two bounded GDB observations
reached `main` without a caught SIGSEGV; LeakSanitizer reported its unsupported
ptrace environment, so neither observation is a passing sanitizer gate.

One subsequent standalone run of the identical executable and shared ASan
runtime passed in 3.114 seconds (exit 0), with ASLR unchanged,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:handle_segv=0` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Address/undefined-behavior
instrumentation and leak detection remained enabled; ASan's SIGSEGV handler
was disabled. This additional passing gate does not explain the original
failure or establish stability across repeated runs. Exact binary/runtime
hashes, argv, output and exits are in `root-sanitizer-diagnostic/diagnostic-summary.md`,
`standalone-receipt.json` and `failure-archive-receipt.json` below the task root.

Independent review found no introduced defect in the frozen five-file scope.
The new fixtures verify owner analysis and structural/effect invariants; they
do not execute an Oracle differential. Allocation-failure injection and an
edge-sensitive availability proof are also outside this slice.

This record covers only the focused ownership boundary; SSA 02.02 remains
planned.
