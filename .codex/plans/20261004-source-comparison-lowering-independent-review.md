---
doc_type: independent-implementation-review
status: static-reviewed-native-green-pending
reviewer: source_compare_direct_recipe_1850
date: 2026-10-04
reviewed_freeze: .codex/plans/20261004-source-comparison-producer-lowering-design.md
production_mutations: none
native_execution: none
---

# Source literal LT/GT lowering: independent frozen-source review

## Scope and evidence

Reviewed the approved producer/lowering design, all three new C translation units and four new internal headers, and the complete read-only tracked diff for the author's twelve existing source/header/registration paths. Root expressly permitted scoped read-only Git diff with `GIT_OPTIONAL_LOCKS=0`; no Git mutation, source copy, diff artifact, native execution, CMake configure or production/fixture edit was performed. Only this review metadata is written by this reviewer.

Independently computed SHA256 for all **19** frozen source/header/CMake paths in the design's `Authorized implementation and static freeze` table. Every hash matched. The three new compiled inputs are:

| Input | Independently matched SHA256 |
| --- | --- |
| compiler/compiler_semantic_compare.c | 0df49128156c328d849ec037426db4a770f0f34342dc16d4257ecf4e8905240c |
| exec_ir/exec_ir_build_compare.c | 94198992de07f5871af57f4b05068798a919e0805caac4c1bdeb23f44f9da07d |
| exec_ir/exec_ir_execbc_compare_types.c | b782c4f2473ebac75d87cfff9e1f14fd787e25adb873e6e5153ed6d401acf16c |

The source/header/CMake freeze table itself remains the authoritative full path/hash record. The new module document was also read. The separate Core selector-domain guard is Root-owned and is outside this review's change approval.

## Verdict

No static production defect was found in the intended path from the four real source literal LT/GT if cases to canonical SemIR, shared ExecIR and the typed VM adapter. The producer is suitable for Root's actual compile/link/Green attempt against the frozen current checkout. This is a static review, not a claim of compilation or runtime success.

The original freeze had one concrete **public canonical VM compatibility blocker** outside the four source cases and thirteen source regression cases. The Root-authorized dual-MATCH repair is now independently read back and hash-verified; that static P1 is resolved. Native focused acceptance remains pending. The already passed direct driver with nine tests/138 TUs does not contain this canonical VM fixture target.

## Finding: preserve the public canonical VM operand annotation contract

**P1 — tests/parser/test_ssa_execbc_vm_canonical_types.inc:81 and exec_ir/exec_ir_execbc_compare_types.c:37.**

`run_canonical_vm_case` creates a shared COMPARE and then assigns its `matchTypeToken = intId` before `ZrParser_ExecIr_BuildProjectionWithConstants`. The ordinary success case, run explicitly by `test_ssa_execbc_vm.c:953`, expects `report.fixtureBuilt` and `report.materialized` to be true (`test_ssa_execbc_vm_canonical_types.inc:276–280`). The new canonical helper rejects any incoming COMPARE with nonzero `matchTypeToken`, so that success case reaches an INVALID_VALUE failure if the projection builder succeeds.

This cannot be dismissed as a fixture that was already guaranteed to stop at projection construction: `exec_ir_projection_common.c:97` validates binding rows, pools and CFG and `:627` calls that validator, but it does not run the shared Core instruction MATCH-schema check. `:918` copies the nonzero MATCH unchanged. The preexisting canonical adapter resolves that token, and the direct VM validator requires runtime INT64 MATCH at `exec_ir_execbc_vm_validate.c:302`. Thus this was a previously admitted canonical-materializer fixture input even though shared Core verification rejects it.

The approved design deliberately preserves the existing shared rule in `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:564`: only TYPE_TEST has nonzero shared MATCH. The source builder also preserves that rule in `exec_ir_build.c:671`. The new canonical adapter's owned staging annotation is the right place for COMPARE operand-type MATCH.

Further API/documentation review establishes that requiring incoming MATCH0 at this public boundary is a regression. `zr_vm_parser/include/zr_vm_parser/exec_ir_execbc_vm.h:36–42` accepts canonical instruction/value/constant types and keeps the comparison selector distinct; it does not impose incoming MATCH0. `docs/parser-and-semantics/execbc-vm-canonical-types.md` explicitly states that COMPARE's matchTypeToken is resolved as a type and records existing native canonical adapter acceptance. The previous implementation also resolves any nonzero canonical match before direct VM validation. Therefore the existing successful canonical fixture must be retained.

