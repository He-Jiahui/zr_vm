---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - tests/parser/ssa_source_execbc_vm_compare.inc
implementation_files: []
plan_sources:
  - .codex/plans/20261004-ssa-typed-source-comparison-design.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
doc_type: implementation-design
status: implemented-static-freeze-native-green-pending
---

# LT/GT source producer: coherent lowering and typed boundary

## Gate and scope

This is finite read-only preparation. Root must first observe four source comparison REDs after ordinary parser/statement compilation and ordinary preSemanticIr validation pass. No production code, headers, tests, CMake, native commands or Git commands are changed/run by this design task. Preserve all thirteen existing source regressions, including unsupported foreach/cleanup/continue behavior. Implement only pure signed-i64 LT/GT with bool result; six legacy opcodes do not imply six source/AOT predicates or a general bool ABI.

## Actual reusable seams

| Existing symbol / location | Contract to reuse |
| --- | --- |
| compiler_internal.h:175 compiler_semantic_ir_find_slot | Maps compiler stack slot to semantic valueId/typeId/placeId; copy IDs before any mutating call that can grow slot arrays |
| compiler_internal.h:178 compiler_semantic_ir_slot_value | Returns the source semantic ValueId bound to a compiler slot |
| compiler_internal.h:180 compiler_semantic_ir_emit | Uses existing ZrParser_SemanticIr_Emit and invalidates preSemanticIrValidated; no separate source-map append API |
| compiler_internal.h:183 compiler_semantic_ir_bind_result_value | Reuses temporary place/slot registration and PLACE_BASE producer; existing same-type slot is updated; new slot may grow arrays |
| semantic_ir.h:418 ZrParser_SemanticIr_AddValue | Adds result with canonical type identity and source range; no fabricated operand value IDs |
| semantic.c:349 ZrParser_Semantic_RegisterInferredType | Registers semantic type record from canonical FromInferred ID; return is actual canonical identity, not runtime enum |
| canonical_type.h:454 ZrParser_CanonicalType_InternPrimitive | Obtain primitive canonical BOOL/INT64 IDs; compare against Find node kind/valueType rather than treating numeric IDs as runtime types |
| semantic_ir_value_facts.c:269 ResolveValueFacts | Atomically refreshes value ownership/nullability witnesses from canonical type graph; facts alone do not encode primitive runtime kind |
| compile_expression.c:704 lower_binary call | Actual source operator decision is already made, operands compiled and destination chosen; insert a small compare lowering call here before arithmetic path |

The arithmetic helper at compiler_semantic_ir.c:2112 rejects different operand/result type at 2151. Comparison must use a separate coherent helper, not weaken that arithmetic check. Current file counts are compiler_semantic_ir.c 2573, compile_expression.c 2823, exec_ir_build.c 999, semantic_ir.c 970, core exec_ir_verify.c 906 and aot_ir.c 1385. These are current observations, subject to concurrent Root edits. Add small call sites only and new responsibility TUs.

## Canonical SemIR representation

Append `ZR_SEMANTIC_IR_COMPARE` immediately before ENUM_MAX, after DIV. This preserves every existing opcode numeric value; ENUM_MAX increases. Do not insert near arithmetic in the middle. Existing named selectors are currently parser-private `ZR_EXEC_IR_COMPARE_KIND_LESS=1` and `ZR_EXEC_IR_COMPARE_KIND_GREATER=3` in exec_ir_execbc_vm_internal.h:20-21; no named Core predicate enum was found. Core's named COMPARE opcode is declared in exec_ir_opcode.def:26 with one result/two operands, but Oracle dispatch still uses raw selector cases. Promote those exact two existing names/values into a shared Core selector header or exec_ir.h, rather than inventing a conflicting encoding. A new SemIR predicate INVALID must use an unambiguous sentinel (UINT32_MAX); zero is historical EQ in Core, not a safe shared invalid selector. Document this as an operation selector, not a canonical type token. Unknown selectors must not reach default-equality fallback.

Append explicit `comparisonPredicate` and `comparisonOperandTypeId` metadata to instruction and instruction-spec structs. Do not reuse symbolId/layoutId/scalarConversionTypeToken/matchTypeId. Struct size grows and all in-tree producers must rebuild; do not claim binary layout compatibility for external users merely because existing opcode values remain stable. Add emission copy/formatting/validation alongside these fields.

