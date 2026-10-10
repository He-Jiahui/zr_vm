---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/place_queries.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.h
  - tests/cmake/ssa-retained-builder-cfg.cmake
  - tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/semantic_value_facts.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
implementation_files:
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.h
  - tests/cmake/ssa-retained-builder-cfg.cmake
  - tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - "user: 2026-10-10 finite retained builder CFG gate; execution reserved for Root review"
tests:
  - tests/parser/test_ssa_retained_builder_cfg.c
  - docs/acceptance/ssa-retained-builder-cfg.md
doc_type: module-detail
status: finite-green
---

# Retained builder CFG fixtures

The finite eight-case gate passed on 2026-10-10 using the actual Windows
clang-cl build with UBSan. A separate MSVC receipt covers compilation of the
same 31 C translation units only. These receipts establish the bounded
structural and retained-storage checks below; broader architecture gates remain
open. See the [accepted evidence](../acceptance/ssa-retained-builder-cfg.md).

## Purpose and ownership boundary

This finite fixture joins the actual SemIR-to-ExecIR builder, opaque Core module
owner and Core verifier. It supplies reusable retained value/effect graphs for
later scalar optimization and observer work. It does not execute a language
program or an ExecIR interpreter.

`ZrTest_RetainedCfgProduce` owns one heap allocation per input function. Each
allocation contains the complete SemIR function and all nested array views:
four blocks, four typed edges, ten or twelve instructions, four or six values,
operand IDs, one place and its matching local. The two-function case allocates
two independent producers.

For each producer, the helper appends two real module constants and assigns their
indexes to SemIR CONSTANT instructions. It calls actual
`ZrParser_ExecIr_BuildModule`, which builds the function and publishes the module
function ID, final token, signature and generation. It then calls actual
`ZrCore_ExecIr_CloneOwnedModule`.

On every helper exit, the transient module is freed and every allocated SemIR
producer is overwritten through a volatile byte view and freed. Only an opaque
Core owner can leave the successful helper. The runner obtains its borrowed
view after the helper has returned and calls `VerifyModule` and
`ValidateStateMap`. It releases every mutable negative-test clone and the owner
before printing observations.

The owner API is a storage copy, not semantic admission. The retained verifier
checks are therefore separate required gates. No runtime Function is fabricated;
real Function association is a separate lifecycle gate.

## Independently specified inputs

Private canonical type identities 10 and 11 label BOOL and I64 fixture facts.
They are explicit test identities, not registered runtime types or aliases.
Ownership and nullability facts are explicitly UNKNOWN. A type-registry or
source-language type-resolution acceptance claim is outside this fixture.

| Graph | Declared blocks and definitions | Expected builder output |
| --- | --- | --- |
| Promoted diamond | B1 branches on external BOOL v1 to B2/B3; I64 constants v2=10 and v3=20 are stored; B4 loads v4 and returns it | Address/provenance v5/v6; B4 phi v7 has ordered incoming (B2,v2),(B3,v3); stores4/7 are NOP; load9 is COPY v7 |
| Promoted loop | B1 stores I64 v1=0 then enters B2; B2 loads v2 and branches on external BOOL v6; B3 adds v2+v3=1 into v4, stores and backedges; B4 loads v5 and returns it | Address/provenance v7/v8; B2 phi v9 has ordered incoming (B1,v1),(B3,v4); stores3/9 are NOP; loads5/11 are COPY v9 |
| Parameter diamond | Same diamond, with both local.isParameter=true and PARAMETER place base | Stores and load remain; B4 heap/effect phis join the outputs from stores4/7 |
| Parameter loop | Same loop and true parameter place facts | Stores and loads remain; B2 heap/effect phis join entry store3 and latch store9; both loads consume the header heap phi |

The entry block's immediate dominator is itself (B1), following the current real
builder convention. Diamond remaining dominators are B1; loop B2 dominates B3
and B4. Source maps retain instruction IDs and deliberately distinct offsets,
lines and columns. Pool constants carry actual I64 bits 10/20 or 0/1; arithmetic
is represented and checked structurally, not executed.

## State-map inventory

The promoted graphs have present-empty maps. Parameter graphs have exactly six
entries: BEFORE_EFFECT, AFTER_EFFECT and CLEANUP_COMPLETE at each STORE. Boundary
and exception flags are THROW, handler/deopt IDs are zero, and cleanup states
are 0/1/2.

The diamond live sets are {v2,v5} or {v3,v5} before each respective store and
{v5} afterward, producing eight stored live-value/owner-state entries in total.
The loop has {v1,v6,v7} or {v4,v6,v7} before the respective stores and {v6,v7}
afterward, producing fourteen entries. UNKNOWN ownership produces no roots.
All map identities equal the final function token/signature/generation1.

The checks use these declared CFG/dataflow facts, not builder output copied into
an expected graph. Token-number assertions express producer/consumer relations
and region tags rather than duplicating incidental allocation numbering.

## Remaining architecture gates

This contributes bounded evidence to 01.01–01.04 through the eight passing cases. Source-produced
semantic lowering, exceptional/cleanup/suspend graphs, complete clone/OOM
denominators, cross-function remapping and optimization differential execution
remain open. The verifier and state-map validator allocate temporary analyses;
this gate supplies no zero-allocation observer evidence. M1, Source, platform
and all47 acceptance remain incomplete. Complete OOM and leak denominators,
full Linux and full MSVC runtime validation, and normal/default project
configuration and execution remain open. Shared registration is tracked by Root.

See [validation procedure](../testing-and-validation/ssa-retained-builder-cfg.md)
and [finite receipt](../acceptance/ssa-retained-builder-cfg.md).