**Root-authorized resolution:** after proving the two operands have the same canonical primitive INT64 TypeId and the result is canonical primitive BOOL, accept incoming MATCH0 or incoming MATCH equal to that proven operand TypeId. Reject unknown, BOOL, qualified/narrow and different canonical match identities. Write the proven canonical operand TypeId only into the owned staged instruction, then let the ordinary resolver convert it to runtime INT64. Keep the existing canonical success fixture. Root's malformed-boundary fixture owner will cover both legal forms, bad matches, empty failure output and input immutability.

This dual-form public VM contract does not weaken the shared Core/AOT schema: the actual source route still reaches Core VerifyALL with MATCH0, and no shared function/projection is modified by the adapter. Equality is checked against an ID already proven through the owning context, so accepting the existing annotation cannot select a different primitive or override the real operand type. A numeric token that happens to equal a runtime enum value is interpreted only as a canonical ID at this entry, as required by the public API.

The reviewer's original recommendation to converge the existing successful fixture to MATCH0 was withdrawn after the API/documentation evidence above. No fixture or production source was edited by this reviewer.

**Repair verification:** the updated helper places `(in->matchTypeToken != 0u && in->matchTypeToken != leftType)` after both operand canonical proofs, the result BOOL proof and equality of the two operand canonical IDs. It still writes only `staged[index].matchTypeToken`. All 19 current production/header/registration freeze rows were recomputed again: 19 matches, zero mismatches. The other eighteen rows retain their original hashes. Refreshed helper SHA256 is `b782c4f2473ebac75d87cfff9e1f14fd787e25adb873e6e5153ed6d401acf16c`; module-doc SHA256 is `3a63e96f057f452f2497777c8deaf2a67020375f951e285ce2a87b1345483849`; design/freeze-plan SHA256 is `1ec8a3ff83a5bc35a525da7fb93db7e0e643a00cc112741b05ee9ce6d56c1e0d`. Both document hashes match the author's refreshed freeze message and describe the dual-form VM boundary while preserving shared MATCH0. No static production blocker remains in this review's scope.

## Checked producer and if admission

- `compiler_semantic_compare_source_supported` accepts only binary ASTs whose two child nodes are exactly INTEGER_LITERAL and whose operator is `<` or `>`. Unary signs, locals, calls, conversions and other operators do not obtain this producer's comparison SemIR.
- The lowerer also requires the compiler-selected signed LT/GT legacy opcode, equal real semantic operand type IDs and a canonical primitive INT64 operand node. It copies both operand ValueIds and their canonical TypeId before registration or result binding can grow arrays.
- The inferred result is registered and independently required to be primitive BOOL. A real bool SemanticIR value is added and bound to the compiler result slot. The instruction carries that bool `typeId`/`resultValueId`; the two original operand IDs and their explicit canonical i64 identity remain separate.
- The actual `compiler_semantic_cfg_begin_if` defined-condition seam calls `compiler_semantic_compare_condition_valid` at `compiler_semantic_cfg.c:431`. For a COMPARE definition it proves the original literal AST, same predicate, bool result/value identity and matching source offsets. Non-COMPARE conditions retain the previous path. The existing while condition helper was not changed.
- The whole-source straight-line scanner recognizes only that same literal comparison shape and requires a same-source instruction with two operands, a result and the same predicate (`compiler_semantic_cfg_finalize.c:218–249`). Whole canonical primitive checks are also required before an executable CFG and before ordinary preSemanticIr validation completes.
- FAILURE does not call arithmetic lowering or emit the caller's legacy replacement: the call site only falls back for NOT_APPLICABLE (`compile_expression.c:705–717`). FAILED reports `ZrParser_Compiler_Error`; compiler diagnostics preserve `hadRecoverableError` (`compiler_diagnostics.c:683`), and script compilation restores overall failure before publication (`compiler.c:810`). Successfully lowered comparisons retain the selected legacy instruction for the existing differential compiler route. No retry-safe/OOM rollback claim is made for void Array_Push support.

## Checked representation and Build/Core separation

