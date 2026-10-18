# Interpreter follow-up implementation — 2026-10-18

Branch: `perf/interpreter-20261017`; starting tip: `42d16e29`.
Requested by John He; findings from `perf-interpreter-20261018-r2.md`.
All work and evidence are in `/workspace/zr_vm-perf`.

## R2-H1: merged SSA build gate

The shared scalar pass source list now includes PHI identity and its alias
availability dependency. Three hand-listed targets consume this list; the
remaining affected targets inherit it. The source-frame OOM object has the
private Core include root. Updated the old PHI-copy assertions to permit the
new singleton-PHI use substitution while preserving blocked incoming edges,
ownership/state-map guards, boundary instructions, and repeat-pass idempotence.

Validation: full Release build succeeds; core CTest **417/417 pass** (64.36 s).
Commands use `build/tools/bin` and `build/local-tools/usr/bin` on PATH and the
local tool library directory on LD_LIBRARY_PATH, as required by the report.
The first gate exposed the missing transitive alias provider and stale PHI
expectations, which are included in this batch. Changes stay on this branch;
no checkout or changes to main were made.

Remaining findings follow the requested order: C1, H2, H3, H4 first step, M1,
M2, M4, H4 countdown, M3. CI wiring follows the functional gates.

## R2-C1: linear thread-root scans

Functions cache VALUE/inline-layout presence and physical stack-slot coverage.
Packed mirrors use a range; sparse and cross-slot layouts use a bitmap. The
existing finalization points rebuild the summary after compilation/load; inline
moves transfer ownership, and function teardown frees the bitmap. Unfinalized
host metadata and allocation failure retain checked fallback. Debug builds
cross-check cached coverage against the original linear query. Inline type
resolution remains at frame granularity because it depends on runtime bindings.

Thread scans now visit frames once and cover staging gaps and the region below
the oldest frame. Debug assertions establish that each layout fits its frame
window; coverage queries are bounded by scanned slots. Minor forwarding uses
the same coverage and handles the stack gaps. Added sparse/cross-slot coverage
and deep-recursion correctness/work regressions and reusable deep allocation
probes with an optional depth ratio gate.

Validation: full Release rebuild; **32/32** targeted GC/frame/struct CTests;
**14/14** interpreter GC unit checks under ASan+UBSan+forced-full GC; integer
boundaries, SSA admission, budget dispatch and typed operands sanitizer binaries;
**15/15** sanitizer smoke probes, all clean.

Pinned CPU 5, Release, median of 3: depth 0 **333.38 ms**, depth 400
**375.03 ms**, ratio **1.125** (target <= 2). Exported GcStep wrapper:
maxima depth 0 **157.8 us**, depth 400 **438.4 us**, depth 1600 **843.0 us**
(target < 2 ms at depth 1600; the stronger 800 us bound still has two misses).
No builds were running during these timings; host load was about 2.4 and falling
following rebuilds. Callgrind depth 100: **2.916 G Ir** versus 28.4 G;
FrameStackSlotIntersectsFrameGcValue **0.77%** exclusive; the former linear
presence helper is inlined and absent from attribution (combined target < 1%).
Evidence: `build/r2-c1-*`, `build/r2-ab/c1*/results.json`.

## R2-H2: occupancy scheduling and minor work sets

Automatic generational steps now wait for nursery occupancy and schedule the
next check from remaining headroom. Explicit requests still collect immediately;
skips do not inflate collection counts. Completed-minor telemetry adjusts a
nursery cap against the configured pause budget, with a 64 KiB floor. This is a
feedback target for synchronous minors, not a hard pause guarantee.

Minors record scanned objects for forwarding, and maintain a young/new prefix
before a consolidated old suffix for preparation, evacuation, finalization and
reclamation. Major sweep invalidates that suffix. Permanent roots have a native
registry, with conservative heap discovery on registration OOM. Short-string
pruning uses a corresponding young/new prefix plus the existing young bucket
indexes. Rewrite-worklist OOM retains the original checked heap walk. Debug
builds assert the old suffix contains no movable young objects.