For a source comparison: instruction.typeId=result canonical BOOL; resultValueId identifies an AddValue(BOOL) result; operands are exactly the existing left/right semantic ValueIds in source evaluation order; both have actual canonical INT64 type; comparisonOperandTypeId is that canonical INT64 identity; comparisonPredicate is explicit LT or GT; matchTypeId=0; all unrelated metadata=0/invalid. A named source operator mapper supplies the predicate. Canonical type IDs are never decoded as predicates.

SemIR Emit/Validate presently permit matchTypeId only for TYPE_TEST (semantic_ir.c:592-594 and :827-829). Keep those rules unchanged. Add a compare-specific validator for opcode/arity/result-definition/operand IDs, operand type equality, operand metadata equality, result type equality, and zero comparison metadata on unrelated opcodes. Include COMPARE in result-bearing opcode classification at :101-104. Add a canonical-type validation helper using context for actual primitive INT64 and BOOL nodes; call it in compiler ValidatePreSemanticIr after ResolveValueFacts. Core pointer-free structural verification can prove equality of type IDs but cannot derive their primitive meaning from opaque IDs.

Suggested new coherent APIs (proposals, not existing declarations):

```c
/* compiler/compiler_semantic_compare.h; implementation in .c */
TZrBool compiler_semantic_compare_lower(
    SZrCompilerState *cs, const SZrAstNode *sourceNode,
    EZrInstructionCode selectedOpcode,
    TZrUInt32 leftSlot, TZrUInt32 rightSlot, TZrUInt32 resultSlot,
    const SZrInferredType *resultType, SZrFileRange range);
TZrBool compiler_semantic_compare_source_supported(const SZrAstNode *node);

/* semantic_ir_compare.c; canonical check receives actual context */
TZrBool ZrParser_SemanticIr_ValidateComparisonsWithCanonicalTypes(
    const SZrSemanticIrFunction *function,
    const SZrSemanticContext *context);
```

Lowering first checks the actual sourceNode with compiler_semantic_compare_source_supported: it must be ZR_AST_BINARY_EXPRESSION, operator exactly `<` or `>`, and both child nodes exactly ZR_AST_INTEGER_LITERAL. It then accepts only the matching selected LOGICAL_LESS_SIGNED / LOGICAL_GREATER_SIGNED opcode, verifies source operand canonical nodes are INT64 and resultType registers to BOOL, copies value IDs/type IDs before binding/appending, adds result, binds result slot using existing helper, emits COMPARE using the existing append/source-map producer, then emits the selected legacy opcode for the named differential route. compile_expression.c:704 passes its actual binary AST node into the new helper. An opcode-only helper would accidentally admit canonical local comparisons outside this finite literal scope and is not acceptable. It must never create substitute operand values or reconstruct SemIR by reading emitted bytecode.

Distinguish not-applicable from failure at the call site. In an admitted canonical comparison, failed append/type proof must set compiler error and stop; it must not pass through the existing generic `if (!lower_binary) emit legacy` behavior. A small enum result NOT_APPLICABLE/LOWERED/FAILED is preferable if boolean cannot express this. All allocated inferred types still follow compile_expression.c's existing cleanup. Partial append failure prevents publication; do not promise OOM rollback merely because current Array_Push APIs return void. Validate lengths/IDs at relevant append sites and test actual injected failure before claiming retry-safe mutation.

## Source CFG admission and builder mapping

Admission starts with exactly `<`/`>` binary expressions whose children are integer literals for the four fixtures. The actual if entry is compiler_semantic_cfg_begin_if at compiler_semantic_cfg.c:370. It obtains conditionValue from the compiler slot at :432 and requires a valid defining instruction at :434-439; it does **not** call compiler_semantic_cfg_loop_condition_is_supported. The :484 call is in begin_while, not begin_if. Therefore the required seam is literal-gated comparison source emission before begin_if is called: once the real bool comparison result is bound to conditionSlot, begin_if's existing defined-value condition gate can start the CFG. Do not change the while/foreach condition admission as a substitute for this if producer.

At begin_if, optionally add a small compare-specific witness check when the condition's definition is new COMPARE: require the literal source helper accepts node->data.ifExpression.condition and predicate/source-range/result identity matches the real definition. Leave existing boolean/identifier/short-circuit condition behavior unchanged. This explicit witness prevents future bypass of the producer's finite scope. Do not broaden expression_is_linear to every binary, recursive call, assignment or float comparison, and do not add comparisons to the loop admission for this four-if fixture slice. A later separate fixture may admit local signed loads or loop comparisons only after reviewing their source-load facts and initialization ordering.