- COMPARE is appended after DIV in the SemIR enum. Every old opcode keeps its numeric value; ENUM_MAX grows. The opcode formatter appends its corresponding name.
- The two metadata fields are appended to instruction/spec structures after sourceRange. Existing members retain their ordering. Structure sizes grow; neither the implementation nor this review promises external binary structure-size compatibility. Core instruction layout and legacy runtime instruction codes are unchanged by these feature edits.
- The new shared Core selector enum names the historical 0..5 domain. The former parser-private LESS=1/GREATER=3 duplicate is removed. The source producer/typed VM slice admits only 1 and 3. Generic Core predicate guard changes are a separate Root gate.
- SemIR emission and validation require a real result with matching result type, exactly two real operands with the declared equal operand type, no result self-use, distinct operand/result type identities and selector 1 or 3. Unrelated opcodes require both appended comparison metadata fields to be zero. Existing result-definition/source-map checks still run.
- Primitive meaning is proved with the real compiler canonical context. The builder's relational helper intentionally cannot infer primitive BOOL/i64 meaning from arbitrary opaque TypeIds without that context.
- The old opcode map was moved intact into `exec_ir_build_compare.c`; comparison mapping was appended and no prior mapping case was removed. The builder sets the new instruction selector from explicit comparison metadata after its normal type assignment, while result value metadata remains bool. Existing shared MATCH checking remains immediately afterward. A helper rejection frees the output function and returns false.

## Checked canonical-to-runtime VM boundary

- The canonical materializer first creates owned instruction/value/constant arrays, then calls the new helper before ordinary canonical resolution. The original projection/context remain borrowed.
- Each operand/result ValueId must be within valueSlotCount, map through valueSlots[id-1] to a bounded physical slot, and find a slotValues record with the same ValueId and a nonzero canonical type. The corresponding canonical node must be primitive INT64 for both operands and BOOL for the result. Operand canonical identities must equal.
- That lookup matches actual projection construction: `exec_ir_projection_common.c:1023–1029` rejects duplicate value occupancy and writes each original value record to its mapped physical slot. This slice does not assume arbitrary slot coalescing.
- Operand/result pool ranges are checked before reading IDs. The repaired public API accepts only absent MATCH or the exact proven operand canonical identity. Unsupported predicates and primitive/type/map/match mismatches fail before output publication.
- Only the owned staged comparison receives its actual **canonical** operand TypeId as MATCH. The ordinary resolver then turns that annotation into runtime INT64. The selector is excluded from canonical `typeToken` resolution. Bool result records are independently resolved to runtime BOOL; direct VM validation requires two runtime INT64 operands, BOOL result and runtime INT64 annotation. No runtime INT64 is incorrectly passed into canonical lookup.
- Existing canonical materializer cleanup frees temporary arrays on failure and initializes output empty. The source fixture calls this actual canonical entry and then the real runtime execution helper; this review does not replace those arrays/functions or execute any provider path.

## Checked CMake/source closure

`CommonMacros.cmake:9,58` and `tests/cmake/ssa-source-direct-validation/CMakeLists.txt:101` discover real module C sources recursively with CONFIGURE_DEPENDS. A fresh configure sees all three new TUs. The only discovered explicit CMake source list for `exec_ir_build.c` is `tests/cmake/ssa-builder-tests.cmake:4`, and the freeze adds `exec_ir_build_compare.c` immediately after it. Read-only searches under tests/scripts found no explicit list for `exec_ir_execbc_vm_canonical_types.c`; those tests link the parser module.

Any manual archive receipt or previously configured fixed object pool must be refreshed for the three new compile inputs and the changed headers. The previous 666-TU receipt is evidence for the old inputs, not a build receipt for this implementation. A later direct list containing the canonical materializer TU must include its compare-types helper too.

## Exact remaining acceptance gates

1. Root's fresh native compile/link against the frozen current sources, including all three new TUs and affected headers.
2. Actual `--comparisons-only`: four tests, zero failures/ignored, exit0, through source executable CFG, Core VerifyALL, Oracle and typed runtime stages; preserve the immutable V46 four-failure RED evidence separately.
3. Actual `--regressions-only`: thirteen tests, zero failures/ignored, exit0, with Root's confirmed stack/link configuration and the preserved unsupported-source outcomes. No sealed old objects may be used as evidence for changed feature inputs.
4. Retain and accept the existing canonical success coverage plus the new legal-zero/legal-explicit/bad-match boundary fixtures, including unchanged borrowed inputs and empty failure output. The public MATCH repair's static freeze is independently verified above; native status belongs to Root's actual receipts.

Source C/LLVM publication and automatic callable publication are separate remaining milestones; this review does not establish them or claim whole-milestone completion.
