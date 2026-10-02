---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
  - "user: 2026-10-02 scoped parser multi-inline-fields repair"
tests:
  - tests/parser/test_ssa_roots_observation.c
doc_type: acceptance-record
status: scoped-accepted
---

# Parser inline-field root specifications

## Scope and baseline

This slice repairs the parser-owned frame-root adapter. Root specifications
describe reference locations, so their count can exceed the number of logical
values: one inline aggregate may contain several reference fields. The previous
`specCount > logicalSlotCount` guard incorrectly rejected that valid input
before per-specification validation.

The production change removes that guard and documents the distinction. It
preserves `calloc` failure handling, candidate-map construction, value/physical
slot lookup, kind and storage-class checks, pointer-field bounds and duplicate
rejection. Failed construction still leaves the existing map intact. There is
no allocator-failure injection in this fixture; the unchanged OOM branch is
verified by diff inspection, rather than claimed as a tested allocation failure.

The existing `ssa_roots_observation` registration is reused. No repository
CMake files, core production files or core regression tests change. The slice
does not wire root specifications into source-language GC, safepoint/codegen,
ExecBC, AOT or native pin lifetimes, and does not complete 04.04.

## Reference evidence

The checked-in reference implementations enumerate aggregate members rather
than treating an aggregate as only one reference location:

- Lua's `traverseudata` visits every initialized user-value cell in
  [lgc.c](../../lua/src/lgc.c).
- CPython's `tuple_traverse` visits each tuple item using `Py_VISIT` in
  [tupleobject.c](../../lua/CPython/Objects/tupleobject.c).
- QuickJS's `js_array_mark` calls `JS_MarkValue` for each array element in
  [quickjs.c](../../lua/QuickJS-master/quickjs.c).

These are aggregate-enumeration references. This repair retains the existing
explicit parser field-root descriptors and makes no claim to implement those
collectors or their lifetime rules.

## Functional inventory

`test_multiple_inline_fields` uses the real `LayoutPackedFrame` API to produce
one INLINE_SPAN logical value containing three pointer-sized fields. It checks:

- Three roots sharing the logical/physical slot but using distinct field
  addresses and pointer values; the last field ends exactly at the frame end.
- An uninitialized middle field produces no callback; the remaining two
  callbacks still target their own field addresses and values.
- Duplicate `(valueId, kind, fieldByteOffset)`, out-of-span fields, unknown
  logical values and unknown kinds produce the expected diagnostic and leave
  map pointer/count/capacity and every descriptor byte unchanged.

The existing managed/derived ordering, relocation, storage-class validation,
observation failure atomicity and reused-physical-slot cases run in the same
binary. All checks use always-active `CHECK`, including in `NDEBUG` mode.

## Tooling and exact commands