Collection fixtures that previously relied on a few bytes of debt now request
minors explicitly; their forwarding and ownership assertions are preserved.
The stress runner also resolves relative build paths before changing directory.

Validation: final full Release rebuild and **417/417** core CTests pass
(62.58 s). ASan+UBSan+forced-full GC: **70/70** GC, **15/15** interpreter GC,
**12/12** AOT root, **95/95** instruction checks, both session-checkpoint and
SSA dispatch boundary CTests, and the 15 smoke probes pass. The first full gate
found additional instruction/checkpoint/stack-reload fixtures relying on tiny
debt; these now request the minor they assert, with the original assertions.

Final timings, CPU 5, no builds/tests overlapping, initial load **0.89 / 1.73 /
1.82**: scale-16 checksum **47994849** and banner preserved, **3,486** minors
(wrapper sees 3,485 idle minors plus one major-completion/minor step) versus
8,400. Idle minors total **1,766.5 ms**, average **506.9 us**, maximum
**2,708.3 us**, **60** over 800 us; major completion maximum **5,436.9 us**.
The average-minor target is met; synchronous minor/major worst-pause bounds
remain open. Core source/binary medians: baseline **1491.362 / 1473.789 ms**;
stress **1489.196 / 1445.002 ms** (C **17.605 / 18.227 ms**). Baseline improves
68% from r2's 4,647.99 ms but still misses both the 1 s intermediate and 300 ms
final goals; M2 addresses the next identified mutator costs. Raw suite reports
mark the environment INCOMPARABLE without a captured environment report, so
these timings do not establish a universal 5% regression gate.

The requested stress runner passes for C/source/binary at scale 16: baseline
**9502.104 / 8613.377 ms**, stress **9074.002 / 8513.626 ms** (source/binary).
Evidence: `build/r2-h2-final-*` and `build/r2-h2-verified-ctest.log`.

## R2-H3: scalar returns and receiver staging

Single-result returns no longer reject compiled escape metadata. Scalars skip
escape marking; GC results retain it under state publication and frame reload.
All debug, exception, call-status, return-destination and close-work guards
remain. Packed VALUE frames skip the out-of-line drop only when both mirrors
have no ownership; other frames retain the complete cleanup. The compiler
omits escape metadata for live, unambiguous scalar typed locals, checking the
binding's base type, ownership and array flags and retaining reused/temporary
slots conservatively.

Exact argument staging now accepts a GC object receiver with no ownership,
control or weak metadata and identical source/staged bits. Enabled the two
existing argument-staging CTests, previously orphaned from the CMake graph;
the VM fixture now uses non-overlapping physical call windows and checks the
receiver entry as well as its existing stale-window scalar fallback.

Validation: full Release rebuild; **53/53** report-selected call/return/escape/
frame/closure/exception/ownership/staging CTests; **5/5** selected sanitizer
CTests and all four call-heavy sanitizer smoke probes pass.

CPU 5, medians of 3, initial load **0.33 / 0.77 / 1.16**, no overlapping
builds: calls **662.67 ms**, fib **164.26 ms**, methods **727.77 ms**, closure
**803.64 ms**. Methods improve about 44% from the report's 1289.64 ms; the
400/60/450 ms and closure 25% targets remain open. The exported-helper probe
sees escape/drop **0/1** for calls, fib and methods, and **1/2** for closure:
only startup/teardown work, **zero per iteration**. Callgrind trip-count diffs
(100k/300k calls, 20k/120k methods): **2154.6 Ir/call** versus 2288 and
**3823.4 Ir/method iteration** versus 7416. Evidence: `build/r2-h3-*`.
Further call-state caching, direct caller dispatch and null-fill redesign
remain the report's prerequisite-heavy next stage; M1 addresses bytecode
staging first.

## R2-H4 first step: polls across VM transitions