Straight-line source scanner at compiler_semantic_cfg_finalize.c:211 maps only +,-,*. Add compare recognition through the new helper; validate a same-source COMPARE exists with the expected predicate, two real operands and one bool result. Extend complete-value preflight (:97) with canonical comparison proof. Keep lexical worklist traversal and evaluation order; no recursive scanner or old-bytecode fallback. The branch operand must be the comparison resultValueId returned by the existing slot lookup, with sourceRange from the if/condition producer. Existing foreach admission is untouched.

Map SemIR COMPARE to ExecIR COMPARE in the existing switch (exec_ir_build.c:74). Add new `exec_ir_build_compare.c/.h` for opcode-specific checks and mapping so exec_ir_build.c does not cross 1000 lines by accumulating a second responsibility. It returns the shared named LT/GT selector (promoted from the existing private names) in x.typeToken, with canonical result bool in the result value record; x.matchTypeToken remains zero. Do not overwrite the result Value type with the selector. Preserve current ValueId mapping (:608-613), sourceId=in.id (:707), source map append/remap (:925), CFG edge construction, effects synthesis and all-level verification/SSA. This generic builder checks relational identities; actual primitive type proof additionally requires the canonical validation stage described above. Do not assert that generic BuildModule can distinguish bool/i64 from opaque type IDs without context.

BuildModule still owns function/module identity, generation, signature/contract and commit-on-success. The four test fixture identities remain fixture-only. Canonical checks must precede builder publication, and wrong predicate/operand/result metadata must give deterministic source/function/block/instruction diagnostics rather than repairing unknown facts.

## Shared verifier, Oracle and typed ExecBC boundary

Shared ExecIR verifier at exec_ir_verify.c:565-567 allows nonzero matchTypeToken only TYPE_TEST. Shared AOT verifier at aot_ir.c:732-734 does the same. Keep both rules intact for this slice. Enforce LT/GT at the source/SemIR/typed comparison boundary. Generic Core has historical EQ/LE/GE/NE Oracle cases; do not globally reject those while adding a narrow source producer. A generic selector-domain check should reject values outside its existing recognized domain rather than silently treating unknown selectors as EQ. Existing compare result is bool in Oracle at exec_ir_interpreter.c:459, but it currently switches bare selector values. Replace LT/GT use sites with shared named constants without shifting existing cases. Oracle RunOracleEx calls zr_oracle_input_valid at exec_ir_interpreter_run.c:180, so audit that actual entry check rather than assume full Core verification occurs there. The source harness already explicitly runs all-level verification before Oracle. Preserve signed relational comparison implementation at interpreter.c:119-138: it compares values rather than subtracting them, avoiding extreme-value subtraction overflow.

Malformed type gates are intentionally layered:

| Corruption | Required rejection |
| --- | --- |
| Source predicate unsupported or selector outside domain | Source canonical admission/lowering rejects; generic Oracle must not default unknown selector to EQ |
| Operand ID missing/out of bounds, bad result ID/count/definition | SemIR Emit/Validate and shared opcode/SSA range checks reject before dereference |
| Operand type IDs differ or do not match comparisonOperandTypeId | SemIR compare validator and builder relational check reject with source position |
| Both opaque operand IDs consistently point to FLOAT/BOOL instead of INT64 | Canonical context check rejects actual primitive kind; generic pointer-free Core equality checks alone cannot prove this |
| Result opaque canonical ID is consistently INT64 rather than BOOL | Canonical check and typed VM boundary reject, even if generic SSA is structurally valid |
| Compare shared match nonzero | Existing Core/AOT MATCH0 checks reject; do not relax them |
| VM matched type BOOL, operand runtime types not INT64, result not BOOL | Existing compare VM validator rejects (:302-306) |
| Branch condition points at i64 instead of comparison bool | Typed branch validator rejects (:718); source canonical def-use gate rejects earlier |

Core COMPARE's schema proves two operands/one result and def-use/SSA invariants, not primitive interpretation of arbitrary canonical IDs. Record which layer rejected each fault; passing generic verification must not be reported as a complete typed proof.

VM compare validator (exec_ir_execbc_vm_validate.c:302-306) requires matchTypeToken=runtime INT64, INT64 operands, BOOL result. **Setting compare matchTypeToken to BOOL is incorrect.** Bool belongs to the result value; conditional branch validator at :718 checks its operand is bool independently. Shared match=0 and VM matched operand type are different contracts.

