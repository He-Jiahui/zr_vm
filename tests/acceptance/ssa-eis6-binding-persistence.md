---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_read.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_write.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/01-schema-relocation.md
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
tests:
  - tests/parser/test_ssa_binding_rows_artifact.c
  - tests/parser/test_ssa_binding_rows_artifact_capacity.inc
  - tests/library/test_ssa_canonical_zraf_eis6_guard.inc
  - tests/cmake/ssa-tests.cmake
doc_type: acceptance-evidence
status: accepted-stage
---

# EIS6 Scalar Binding Persistence

The public scalar codec now persists typed binding rows, including a typed
empty table, in EIS6. This completes the bounded persistence subtask. General
artifact schema/relocation and complete binding symbol closure remain open.

## Behavioral Evidence

The typed CALL fixture first passes Core module verification. Roundtrip checks
the entire scalar graph, contracts, reciprocal instruction-row association,
binding hash, memory pools and deterministic little-endian bytes. Negative
cases cover reserved header bits, byte length, bounded counts, overflowing
memory ranges and conflicting row references. Rejection leaves caller buffers
or the empty output module unchanged.

Capacity is not a serialized field. Typed empty and populated tables with
extra reserved capacity roundtrip using only live entries. A typed CALL without
binding facts also roundtrips, matching the Core producer contract; persistence
does not prove target closure.

Static tests intercept allocations in ExecIR storage, binding-row storage and
the EIS6 reader. The observed decode has ten allocation sites. Every site is
failed in turn, returning LIMIT with byte-identical empty output and unchanged
allocation balance. Each injected failure is followed by a successful retry
and matching binding hash. This sweep does not intercept all verifier or writer
allocations.

The canonical ZRAF opener rejects typed EIS6 after public scalar decoding and
Core verification. Typed empty and typed CALL guard fixtures verify the nested
payload diagnostic offset and absence of publication in both the module opener
and hotpatch validator. Legacy EIS1-5 encoding remains selected for schema zero.

## Validation

The initial typed-row tests failed before the codec was implemented; the RED
log is `D:/tmp/zr_vm/ssa-control/eis6-real-red-ctest.log`. Current MSVC 19.44
execution passed the typed-row contract and the ten-site fault sweep, with the
full test log preserved in
`D:/tmp/zr_vm/ssa-control/canonical-constants-eis6-msvc-last-test.log`.

Current GCC 11.4 and Clang 14 WSL runs passed `ssa_binding_rows_artifact`,
`ssa_exec_ir_artifact_v6_write`, `ssa_exec_ir_artifact_v6_roundtrip`,
`ssa_canonical_zraf_validation` and `ssa_capability_validation`. The 19-suite
initial selection had one separate conditional-cleanup fixture failure.
Original logs are preserved at
`D:/tmp/zr_vm/ssa-control/{gcc,clang}-current-scalar-before-phi-ctest.log` and
the corresponding `last-test.log` files.

After the fixture repair, final GCC and Clang runs each passed all 19 selected
suites, including these five persistence and loader gates. Final logs are
`D:/tmp/zr_vm/ssa-control/{gcc,clang}-current-scalar-final-{build,ctest,last-test}.log`.

All caches, generated artifacts and compiler temporary files for these runs
are under `D:/tmp/zr_vm`. No artifact is moved between volumes.

INVOKE, phi, ownership/GC/deopt/frame/state maps, general source canonical type
tables, and complete symbol resolution remain outside EIS6's scalar subset.
