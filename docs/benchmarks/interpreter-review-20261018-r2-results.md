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