Introduce a coherent `exec_ir_execbc_compare_types.c/.h` boundary helper using the actual existing API inputs: the original **projection**, canonical context, and owned staged instruction array. WithCanonicalTypes has no original shared-function parameter. No API extension is required: derive canonical metadata via the existing valueSlots/slotValues mapping. For each operand/result ValueId, check nonzero/id<=valueSlotCount, map physicalSlot=valueSlots[id-1], check physicalSlot<physicalSlotCount, then require slotValues[physicalSlot].id==id and read its canonical typeToken. Validate pool ranges before reading IDs and actual canonical nodes before trusting primitive kind; use original projection arrays since the owned value copy may already be normalized.

Confirm two canonical operand types are the same actual INT64 node, one canonical result type is BOOL, and selector is named LT/GT. Source shared compare matchTypeToken remains zero. The public canonical VM API also accepts an explicit canonical matched type: after proving the actual operand/result nodes, allow incoming matchTypeToken=0 or the proven canonical operand INT64 TypeId only; reject UNKNOWN, BOOL and a different canonical ID. Annotate only stagedInstructions[index].matchTypeToken with the **canonical operand INT64 TypeId**, deriving zero or preserving the proven explicit identity, before the generic canonical resolver at :118. Do **not** write runtime ZR_VALUE_TYPE_INT64 before this resolver: runtime enums are not canonical IDs and would be looked up as the wrong canonical node. A clean implementation performs this annotation before the copied-values/instructions normalization loops. The existing resolver then maps the canonical matched type to runtime INT64, which the low-level VM validator requires.

Thus source-built shared projection remains MATCH0, the staged pre-resolution view carries canonical matched operand identity, and the final owned VM view carries runtime matched INT64. Existing resolver checks canonical primitive kinds and maps BOOL/INT64 to runtime enums (:10-23); compare selector typeToken remains untouched at :130. Bool is result metadata, not the matched operand token. Preserve source maps, effects, CFG and constant IDs. No input shared projection/function mutation; failure frees staged arrays and leaves output empty.

This placement avoids accidentally annotating AOT's prepared projection, because LowerAotWithConstantsAndLayouts internally calls the generic BuildProjectionWithConstantsAndLayouts (:55), then transfers its pools. Source Oracle and AOT keep original shared match=0 while the VM has derived matched type. Never weaken MATCH0 shared checks to accommodate a VM-only field.

## Future same-source AOT handoff: narrow i64 return, bool internal

Actual API `ZrParser_ExecIr_LowerAotWithCanonicalCallable` already exists (exec_ir_lower_aot.c:175). It checks real canonical callable structuralHash against function/contract signature, effect flags, no parameters/receiver/effects, INT64 return and frame parameter counts. Preserve that noargs-i64 ABI; internal bool compare does not require bool callable-return ABI. Generic lowering at :65 still produces ABI_UNKNOWN and runnable=false and cannot alone prove an executable source AOT route.

Future handoff should call that existing canonical-callable API on the same source-built function and source constants, retaining canonical bool in projection.slotValues. A new narrow scalar metadata validator proves compare operands/results and branch bool def-use against actual slot/value mapping before descriptor conversion. Descriptor conversion at exec_ir_aot_projection_descriptor.c:179 copies shared match=0. Do not strip a VM view's match token to make invalid provenance look shared, and do not set a bool ABI token merely because a condition is bool.

Existing backend_aot_ir_scalar_conditional.c plain():29 requires match=0, accepts only selector 1/3 at :110, and hardcodes 3 blocks/8 instructions/4 constants at :45-49. Real source literal scratch places may add PLACE_BASE/INITIALIZE/LOAD or pruning artifacts, and source maps may carry metadata this helper rejects. Before adapting C/LLVM, inspect actual Green source IR dump and prove scratch-place elimination or support the real shape through existing eligibility/promotion; do not construct an 8-instruction replacement array. Emitters consume one validated conditional plan derived from actual producer CFG/value/constant pools. Source C and LLVM coverage remain not established until that path and actual emitted-code checks execute.

The source fixture also lacks real canonical callable/script signature production and frame metadata. Fixture token/hash constants cannot satisfy LowerAotWithCanonicalCallable honestly. Canonical source callable identity/frame producer is a distinct prerequisite to this AOT handoff, not an emitter workaround.

