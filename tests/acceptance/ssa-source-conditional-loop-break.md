---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_loop_break.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_loop_break.inc
doc_type: testing-guide
status: msvc-validated-linux-pending
---

# Source conditional loop break acceptance

## Scope

Ordinary while-body conditional break through source semantic CFG, verified
ExecIR, Oracle, ExecBC projection, canonical materializer and actual Core VM.
No local mutation, loop-carried PHIs, conditional continue or cleanup extension.

## Baseline

The existing source suite has seven branch/arithmetic cases. The loop body
preflight rejects a conditional break because its containing if arm does not
fall through. The independent `begin_if` preflight additionally admits loop
transfers only through the existing finally protocol. The fresh MSVC baseline
source CTest exited 8: 11 tests, exactly the two new positive while cases failed
at source CFG publication. Existing seven cases and conditional continue/cleanup
negatives passed.

## Test inventory

- Existing seven source integration cases.
- True conditional break: first break executes, return 9.
- False conditional break: trailing break executes, return 9.
- Conditional continue: source CFG and all new-path outputs remain unpublished.
- Conditional break through try/finally: same nonpublication boundary.
- For and foreach conditional break: same existing nonpublication boundary.

Positive tests check published identity, full verification, both break targets,
zero PHIs/effect events, projected/materialized execution, selected break source
offset, PC/source mappings, declared CFG transitions and Oracle final block.

## Tooling evidence

All new logs, compiler temporary files, objects and binaries belong under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/source-break`.
MSVC 19.44.35228.0 fresh source target built 889 steps, exit 0 in 596.28 s.
The RED CTest used the registered `ssa_source_execbc_vm` selection with
`--no-tests=error`, exit 8, no timeout; its actual test duration was 6.26 s.
Logs and receipts under the directory above:

- `native-red-build.log` / `native-red-build-receipt.json`.
- `native-red-test.log` / `native-red-test-receipt.json`.
- `red-test-inputs-before.json` / `red-test-inputs-after.json`: 2177 inputs,
  no changes during the RED run.

The first configure recorded SCCP source drift; the initial build recorded
verifier and parser root-projection source drift from independent workers.
These facts remain in their receipts. Initial CMake MSVC `/showIncludes`
prefix detection also produced mojibake, so that tree's incremental header
tracking is not relied on for GREEN. The separate fresh GREEN tree also
required a local Ninja rule repair: actual CL output has the UTF-8 Chinese
include prefix. After terminal-state verification, 20 partial target objects
were removed from the owned tree and the rule prefix was set to those exact
bytes. The receipt records byte prefixes, SHA hashes and containment checks.
The corrected fresh support build completed all 889 steps, exit 0, in 461.78 s
with 2178 source/configuration inputs unchanged. Ninja deps for the CFG object
are VALID and include the new private header.

## Results

Expected RED observed; while-only producer changes implemented. The first
13-case GREEN run failed only the two positive cases at break source-site
identification: published CFG, ExecIR verification and Oracle checks passed.
Diagnostic source maps preserved lexer cursor offsets rather than keyword
offsets. The essential one-line parser correction makes break/continue capture
the existing token-location helper. Source-token assertions were retained.

Final exact-target incremental build exited 0 in 28.81 s; registered CTest
exited 0 in 2.91 s. Direct execution confirms 13 tests, 0 failures, 0 ignored.
The final 2227-input epoch is stable. Actual Ninja records contain 871 VALID
objects and no STALE objects. The provider seal records 563 actual repository
header dependencies and a configured-source/dependency superset; the superset
is not a claim that every configured target was compiled.

Final evidence: native-green-build-final.log, native-green-test-final.log,
native-green-unity-final.log, their terminal receipts, final-provider-seal.json,
final-ninja-deps.log and final-actual-dependency-inputs.json. Earlier RED and
source-map probe logs remain available.

## Reproduction

In the owned output directory, run run_msvc.py green-build with a fresh unique
stage suffix, then run_msvc.py green-test with a fresh unique stage suffix.
Both use the exact zr_vm_ssa_source_execbc_vm_test target and the registered
ssa_source_execbc_vm CTest, with parallelism 2 and all temporary output on D.
For a new tree, use the configure options recorded in run_msvc.py and verify
the Ninja showIncludes prefix against actual CL bytes before relying on header
dependencies. The repaired existing tree retains that reliable prefix.

## Acceptance decision

MSVC accepts this finite slice. GCC/Clang actual source VM integration remains
unverified: no matched-current Linux support library was available. A future
Linux gate must build current support from a native-created immutable D source
snapshot; MSVC libraries cannot establish a Linux runtime gate.
Full 01.02/01.05 and loop-carried PHI acceptance remain open.
