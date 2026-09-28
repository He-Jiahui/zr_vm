---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
  - tests/parser/test_buffer_pool_ffi.c
status: partial
---

# SSA 01.02: undefined `if` condition falls back before source CFG activation

## Failure and correction

The buffer pool lease reuse fixture compiles a nested logical condition with
comparisons and view indexing outside the source CFG preflight subset. Before
the fix, the condition stack slot still held an analysis-only SemanticIR ValueId
with no defining instruction. `compiler_semantic_cfg_begin_if` activated the
source CFG and appended blocks before its branch emission rejected that value.
The caller then reported `If condition lacks a semantic value` because the
partial graph remained active.

`begin_if` now checks that the condition ValueId has a defining instruction
before activating or extending the source CFG. If the check fails, it leaves
an inactive graph inactive or abandons an active graph, allowing the existing
ExecBC conditional path to compile the expression. A synthetic lower-layer
fixture binds an undefined value to the condition stack slot and requires
`begin_if` to return false with the source CFG inactive. The existing
canonical `if` and short-circuit fixtures remain the positive cases.

## RED evidence

- The test-only GCC run in `D:/tmp/zr_vm/ssa-artifact-v6-gcc` reported 115
  cases, one failure: the new fallback fixture observed
  `preSemanticIrCfgActive == true` after `begin_if` returned false.
- Read-only GDB on the original buffer pool lease test observed source line
  14, condition stack slot 33, ValueId 48, and
  `definitionInstructionId == 0` (invalid). At entry, CFG active was false;
  after `begin_if` returned false, CFG active was true with four blocks. The
  SemanticIR instruction and source-map lengths both remained 178.

## Validation

The WSL GCC 11.4.0 debug build under `D:/tmp/zr_vm/ssa-artifact-v6-gcc`
rebuilt `zr_vm_pre_semantic_ir_test`, `zr_vm_buffer_pool_ffi_test`,
`zr_vm_ssa_source_while_short_circuit_test`, and
`zr_vm_ssa_source_for_short_circuit_test` successfully. The direct
pre-semantic suite passed 115/115, including the new fallback test and the
existing canonical branch tests. The adjacent source `while` and `for`
short-circuit suites passed 8/8 each.

The original buffer pool lease reuse test advanced past the previous line 14
`If condition lacks a semantic value` diagnostic. It still fails at
`test_buffer_pool_ffi.c:487` because the compiler now reports `Active
contiguous view prevents source move or drop` at source line 9. That view NLL
issue is a separate follow-up, so this is a partial integration result. The
default FFI binary later aborts at a `closure.c:551` assertion in another
test; its full suite does not pass in this checkpoint.

## Boundary

This restores conservative fallback for an `if` condition without a defined
semantic value. It does not add source CFG support for comparison, view,
index, member, or general computed logical operands, and does not close the
full SSA 01.02 milestone. The change stays in the existing `begin_if`
responsibility of `compiler_semantic_cfg.c`; splitting that file for this
single guard would add indirection without isolating a new responsibility.