## Owned files and explicit build closure once authorized

New source responsibilities: compiler/compiler_semantic_compare.c/.h; semantic_ir_compare.c; exec_ir/exec_ir_build_compare.c/.h; exec_ir/exec_ir_execbc_compare_types.c/.h. Existing headers add minimal declarations/metadata; source call sites stay small. Source ABI enum is append-only; shared ExecIR/AOT instruction layouts need no predicate field growth for the existing selector contract. Update functional producer/projection docs with code-module-docs-maintenance.

Normal parser module uses zr_declare_module and source discovery; direct validation driver uses GLOB_RECURSE CONFIGURE_DEPENDS (tests/cmake/ssa-source-direct-validation/CMakeLists.txt:106) and requires a new configure/provenance collection for every new TU, not stale 666-TU claims. Driver test at :163 compiles actual existing source TU and registered comparison/regression selections at :185/:187; manifest includes all .inc fixtures at :223-225.

Standalone explicit `_zr_vm_ssa_builder_sources` in tests/cmake/ssa-builder-tests.cmake:3 must add exec_ir_build_compare.c and any new shared schema verifier TU linked by its imported core sources. Other explicit projection source lists in tests/cmake/ssa-tests.cmake:452/:509/:551 and ssa-sccp-conversion-tests.cmake:32 require any helper called from projection code. If VM-only annotation is called solely by canonical_types TU, list the new compare_types TU wherever that canonical TU is explicit. Use rg on actual list after final seam choice; do not rely on normal module globs to satisfy focused fixtures.

Mandatory native evidence after Root authorization: four compare tests pass ordinary validation, prove actual compare/bool type identity and both branch selections, all thirteen original tests stay unchanged/passing; malformed predicate/type/result/branch must fail precisely; typed VM input is unchanged on failure. Then separately record AOT same-source prerequisites and execution; no synthetic emitter success counts as source coverage.

## Separate remaining compiler publication work

Comparison source Green only completes the manually orchestrated fixture route. Script normal compiler currently validates at compiler.c:1021 then publishes module/submission at :1160/:1174. Child function isolation ends at compiler_function.c:463 and frees child SemIR through compiler_semantic_ir.c:1437. Later automatic publication must build/verify/retain child artifacts before that lifetime boundary and script artifact before module/submission handoff, derive real canonical callable/module identity, and own it beyond CompilerState_Free. Implement a separate publication TU/retention contract and normal public Source_Compile RED. This task does not turn an observer callback or BuildModule fixture into automatic production publication.

Local primary reference semantics were read from lua/src/lvm.c:499/:1703 and lua/CPython/Python/bytecodes.c:3033: typed integer comparisons produce boolean decisions. Only their separation of operand/result kinds informs this design; no foreign opcode or compiler code is copied.

## Independent design review incorporated

source_compare_direct_recipe_1850 identified and this revision resolves three concrete issues: begin_if's true entry/defined-value gate replaces the mistaken begin_while admission seam; the lower helper now accepts the actual AST and enforces literal-only LT/GT scope; the VM annotation uses existing projection ValueId-to-slot metadata and canonical TypeId before normalization, without assuming an unavailable shared-function input. These are design corrections only. Root's actual native four-case CFG RED plus thirteen regression evidence remains the prerequisite to production authorization.

## Authorized implementation and static freeze

Root subsequently established the actual V46 four-case CFG RED and authorized production implementation. The original immutable log was read and hashed: `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/source-direct-red-v46/comparisons-red.log`, SHA256 `242c2ec3f05978da9d1a27463c08dc297e302e525352bbc16af3e7efdaaeb13f`, four tests/four CFG failures/zero ignored after both prerequisite gates passed. Root separately investigates the old thirteen-case stack overflow. No native/compiler/Git action was run by this agent.

The actual implementation uses three new compiled TUs, not the proposed separate semantic_ir_compare.c: compiler_semantic_compare.c owns canonical proof, semantic_ir_compare_internal.h contains reusable relational inline checks, exec_ir_build_compare.c owns the relocated existing opcode mapping and comparison lowering metadata, and exec_ir_execbc_compare_types.c owns staged canonical VM matched-type annotation. The only explicit builder pool was updated in tests/cmake/ssa-builder-tests.cmake. No explicit canonical_types.c CMake source list was found under tests/scripts, so its helper is discovered through the normal parser module and direct source driver globs. All nineteen frozen paths below exist. Source fixtures/test harness/driver were untouched.