Removed only the per-transition resource countdown reset. Budget-pointer
changes and native returns retain immediate checks. Added exact admission and
trace equality tests across VM calls/returns (including a callee limit), a
caught throw, and native callback reentry into ZrCore_Execute through the real
call API. Tests include zero, quantum boundaries, the last instruction and
completion, and assert counts observed inside trace callbacks. The benchmark
accepts `--source FILE [--expected INTEGER]`, preserves the numeric loop mode,
checks every sample, and emits JSON including cost per admitted instruction.
Small calls/fib source fixtures are registered CTests.

Validation: full Release rebuild; **9/9** report-selected budget/dispatch/close
CTests pass in both Release and ASan+UBSan+forced-full GC; **6/6** exact budget
unit tests. CPU 5, five alternating samples, no overlapping builds:

| Program | Plain ms | Budget ms | Overhead | ns/instruction | Clock reads/run |
|---|---:|---:|---:|---:|---:|
| default loop | 311.897 | 442.463 | 41.86% | 1.451 | — |
| intloop file | 322.880 | 455.176 | 40.97% | 1.470 | 351,570 |
| calls | 621.790 | 701.813 | 12.87% | 2.425 | 128,914 |
| fib | 153.300 | 157.155 | 2.51% | 0.809 | 18,629 |

Clock counts are totals divided by five budgeted samples, including about
eight timer/startup reads per sample. They match instructions/256, versus the
report's two reads per call. Timing samples exclude the preload counter.
The loop and calls 10% goals remain open for the later countdown step after
M4. Evidence: `build/r2-h4a-*`.

## R2-M5: CI wiring

Added `interpreter-gates.yml` for main/perf-branch pushes, main PRs and manual
runs. The gating Ubuntu job builds all dependencies with ASan/UBSan and forced
full GC, runs the standalone GC suite plus the report's selected CTests, and
runs all 15 smoke probes. The non-gating Release job records smoke JSON, the
full-allocation deep pair with the ratio check, and intloop/calls/fib budget
JSON. Both retain evidence artifacts even on failure. The commands have been
exercised locally in the batches above; hosted runner results are pending CI.
The existing TSan attached-domain job remains enabled in the platform workflow.

## R2-M1: compact scalar bytecode

Scalar self-aliasing result stores snapshot payload and type before clearing
metadata. Float producers now support direct result stores. Numeric literal
trees fold only when their checked result is representable and finite;
source SemIR retains the original operations for CFG/ExecIR admission. Dead
scratch-only null resets left by folding are removed. Constant signed branch
and float ADD/MUL opcodes are appended, preserving existing opcode numbers.
Their operand roles, relocation, verifier, C/LLVM AOT lowering and writers
are updated. Scalar call arguments load directly into the call window;
managed reference arguments retain their reference staging. The typed AOT
counting-loop matcher accepts the compact self-update shape.

Validation: full Release build and **57/57** compiler/quickening/execbc/AOT/
loader/staging checks; **25/25** focused quickening assertions, including
checked literal overflow, direct arguments and loop shapes. ASan/UBSan with
forced full GC passes quickening and integer boundaries, and **15/15** smoke
probes. Floatloop compiles, reloads and runs as binary with checksum 4950494;
its generated AOT C passes host C syntax checking. Existing generated AOT
execution and frame-free shape gates pass.

CPU 5, median of three, no overlapping builds/tests (load about 1.1):

| Probe | ms | r2 target |
|---|---:|---|
| loop_lt | 251.08 | <=400; met |
| loop_le | 250.99 | <=467; met |
| floatloop | 142.31 | <=250; met |
| calls | 606.50 | <=400; still open |
| intloop | 249.01 | >=30% below 454; met |

Both canonical integer loop bodies are **4 instructions**, floatloop is
**7**, satisfying the <=4/<=8 shape targets. Loop_lt/loop_le ratio is 1.000.
Evidence: `build/r2-m1-*`, `build/r2-ab/m1-final/results.json`.

