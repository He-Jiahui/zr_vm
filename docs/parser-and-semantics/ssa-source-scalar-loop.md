---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_scalar_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_scalar_expression.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_scalar_result.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_scalar_result.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_loop_phi.inc
  - tests/parser/ssa_source_execbc_vm_scalar_guards.inc
  - tests/parser/test_ssa_source_straight_line_cfg.c
  - docs/parser-and-semantics/scalar-literal-place-scratch-promotion.md
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_scalar_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_scalar_result.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
  - .codex/plans/20261004-source-comparison-producer-lowering-design.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_loop_phi.inc
  - tests/parser/ssa_source_execbc_vm_scalar_guards.inc
  - tests/acceptance/ssa-source-execbc-vm.md
doc_type: module-detail
status: finite-source-loop-windows-ubsan-accepted-v4
---

# Finite source scalar while loops

The current producer extension admits a bounded source shape into the existing
SemIR CFG, Place promotion, ExecIR Oracle and ExecBC/Core VM route. Root V4
accepts the current finite Windows UBSan route, including six real-source loops,
seven refusal guards and the selected regressions. The evidence and broader
limits are recorded below.

## Source admission and witnesses

The added while condition uses signed canonical i64 operands with `<` or `>`.
Scalar expressions are integer literals, local identifiers and recursively
formed `+`/`-` expressions. The private `scalar_expression` helper first checks
AST shape, then confirms canonical i64 types through inferred literals and
registered local slots. A separate witness check ties each value to its actual
SemIR definition, result/type, exact source range, opcode and recursively
matching operands. An identifier requires a LOAD from a valid Place; a literal
requires a CONSTANT with a pool index. Shape alone is insufficient.

The added loop body admits ordinary scalar expression statements and local
identifier assignment using exactly `=` with a supported i64 expression.
It does not add compound assignment, member/projection assignment, nested loops,
conditional transfer or cleanup. Float, unsigned and mixed representations do
not qualify for this extension. Those inputs retain ordinary compilation and
the executable source CFG extension declines ownership; existing independently
supported source forms retain their own admission rules.

The private `scalar_result` binder preserves the connection between an emitted
LOAD/result and the compiler stack slot. It checks canonical type, source slot,
Place identity, symbol and actual defining instruction. Fresh results use a
temporary Place; an existing local identity is retained only for the exact
same-slot load. Loan-bearing and unsupported types do not receive this scalar
binding. Canonical i64 literals, loads and transfers bind their existing pure
value to a typed temporary without emitting INITIALIZE or manufacturing a scalar
scratch proof. For local initialization, narrow reuse requires an existing
same-type, loan-free TEMPORARY Place and a matching typed definition. The local's
real INITIALIZE consumes that existing value. This does not introduce a COPY
initializer or weaken existing scalar scratch/Place eligibility protections.
Unsupported initialization keeps its existing path.

## Existing promotion produces the loop PHI

The extension supplies real source CFG edges and typed local operations to the
existing Place promotion implementation. It does not construct a fixture-only
loop PHI or seed its result. The required header PHI receives the entry initial
value and the body's ADD/SUB update on the backedge, in verified predecessor
occurrence order. The update uses the previous header value, and the comparison
uses that same loop-carried value.

The source fixture calls Core VerifyAll before examining PHIs or executing the
Oracle. Its PHI inspection follows COPY chains and checks entry initialization,
body-local arithmetic definition, a body successor/backedge relationship and
condition use. A PHI count alone does not establish loop-carried semantics.

## Oracle and Core VM harness

The ordinary source chain parses and compiles source, validates preSemanticIr
and its executable CFG, builds ExecIR, verifies it, runs the Oracle, lowers to
ExecBC and materializes a canonical-type Core function. The Core dispatcher
result must equal the independently expected return and Oracle return.
Mapped VM PCs retain instruction source IDs, follow CFG successor edges and end
at the Oracle's final block. Repeated mapped blocks distinguish an executed
backedge from a zero-trip case; this is not full Oracle/VM trace equality.

The harness preserves unused NULL constant-pool descriptors created by local
declarations. A CONSTANT instruction that references such a NULL entry remains
unsupported, both in fixture preparation and canonical VM adaptation. This
does not admit NULL as an executable scalar. Oracle initial values are supplied
only for values explicitly flagged EXTERNAL_ENTRY and used as PLACE_BASE
provenance. Definition-zero local PHI results are not external inputs and are
never silently seeded.

## Current coverage and scoped acceptance

The current `--loops-only` group contains six source cases:

| Source | Expected return | Backedge |
| --- | --- | --- |
| `var i:int=0; while(i<3){i=i+1;} return i;` | 3 | Taken |
| `var i:int=3; while(i<3){i=i+1;} return i;` | 3 | Zero trips |
| `var i:int=3; while(i>0){i=i-1;} return i;` | 0 | Taken |
| `var i:int=2; while(i<3){i=i+1;} return i;` | 3 | One iteration |
| `var i:int=1; while(i>0-2){i=i-1;} return i;` | -2 | Taken |
| `var i:int=0; while(i+1<4){i=i+1;} return i;` | 3 | Taken |

Each requires Core VerifyAll, real loop-carried PHI witnesses, the expected
comparison/arithmetic opcodes, Oracle/Core VM agreement and no semantic effect
events. The negative endpoint uses a supported SUB expression, and the final
case exercises an arithmetic expression inside the comparison, not a nested
loop. The separate `--scalar-guards-only` group has seven cases: float, unsigned,
mixed width, multiply update, divide update, compound update and indexed
assignment. Each requires legal ordinary compilation and SemIR validation while
rejecting executable source CFG publication. The existing four comparisons and
thirteen regressions remain separately selectable; the unfiltered source suite
contains thirty tests.

Root V4 completed configure, incremental build and CTest with natural exit zero.
All seven selected CTest groups passed: four comparisons, thirteen regressions,
six loops, seven guards, 35 source straight-line cases, 63 comparison metadata
cases and the existing standalone scratch-eligibility fixture. The six counted
groups total 128 cases; the standalone fixture is additional. Metadata checks
recorded 346 passing preconditions and zero precondition failures. No UBSan
diagnostic appeared, and owned input pins remained unchanged.

Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/source-loop-direct-v4/receipt.json`,
SHA256 `844fb85e034261c010f28b2498942b21943c86755ef7be17d3e0ba84a08fbd69`.
CTest log SHA256:
`db897305e7fd6ed04cca4c04ab20386bbb2d1a524cbc60433f2276d22d4e2a6f`.
The real source directory was `E:/Git/zr_vm/tests/cmake/ssa-source-direct-validation`;
the build reused `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
The executed scope is Windows clang-cl 19/MSVC ABI, UNDEBUG, selected-input UBSan
and an 8 MiB PE stack reserve. No source snapshots were created.

V1/V2/V3 failed receipts remain false. V3's six loops and seven guards passed,
but two of its 35 straight-line regressions failed; that aggregate is not GREEN.
The final straight-line fixture now checks real SemIR initialization/store/load,
source pool 7/9 and mappings. Its promoted graph has no LOAD/STORE or PHI, and
the Oracle returns 9 with zero events/stores. The unknown-child LOAD retains
the original INVALID type analysis. These updated contracts are included in
V4's 35-case regression acceptance; scratch eligibility protections remain intact.
Linux, ASan, native32, C/LLVM source consumers, automatic production publication,
full 01.02/01.05 and the 47-leaf milestone remain unestablished.
