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