All artifacts, snapshots, logs and TMP/TEMP/TMPDIR paths are below
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\parser-inline-fields` (`$TASK` below).
There are no new whole-tree configurations. Immutable Python drivers use a
300-second build watchdog and a 60-second CTest watchdog; the temporary focused
CTest itself has a 30-second timeout. Receipts preserve exact argv, exits and
before/after source/driver SHA256 values.

`snapshot-red-manifest.json` and `snapshot-green-manifest.json` record 211
source/header files copied to D: and verified against the workspace. An
unsuccessful Linux driver included four complete C sources in one translation
unit; it timed out and contributes no passing Linux evidence. The subsequent
recursive header-expansion experiment was interrupted before generating a
translation unit or running a compiler, following root review. No expanded
input contributes to acceptance. MSVC uses three real
parser/test sources and the existing formal `matrix/msvc/lib/zr_vm_core.lib`,
matching that archive's `/MDd` runtime and root-captured VS environment.

```powershell
python "$TASK/run-red-native.py" msvc red-msvc-diagnostic
python "$TASK/run-final.py" msvc green-msvc
python "$TASK/native-clang.py"
```

The unsuccessful Linux attempts used this prefix, followed by the driver,
mode and label recorded in their receipts:

```text
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/parser-inline-fields/tmp
TMP=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/parser-inline-fields/tmp
TEMP=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/parser-inline-fields/tmp
/usr/bin/python3
/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/parser-inline-fields/<driver>
gcc <label>
```

Each driver invokes `ctest --test-dir <mode> -R
'^ssa_roots_observation$' --output-on-failure --interactive-debug-mode 0`.
The temporary driver is focused support-API evidence; root separately verifies
the repository's formal target before committing.

## Results and failures

Actual RED was recorded before the production change. The diagnostic MSVC
build exited 0; CTest failed in 2.80 seconds with
`multi-inline build diagnostic=11 (INVALID_RANGE=11)` and `CHECK failed: built`,
exit 8. The same binary's direct run reproduced that diagnostic and exited 1
in 0.876 seconds. Logs are `red-msvc-diagnostic-{build,ctest}.log` and
`red-msvc-diagnostic-direct.log`.

| Mode / label | Build exit | CTest exit | Observed result |
| --- | --- | --- | --- |
| MSVC Debug / `green-msvc` | 0 | 0 | Exact test 1/1 PASS, 1.53 seconds |
| GCC Debug / `green-gcc` | 124 | Not run | Build watchdog timeout; no Linux PASS |
| Windows Native Clang 19.1.5 Debug | 0 | 8 | Exact test timeout, 43.52 seconds |
| Windows Native Clang NDEBUG | Not run | Not run | Stopped after Debug timeout |

Every failed setup attempt is retained separately:

- `run.py` native launch initially failed to find `cl.exe`; resolving its
  absolute path from the captured toolchain environment corrected the launch.
- Direct `exec_ir.obj` linking failed with six unrelated unresolved ExecIR
  dependencies (build exit 2). Using the formal Core archive repaired the
  dependency set; the next build exposed a CRT mismatch (exit 2), fixed by
  `/MDd`. Neither setup failure is counted as product RED.
- `red-msvc-v4` built successfully but CTest timed out at 41.56 seconds (exit
  8). A bounded direct run reached the expected CHECK failure (exit 1), and
  the subsequent diagnostic CTest reached the same real failure in 2.80
  seconds. The timeout is retained and is not counted as the expected RED.
- GCC separate-TU builds from both workspace source and a D-only snapshot
  timed out at the 300-second watchdog (exit 124). The latter's owned `cc1`
  was observed in `p9_client_rpc`; PID/start/cwd/argv evidence is retained in
  `red-gcc-v2-processes.json`. Only the driver's own process group was stopped.
- The subsequent whole-source unity attempt (`run-linux.py gcc green-gcc`)
  also hit its 300-second watchdog (exit 124). No GCC/WSL CTest completed.
  Root then stopped further preprocessing experiments; `flatten.py` was
  interrupted through its own exec session (exit 1) before producing its output
  or starting the proposed flattened-input compilation. No recursive-expanded
  source is counted as a Linux gate.
- The single Windows Native Clang 19.1.5 attempt compiled the original
  repository sources against the formal Core archive successfully (exit 0).
  Its Debug CTest timed out at 43.52 seconds without test output (exit 8).
  Following root's bounded-validation instruction, no retry or NDEBUG run
  followed. `native-clang-receipt.json` confirms input/driver hashes remained
  unchanged. This is a build PASS only, not a Clang functional PASS.

Before the new slice, obsolete prior RED sources, the abandoned compiler-ID
partial tree and old non-final MSVC objects were removed: 11 files,
1,331,167 bytes. `previous-cleanup.tsv` records resolved paths, sizes and
SHA256 values after no-reparse and exclusive-open checks. Final GCC/Release/
Clang/MSVC caches and binaries, outer logs, receipts and root control artifacts
were retained. An initial full-machine `MainModule` enumeration produced
permission errors and was interrupted through its own exec session (exit 1);
cleanup was then completed using scoped writer evidence and candidate-file
exclusive opens (exit 0).

## Acceptance decision

The bounded parser support-API repair is scoped-accepted on MSVC Debug, based
on the actual diagnostic RED and exact 1/1 GREEN. No obsolete compatibility
path is introduced by the one-guard removal.
GCC/WSL validation remains unpassed, and sanitizer validation was not run for
this slice. Windows Native Clang is a separate Windows compiler gate and cannot
replace Linux/sanitizer evidence; its functional gate timed out. The CHECK
macro remains active under `NDEBUG`, but an NDEBUG executable was not validated
in this slice. Root independently verifies the repository's
formal target as recorded below. Production GC/safepoint integration and the
full 04.04 milestone remain open.

## Independent root verification

Root rebuilt the current repository target with the reusable MSVC Debug
configuration; the build exited 0. The registered `ssa_roots_observation`
CTest passed in 40.82 seconds. The accompanying SCCP CTest also passed, and
the two-test command exited 0. Exact commands and output are retained in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/sccp-parser-inline-final-build.log`
and `sccp-parser-inline-final-ctest.log` in the same directory.

An independent review of the final four-file scope found no introduced defect.
The review confirmed that allocation still uses separate `calloc` arguments,
per-field bounds and duplicate validation remain in place, and failure does
not publish the candidate map. This adds a formal MSVC functional gate; the
unpassed Linux, Clang runtime and sanitizer gates above remain unpassed.