## R2-M2: single-owner synchronization and temporary roots

Domain mutexes now elide on the native owner thread, with an atomic draining
handoff before any foreign root API or second attached mutator can enter.
Nested owner scopes may finish during handoff. Per-state LIFO roots replace
object-write and weak-string insertion leases; collection scans them, native
pointers remain pinned, reserve failure preserves state, and TryRun unwinding
restores the saved depth. Barrier early exits run inside an exclusive owner
scope and retain remembered-set and ignored-root maintenance. ELF profiling
TLS uses initial-exec. Stack growth no longer races a shared stop flag: its
raw allocator cannot collect, and stack pointers are restored before the
mutator can acknowledge a safepoint.

Validation: full Release rebuild and **423/423** core CTests; **24/24** selected
ASan/UBSan/forced-full-GC CTests, **70/70** standalone GC checks, and **15/15**
sanitizer smoke probes. All **3/3 TSan** checks pass, including attached-domain,
foreign root handoff and distinct-root threads. An extra full forced-GC language
suite was stopped in its expensive compiler integration test; full ASan coverage
is reserved for the final layout batch. Full-suite fixtures now exercise runtime
division through a variable and the direct AOT argument copy, and preserve all
legacy opcode number assertions while checking the appended opcodes.

Scale-1 gc_fragment Callgrind: **2.782 G Ir**, pthread mutex lock/unlock only
**150 Ir total** (well below 1%); barrier **197.5 Ir/call** versus 669, still
above the 150 target. The methods trip-count diff has **zero `__tls_get_addr`**.
CPU 5, no overlapping builds/tests: methods median of three **744.53 ms**;
core GC source/binary one-sample checks baseline **1432.792 / 1433.831 ms**,
stress **1482.369 / 1366.008 ms**. These remain above the 450/300 ms goals.
Evidence: `build/r2-m2-*`, `build/r2-ab/m2*-*.callgrind` and `m2-final/results.json`.

## R2-M4: verified bytecode and guarded host compatibility

A shared nonallocating operand/control-flow validator now covers IO, final
source quickening, ExecIR lowering and lazy host entry. Strict bodies must be
nonempty and end in a non-yielding return, tail call or in-body unconditional
jump. Jump-to-end and fused iterator skip/branch overruns are rejected. Source
quickening adds an explicit empty return landing for former fallthrough/end
sentinels, preserving source and execution-location metadata.

Verified return-terminated functions use unchecked fast fetches and, when all
slots have a packed VALUE mirror, direct slot addressing. Debug assertions stay
in place. Nonempty legacy host bodies may fall through only after operand and
control-flow validation, with the guarded dispatcher and bounded slot lookup;
they never acquire the strict verification flag. Hosts must clear the runtime
flag before mutating published instructions/operand metadata. Low-level capture
fixtures now describe their closure storage, and metadata-only IO fixtures have
an explicit return.

Validation: strict verifier regressions cover empty/missing terminators, final
yields, invalid opcodes/slots/constants/call windows, jumps and fused skips.
ASan/UBSan/forced-full GC: **7/7** loader/ExecIR/verifier/integer/budget CTests,
**95/95** low-level instruction checks. The report mutation probe completes
**118 cases: 70 rejected, 48 valid executions; zero sanitizer reports, crashes
or timeouts**. A timing-sensitive persistent-protocol test had a 650 ms sleep
outlier under concurrent sanitizer work; the final Release gate is recorded
below. Evidence: `build/r2-m4-clean-*`, `build/r2-m4-final-fuzz.log`.

Final full Release rebuild succeeds; core CTest **424/424 pass** (62.14 s).

## R2-H4 countdown and CORR-L2 prerequisite

