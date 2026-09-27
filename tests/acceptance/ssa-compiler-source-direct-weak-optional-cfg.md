---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_optional_value.inc
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
  - tests/parser/test_ssa_oracle_projections.c
  - tests/parser/test_ssa_c_llvm_lowering.c
status: partial
---

# SSA 01.02: direct weak optional source CFG

## Scope

Direct `weak?.method(...)` retains a source-owned present/absent CFG when the
weak receiver has a canonical ValueId and the known member call has supported
argument facts. The entry defines one semantic `OWN_CONSTRUCT(WAKE)` and
branches on its nullable result. The present path alone evaluates arguments
and invokes the member. The normal continuation drops the temporary wake owner
before the join. The exception edge reaches a landing block that reads the
payload, drops the owner, and explicitly throws that payload; the absent branch
never drops a nonexistent owner. Existing ExecBC behavior remains unchanged.
The skipped-argument rule follows `docs/zr_language_specification.md`'s
optional-receiver contract; `lua/QuickJS-master/tests/test_language.js` also
covers short-circuited optional-call arguments as reference-language evidence.

## Test Inventory

`test_direct_weak_optional_call_keeps_source_cfg` requires a weak guard fact,
WAKE-defined branch operand, guarded argument assignment in the present arm,
one normal and one exceptional DROP, no join DROP, explicit exceptional THROW,
and a successfully built ExecIR graph with two DROP instructions, one INVOKE,
and one THROW. `test_direct_weak_optional_value_merges_after_guard`
checks the two-predecessor nullable result merge and balanced path-local drops.
`test_unmodeled_weak_optional_value_abandons_source_cfg` suppresses source CFG
startup and requires no synthetic WAKE or partial graph. This is a producer
fallback injection; allocation-failure and cancel injection are not exercised.
The later opcode checkpoint requires source WAKE to stay `ZR_EXEC_IR_OPCODE_WAKE`
instead of silently becoming `COPY`, with ownership memory tokens on both sides.
The explicit `wake(weak)` source fixture
also counts it separately from ordinary copies. The projection fixture checks
that the direct oracle reports `UNSUPPORTED` at instruction 1 and that both
pointer-free projections retain WAKE while advertising non-runnable output;
attempting to run the ExecBC projection reports `UNSUPPORTED` as well.
The shared AOT lowering fixture requires one unsupported record for WAKE,
without counting it as a runtime bridge.

## Baseline And Tooling Evidence

The preceding focused pre-SemanticIR baseline was 102/102 cases. The initial
direct-weak tests failed because the source CFG was abandoned; a targeted GDB
backtrace traced the call rejection to an exact-argument check receiving a
function signature instead of the resolved member signature. Subsequent RED
tests exposed an exception cleanup block without an explicit terminator.

The WSL GCC 11.4.0 debug build at
`/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu` rebuilt the affected parser
objects, shared library, and focused test binary. Direct execution passed
105/105 Unity cases, including all three new tests. The producer's
`ValidatePreSemanticIr` and `ZrParser_ExecIr_Build` both succeed for the two
supported shapes.
The WSL Clang 14.0.0 debug static build at
`/home/hejiahui/zrvm-ssa-nested-clang.kBIWlA` passed the same 105/105 after
rebuilding the stale `semantic_ir.c` object; before that rebuild, the older
object made the pre-existing single-definition validation test fail.
The GCC adjacent `ssa_construction`, `ssa_builder_cfg`,
`ssa_builder_iterator_invokes`, `ssa_source_value_facts`,
`ssa_source_straight_line_cfg`, and `ssa_source_cleanup_cfg` CTest selection
passed 6/6. Its missing source-value-facts executable and four harness objects
were built before the run; no missing test was counted as passing. The final
ExecIR projection assertions were rerun on both toolchains (105/105 each).

For the distinct-WAKE checkpoint, the new source-opcode assertion first failed
under GCC (1 failure in 105 Unity cases): the builder emitted `COPY` for the
semantic wake. After the opcode, oracle, and projection changes, both the GCC
11.4.0 debug shared build and the Clang 14.0.0 debug static build passed
`zr_vm_pre_semantic_ir_test` (105/105 each), the direct
`zr_vm_ssa_oracle_projections_test` and `zr_vm_ssa_c_llvm_lowering_test`
executables (exit 0 each), and
`ctest -R "^(ssa_oracle_projections|ssa_c_llvm_lowering|ssa_construction)$"`
(3/3 each). The existing explicit-wake fixture now expects one `WAKE` instead
of counting it as a fourth `COPY`. The AOT test binaries initially lacked
some target objects in these reused build directories; the missing objects
were compiled and the linked tests rerun successfully. This is a compile and
fail-closed projection check, not a claim of runtime weak-upgrade equivalence.
The MSVC 19.44 debug build at `build/codex-ssa-conversion-msvc` compiled the
changed core, parser, oracle-projection, shared-AOT-lowering, and construction
targets; the same three-test CTest selection passed 3/3, including a rerun
after the parser rebuilt. The Windows `zr_vm_pre_semantic_ir_test` source
target compiled but did not link (LNK2019/LNK1120): its fixture references
`compiler_semantic_ir_find_slot`, `compiler_semantic_cfg_try_catch_is_supported`,
`allocate_local_var`, and `find_local_var`, which are compiler-internal and
not exported from the parser DLL. No Windows source-test success is claimed;
the source assertions passed 105/105 on both WSL toolchains.

## Acceptance Decision

Partial 01.02 source-CFG checkpoint only. Runtime differential behavior is not
established: ExecIR now preserves WAKE but neither its direct oracle nor its
new projections execute weak upgrades. Production ExecBC still uses `OWN_WAKE`.
The full plan's exception, loop, and cross-backend gates remain open. The compiler
state is freshly initialized per fixture; cancellation, OOM, and repeated-build
failure injection are outside this producer-only change.