Freeze SHA256 values (before independent implementation review; later repair must refresh affected rows):

| Path | SHA256 | Lines |
| --- | --- | --- |
| zr_vm_core/include/zr_vm_core/exec_ir.h | 60107384b566d3b9b237c87b745d355ca75d0537535de9ae68bc9138541c00f2 | 731 |
| zr_vm_parser/include/zr_vm_parser/semantic_ir.h | 95bac0d359a07686afe3021e294c56a49d1b63094686edbb805f2535d581b7f8 | 554 |
| zr_vm_parser/src/zr_vm_parser/semantic_ir.c | 29ac8017c12601fdd820d70a0e4e1974961880d3d6baf584d9ad643e5d7e46c1 | 979 |
| zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c | 32ea7c3b737ad200e05bf336bc685eb5fb7d809c3259300baa02e08e977f0ea5 | 149 |
| zr_vm_parser/src/zr_vm_parser/semantic_ir_compare_internal.h | f2b165c43a78f4fe8ce220b3ab283ac57fb7351ad0eeed7b1ba98ccbf838d979 | 57 |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c | 0df49128156c328d849ec037426db4a770f0f34342dc16d4257ecf4e8905240c | 120 |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.h | bc7b75928b316986e0ad39b8c75e0ceeadad7fce9ec314a2cd8d641b2b92745c | 22 |
| zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c | 0944e8720bc7df115c5b2a0d953ffa744de10e82c1460ffc4265d7e2cc143afa | 2829 |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c | 18ee944006ec794aed1dd4a116feb60d656490a386e64470c6c529f236432b28 | 2575 |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c | 98ff9874d06e509135ee53bad77b2ebf204e5baa2114a2d604d763a918a21118 | 1117 |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c | 01f0806d850c166a70a2ed7fd85b7266641023d219aaa2f425bb6f4f92104d3e | 766 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c | b772493d45929f0656f5f9bee248354a33d900e8c9b88d5b7b89167257ed465d | 945 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c | 94198992de07f5871af57f4b05068798a919e0805caac4c1bdeb23f44f9da07d | 89 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.h | 4a351da70a3bab8c8f47dba517ecb195345060bb406a4db274db24d9cd82c226 | 14 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.c | b782c4f2473ebac75d87cfff9e1f14fd787e25adb873e6e5153ed6d401acf16c | 59 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.h | 79e6f40358dab4cc4aaf1c8befe0cde8238f0720d345416e51246c2c4869bcd3 | 11 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c | 9c748addcadf11f1ae1210238907c84db3eb444ea2ad39f01157aec8b360e251 | 197 |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_internal.h | c6ec0ce70a739493fe4a532c0410faa13cb1cd237bde9c6163340d8d6f81d009 | 63 |
| tests/cmake/ssa-builder-tests.cmake | d21fd605b1810de10af45621f5615aec70acab20f818df05ff4b64a3c4f5ced6 | 182 |

Detailed actual module contracts and pending Green boundaries are documented in docs/instruction-generation/source-signed-literal-comparison.md. Root's next configured actual source pool must include the three new TUs and refresh discovery/provenance; old 666-TU receipts remain historical. Core predicate-domain RED/guard is owned by sccp_phi_copy_impl_1805, using the frozen shared named selector header.

### Public canonical VM matched-type review repair

Independent implementation review checked exec_ir_execbc_vm.h:36-42 and the preexisting canonical resolver's handling of nonzero matched type IDs. Root authorized preserving that public boundary contract: after proving the actual same canonical INT64 operands and BOOL result, incoming comparison matchTypeToken may be zero or that exact canonical operand TypeId. Zero derives the staged ID; a valid explicit ID is preserved; unknown, BOOL and different canonical IDs reject. The borrowed projection is unchanged. Shared Core/AOT MATCH0 and source VerifyALL remain unchanged, and the existing successful explicit-match fixture is preserved.

Only exec_ir_execbc_compare_types.c and these two owned documents changed in this repair. The refreshed production row is above; all other eighteen frozen production/header/registration hashes remain unchanged. Module document docs/instruction-generation/source-signed-literal-comparison.md is 171 lines, SHA256 3a63e96f057f452f2497777c8deaf2a67020375f951e285ce2a87b1345483849. Root owns native validation and a separate metadata fixture agent covers absent, valid explicit and malformed match tokens; no native/Git actions were run here.
