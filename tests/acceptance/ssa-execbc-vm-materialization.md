# SSA 01.05: initial ExecBC to Core VM materialization

## Scope

`ZrParser_ExecBcProjection_MaterializeVmFunction` emits a regular Core
`SZrFunction` from a validated, no-call ExecBC projection. The supported first
slice is zero-parameter i64 constants, signed ADD/SUB, signed less/greater
comparison to bool, conditional CFG branches, scheduled phi copies and one-i64
return. VM execution uses the existing Core dispatcher; neither the direct
ExecIR oracle nor `ZrParser_ExecBcProjection_Run` is treated as VM execution.
The validator checks block phi inputs against raw parallel-copy rows and
symbolically proves that scheduled per-edge moves realize those assignments.
CFG adjacency is matched bidirectionally by edge occurrence, preserving legal
parallel edges. It rejects nonzero instruction deopt IDs and synthetic blocks
with an instruction body or explicit terminator.

The focused VM fixtures construct ExecIR directly. They exercise the real
ExecIR-to-projection-to-materializer-to-Core-dispatch path, but do not exercise
source parsing/compiler metadata through that path.

Physical projection slot zero maps directly to VM `frameBase[0]`. The emitted
frame covers physical slots, sparse holes and the reserved phi temporary slot.
Packed frame metadata, typed binding rows, CALL, effects, non-i64 arithmetic,
unsupported compare kinds and unsupported opcodes return structured failures
without publishing a function or falling back to the legacy compiler.
Emission begins with `entryBlockId`, regardless of block numbering, and starts
each block only after slot types, operand ranges and CFG terminators have been
validated.

The ExecBC projection has no schema or contract field. Its ExecIR producer
checks the source function's binding-row schema before projection; the
materializer checks the actual projection fields, including `runnable`, each
instruction's `bindingRow`, and packed-frame metadata. Hand-assembled inputs do
not bypass the producer boundary.

When the projection includes a constant pool, each constant `layoutId` is
strictly checked as a pool index; each referenced entry must be an unflagged
i64/bool constant, and only referenced entries are copied. With no pool, the
existing inline scalar literal convention is preserved. Every generated VM PC
receives a native-owned map row with the originating ExecIR instruction, block
and source IDs.

## Test inventory

The focused `ssa_exec_ir_execbc_vm` target runs actual emitted Core functions
and compares their return values and traversed block/source trace with
`ZrCore_ExecIr_RunOracleEx`:

- zero-parameter i64 ADD/SUB with a return value in slot zero;
- compare true and false branches selecting separate phi inputs;
- a no-temporary phi move that reads physical slot zero while the
  zero-initialized `phiTemporarySlot` field is also zero;
- a critical-edge split with a phi copy in its synthetic block;
- a loop swap whose scheduled phi copies use the reserved temporary slot;
- a non-first entry block (block 2) whose parallel conditional edges reach the
  same lower-numbered return block; both critical-edge occurrences are split,
  and the taken path is traced as blocks `{2, 3, 1}`;
- pooled i64 and bool constants, exercising both bool branch outcomes;
- sparse physical slots, including complete frame clearing;
- CALL rejection, non-i64 type rejection and invalid opcode rejection, each
  requiring empty output;
- nonzero per-instruction deopt rejection with instruction/source diagnostics;
- missing and wrong scheduled phi moves, with structured rejection;
- a CFG edge whose target predecessor row omits the edge, requiring structured
  rejection before materialization;
- malformed synthetic-block instruction ownership, with an invalid-block
  diagnostic.

The Unity fixture cleanup runs before assertions, and setup/lease flags cover
early exits. Successful functions are rooted immediately after materialization;
freeing the emission only releases its PC-map sidecar. Allocation-failure
cleanup frees the partial function and then removes a root added by this call.
If root removal itself fails, the function is retained under Core/GC ownership
until state teardown so the root membership table cannot refer to freed memory.
The output record is fresh and zero-cleared on entry; callers free it before
reusing it.

## Baseline and resolved REDs

The first CFG reciprocity regression showed that a branch edge omitted from its
target predecessor row was accepted; the reverse-edge validator and
`test_phi_rejects_cfg_edge_missing_from_target_predecessors` now reject that
projection. The valid parallel-edge fixture then exposed an expected-trace
omission: the producer splits the taken critical edge through synthetic block
3, so the VM path is `{2, 3, 1}` rather than `{2, 1}`. Both cases pass in the
current focused run.

## Tooling evidence

On 2026-09-30, root ran this six-target MSVC incremental build through the
native wrapper; CMake completed 13/13 steps in
`D:/tmp/zr_vm/ssa-artifact-v6-msvc`:

```text
python run_native.py accessor-vm-gc-build build zr_vm_ssa_core_model_test zr_vm_ssa_static_binding_facts_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_binding_rows_artifact_test zr_vm_ssa_exec_ir_execbc_vm_test zr_vm_gc_nested_mutation_test
cmake --build D:/tmp/zr_vm/ssa-artifact-v6-msvc --parallel 8 --target zr_vm_ssa_core_model_test zr_vm_ssa_static_binding_facts_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_binding_rows_artifact_test zr_vm_ssa_exec_ir_execbc_vm_test zr_vm_gc_nested_mutation_test
```

The exact CTest wrapper and underlying test selection were:

```text
python run_native.py accessor-vm-ctest ctest -R '^(ssa_core_model|ssa_static_binding_facts|ssa_oracle_projections|ssa_binding_rows_artifact|ssa_exec_ir_execbc_vm)$' --no-tests=error
ctest --test-dir D:/tmp/zr_vm/ssa-artifact-v6-msvc --output-on-failure --timeout 120 -R '^(ssa_core_model|ssa_static_binding_facts|ssa_oracle_projections|ssa_binding_rows_artifact|ssa_exec_ir_execbc_vm)$' --no-tests=error
```

The focused logs are `D:/tmp/zr_vm/ssa-control/accessor-vm-gc-build.log` and
`D:/tmp/zr_vm/ssa-control/accessor-vm-ctest.log`.

## Results

The focused CTest selection completed 5/5 tests (2.08 seconds total); the VM
CTest took 0.44 seconds and all 16 Unity cases passed.

## Acceptance decision

This accepts only the focused, manually constructed scalar/control VM slice on
the recorded MSVC run. The separate source-to-VM path remains RED for the
broader source metadata/`PLACE_BASE` shape: the latest recorded source target
run failed both branch cases at materialization (`code=28`, `actualVersion=1`)
in `D:/tmp/zr_vm/ssa-control/gc-providers-current-ctest.log`. That target was
not part of the current 5/5 CTest selection. No source-to-VM pass is claimed.
The wider 01.05 Oracle/ExecBC/AOTIR/C/LLVM semantic matrix and complete M1
remain open, including effects and typed calls.