A single dispatch countdown now covers the next mutator safepoint, resource
quantum and exact instruction limit. Admitted counts settle at observable
boundaries, including helper protection, VM transitions, trace callbacks,
native/resource polls and completion. Resource intervals survive VM calls.
A state-reachable context lets the real integer-helper error path publish the
throwing PC/stack and pending count before allocation. TryRun snapshots and
restores the context while its C locals are live, before longjmp; catch never
traverses abandoned contexts. Counts saturate at UINT64_MAX.

Validation: full Release rebuild and core **424/424** CTests pass (62.42 s).
All **8/8** exactness unit tests pass, including the new saturation and
post-loop inline overflow regression, with identical traced/untraced admission
counts and source line 4 in the captured Error. **8/8** report-selected
ASan/UBSan/forced-full-GC budget/dispatch/close/integer CTests pass.

CPU 5, five alternating samples, no concurrent builds/tests; load about 3.0:

| Program | Plain ms | Budget ms | Overhead | ns/instruction | Clock reads/run |
|---|---:|---:|---:|---:|---:|
| default loop | 232.736 | 239.593 | 2.95% | 0.098 | — |
| intloop file | 214.715 | 228.147 | 6.26% | 0.192 | 273445.4 |
| calls | 576.129 | 583.247 | 1.24% | 0.264 | 105476.4 |
| fib | 149.173 | 152.686 | 2.35% | 0.850 | 16146.4 |

All measured overheads meet <=10%. Clock counts are the wrapper's total divided
by five budgeted samples, about instructions/256 plus eight startup/timing
reads per sample. Counts are 70,000,005 / 27,000,008 / 4,131,540 instructions
for intloop/calls/fib after M1. Absolute calls/fib and GC worst-pause/throughput
targets remain unmet. Evidence: `build/r2-h4b-*`.

## R2-M3: versioned frame coordinates and smaller values

Bytecode patch 46 encodes frame sizes and layout offsets as physical slot
coordinates plus a checked intra-slot remainder. Older artifacts are rejected
with a clear recompile diagnostic; loader tests cover old versions and invalid
coordinates. The tracked string-build benchmark artifacts were regenerated
from their unchanged sources. All native consumers must rebuild for this ABI.

Values and stack slots now use natural alignment: **40 bytes / 48 bytes**,
alignment **8**. CallInfo is **240 bytes**; RawObject/String/Object remain
**208/240/480**; Function is **1280** including the GC coverage caches. A
registered footprint probe guards the 64-bit value/slot target. The separate
ownership/header design is `value-layout-and-ownership-next-steps.md`.

Compiler VALUE mirrors start at physical slot boundaries and reserve a full
slot even after inline payloads. Native returns use dense values and native
children start beyond the whole reserved window. Generated native AOT entries
initialize and explicitly activate their byte layout before GC can scan inline
fields; metadata-only native frames keep that layout inactive. Both AOT entry
paths and minor forwarding follow the same rule. Cross-mutator visits validate
against the scanned stack and mark through the collecting state. Shared/traced
dispatch also avoids treating control counts/flags as destination slots.

Regressions cover native general/fast returns, active generated partial slots,
other-mutator roots, mixed compiler layouts and tail reuse ownership release.
The tail fixture checks the released shared reference and new dense null slots
instead of reading the overwritten inactive partial-slot view.

Validation so far: full Release rebuild and **425/425** core CTests pass
(286.79 s). ASan/UBSan/forced-full GC: **94/94** selected AOT/artifact/loader/
frame/GC/budget/dispatch/debug CTests, **17/17** interpreter GC unit tests,
**70/70** standalone GC, **5/5** native return checks, **4/4** tail reuse,
**40/40** inline layout tests and **15/15** smoke probes pass. The native
value-type shared-library smoke passes **6/6**. Patch-46 mutation probe:
**118 cases, 71 rejected / 47 normal**, zero sanitizer reports/crashes/timeouts.
Evidence: `build/r2-m3-active-layout-*`, `build/r2-m3-complete-*`,
`build/r2-m3-final-release-*`. Final ASan and timing evidence follows below.
