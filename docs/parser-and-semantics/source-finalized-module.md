---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/parser/test_ssa_source_finalized_module.c
  - tests/parser/support/ssa_source_finalized_module_fixture.h
  - tests/parser/support/ssa_source_finalized_module_fixture.c
  - tests/parser/support/ssa_source_finalized_module_witness.c
  - tests/parser/support/ssa_source_finalized_module_faults.c
  - tests/cmake/ssa-source-finalized-module.cmake
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - .codex/plans/20261009-ssa-source-finalized-module-design-v3.md
  - .codex/plans/20261009-ssa-source-finalized-module-test-protocol-v1.md
tests:
  - tests/parser/test_ssa_source_finalized_module.c
  - docs/testing-and-validation/source-finalized-module.md
doc_type: module-contract
status: functional-red-verified-commit-pending
---

# Finalized primitive Source module

## Current implementation and evidence boundary

The private RED seam exists in two C translation units and one private header.
It resets all nine diagnostic fields and refuses any required top-level NULL with
INVALID_ARGUMENT/token0. Other calls return UNSUPPORTED with a safely bounded
original selected Function token, or token0 if that header cannot be established.
It performs no allocation/free, query, Verify, clone or publication. NULL
functionConstants is not a top-level NULL guard: its count/storage relation
belongs to the intended GREEN admission.

Actual RED r3 reached all24 cases: required-NULL guard1 PASS and23 exact
feature failures. Each feature refusal preserved all nine fields and input/old
output records with attempts0/frees0; healthy teardown completed without ledger
or UBSan errors. This establishes the RED baseline, not any later mutation,
complete-copy, lifetime-cycle or allocation-sweep acceptance. Current MSVC r3
compiled31 ordered units with zero warnings. Root proof, exact16-path RED commit
and conditional obsolete-object cleanup remain pending.

The following complete contract is the accepted GREEN target, not behavior
supplied by the RED stub. Callable query GREEN is separately committed at227ba1ff;
the v3 plan's earlier query-pending statement is historical. Test-support scope
also includes an additive nonasserting allocator getter: invalid record/output
returns false without changing the output. Numeric observers use it and cleanup
finishes before Unity assertions, including after an intentional feature failure.
The three production RED files have not changed to implement GREEN.

## Private API and caller lifetime

```c
TZrBool source_snapshot_finalized_primitive_module(
    const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context,
    const SZrAstNode *script,
    const SZrFunctionSourceCallableIdentity *sourceIdentity,
    const SZrArray *compilerConstants,
    const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount,
    const SZrExecIrModule *original, const SZrExecIrFunction *finalized,
    SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic);
```

All traversed storage must be genuinely readable, initialized, typed/aligned,
live, stable and serialized. Output is an exclusive writable slot holding NULL
or a genuine unique generic owner. Optional diagnostic and slot must be disjoint
from each other, all reachable inputs and old owner, including untraversed
storage. Arithmetic checks establish numeric geometry, not pointer readability.

The actual Source caller must supply matching still-live scalar bridge,
Build/Binder, compactor and frame producer witnesses from one compilation.
Declared lower fixtures demonstrate field consistency without proving that
history. Names/source/state pointers are borrowed scalar comparisons; no String,
State/global or provider dereference is needed.

Finalized may borrow a distinct old output. That owner stays live through the
last snapshot operation and is released only on successful replacement. Success
ends old borrows. Numeric canonical IDs stored in output do not retain context,
AST, registry, CompilerState or Function constants.

## Storage inventory and geometry order

Input budget is exactly94 spans; internally owned candidate adds at most33,
combined127. Empty NULL allocations add no span. Header slices embedded in larger
Compiler/Function/context/semantic owners are registered once as traversed slices,
not as whole containing owners. The original selected Function header is contained
in module.functions and is not a second allocation.

| Input allocation or slice family | Maximum |
| --- | ---: |
| Root Module/finalized/context/semantic/identity/compiler-array/slot/diagnostic |8|
| Four original Module pools |4|
| Seventeen Function pools for each Function |34|
| Frame/GC/State owner headers for each Function |6|
| Eight nested frame/GC/State pools for each Function |16|
| Canonical/symbol/selected parameter allocations |3|
| Fourteen Semantic typed allocations |14|
| Sole place projections and CFG outgoing edges |2|
| Independent compiler and Function constant pools |2|
| SCRIPT/statements header/pointer pool/RETURN/INTEGER |5|

Typed SZrArray order is isValid, elementSize, length<=capacity, checked
capacity×actual width, pointer/capacity consistency, uintptr end, overlap, register.
Native pool order starts count<=capacity then the same product/pointer/end/overlap
sequence. Headers are registered before nested pointer reads. Positive spare
capacity is checked at its actual extent; content identity covers logical rows,
not padding or spare bytes. AstNodeArray uses actual pointer width and count/capacity.
Function constants have UInt32 count and no capacity, so count is their extent.

Register roots in API order while deferring finalized until the original table
has exactly one readable selected header. Exact finalized==selected immediately
refuses UNSUPPORTED/T before second registration or nested traversal. Partial
header overlap/shared independently owned pools refuse INVALID_RANGE. Preserve
old output; never free it to make validation pass.

Function17 order is values/instructions/blocks/operandPool/resultPool/memoryTokenPool/
phiPool/phiIncoming/predecessors/successors/gcRoots/deoptStates/deoptValues/
deoptAggregates/deoptAggregateFields/sourceMaps/bindingRows. Owners are frameLayout,
gcMap,stateMap; nested pools are slots, GC entries/slotIndexPool/inlineRefOffsetPool,
State entries/valuePool/rootPool/ownerStatePool.

Semantic14 order is places/cfg.blocks/locals/values/instructions/valueOperands/
regions/cleanupScopes/sourceMap/loanFacts/escapeFacts/contiguousViewFacts/boundsFacts/
scalarScratchProofs. Require finite one-place/one-block counts before their nested
arrays. Canonical/symbol scans and query establish active FUNCTION parameters
before their allocation is globally registered; AST shape is established before
following each child.

## Intended first-error and transaction sequence

1. Reset nine fields; required NULL guards precede all other reads. Preflight
   numeric roots/pools and obtain original selected token T.
2. Check current Module/entry contract domains, original unsealed/frame-absent
   profile, then actual Core VerifyModule(original), preserving its complete lower9.
3. Call actual callable query; preserve its lower9. Check identity/state/unique
   symbol/SCRIPT→RETURN→INTEGER origin and bounded Semantic predicates in v3 order.
4. Check original/finalized count agreement and establish complete original
   keyed source-map mapping M. Every mapping failure uses zero locations.
5. Check independent compiler→Function→Module constant count/index/type/tag/bits
   and layout, then logical original/finalized block/instruction/value/slice/map
   transition. Only safely established original role locations may be reported.
6. Actual const frame validator on finalized precedes complete-copy allocation.
7. Initialize candidate; CloneModuleReplacingFunction(original,1,finalized).
   Preflight candidate33 against input94, Verify and recheck its complete identity,
   pools/source relation/constants/layout/frame using actual capacities.
8. CloneOwnedModule(candidate,destination) is last fallible/publication operation.
   It alone releases old output and publishes. Free initialized candidate on
   every path, including success, while preserving the precise diagnostic.

No Function_SnapshotExecIr double clone, original owner transfer, new arena,
ID remapping or registry ownership is introduced. Source clone failure precedes
Core's actual immutable identity comparison table and replacement clone.

## Current domains, origin and constants

Admission requires current execution schema/ABI/logical symbolic versions6/17/1,
reserved fields0 and generation1. Equal unknown versions are not sufficient.
MODULE contract signatureHash is the MODULE blob hash; Function signatureHash
and contract signatureHash are canonical callable hashes. moduleHash is the
independent SCRIPT_ENTRY ABI hash. Target tokens have MODULE and MEMBER_DEF
tables respectively. Module layoutHash stays0; final frame hash is Function-only.

Original has one Function/constant/layout and zero module source maps, entry id1,
PLACE_BASE form, unsealed/frame absent/layoutHash0. Finalized may be sealed.
Expected compacted relation allows only dead PLACE_BASE→NOP and associated dead
address removal; compare live slices rather than reconstructed offsets.

Identity schema1/type/symbol/signature/profile/declaration range must agree with
actual callable facts and SCRIPT. Check nonNULL matching semantic/context/place/
CFG state pointers without reading state. Do not require cfg.semanticContext
or legacy block.isTerminator; actual producer establishes neither condition.

Require one unique FUNCTION symbol, correct type/script/range, one RETURN node
and INTEGER child. Numeric literal AST bits are not a constant producer. Validate
source pointer relations, UInt32 offset representability, positive signed lines/
columns, ordering and containment before lossless comparison/conversion.

Semantic finite profile is instructions3/values1/places1/valueOperands1/CFG1/
regions1 and the specified other arrays empty. Dedicated type/index/source-map/
bound errors cannot be folded into generic UNSUPPORTED. Constants compare live
compiler SZrArray and independent Function pool, then Module bits. Widen UInt32
Function count; reject compiler length>UINT32_MAX before narrowing. Require INT64
tags before union reads; convert signed bits through defined UInt64 conversion.
No SZrTypeValue padding/pointer metadata equality is treated as numeric identity.

## Diagnostic widths and exact locations

All wrapper refusals reset9, set code and established T (or0 before selected
header), and only their prescribed payload. blockId is0 for wrapper errors.
V fields are UInt32 IDs/enums; H fields are UInt64 hashes/wide counts/generation.
The complete v3 table is authoritative; lower query/frame/Core/copy/snapshot
failures retain every field unchanged.

Before completed M, wrapper instruction/source locations are0. After M, only its
actual original implicit instruction index+1 and instruction.sourceId pair is
used; malformed finalized/candidate IDs cannot supply locations. Layout/contract/
global errors retain locations0. Success returns all nine fields zero.

- NULL: INVALID_ARGUMENT/token0.
- Geometry/overlap: INVALID_RANGE; checked overflow/budget: CAPACITY_OVERFLOW.
- Generic finite profile: UNSUPPORTED; original sealed: SEALED.
- Dedicated types/hashes: SIGNATURE_MISMATCH with exact V/H pair.
- Current versions/reserved/schema: VERSION_MISMATCH; generation: STALE_GENERATION.
- MODULE/entry token and target domains: MODULE_MISMATCH/TARGET_MISMATCH.
- Capability/effect/layout predicates retain their corresponding precise codes.
- Map count/bounds: INVALID_RANGE; map identity/range/transition/bits: INVALID_VALUE.
- Actual envelope OOM comes from CloneOwnedModule V1/0/token0, not sizeof guessed
  by this wrapper. No boolean producer failure is relabeled OOM.

## Intended complete diagnostic table

This is the accepted v3 wrapper table for future GREEN, not RED implementation
credit. Established T, V/H widths and completed original mapping M mean exactly
what the previous sections specify. Actual lower diagnostics bypass normalization.
| Predicate family, evaluated in its exact field/row order above | Code / active fields |
| --- | --- |
| Any required top-level NULL | INVALID_ARGUMENT; token0; V/H/locations0 |
| Array invalid/width/count-capacity/null inconsistency; pool shape; span overlap/partial owner alias | INVALID_RANGE; established token; all other fields0 |
| Checked multiply/add/address overflow or94/127 span exhaustion | CAPACITY_OVERFLOW; established token; all other fields0 |
| original.functionCount!=1 | UNSUPPORTED; token0; other fields0 |
| finalized==readable original selected header, before second registration/traversal | UNSUPPORTED; T; other fields0 |
| Other finite counts/rich metadata/AST/CFG/place/region/inert/profile fields fail, excluding every dedicated predicate below | UNSUPPORTED; T; other fields0 |
| original selected id!=1 | INVALID_BLOCK; T; V1/actualID |
| Module token zero/wrong table | MODULE_MISMATCH; T once established; V expected MODULE table/actual table (zero token uses V1/0) |
| Entry token zero/wrong table | TARGET_MISMATCH; T; V expected MEMBER_DEF table/actual table (zero token uses V1/0) |
| module.moduleHash==0 | MODULE_MISMATCH; T; H1/0 |
| version/reserved/sourceIdentity schema field fails | VERSION_MISMATCH; T; V symbolic expected/actual |
| generation field fails | STALE_GENERATION; T; H1 or actual module generation/actual generation |
| module contract target mismatch | MODULE_MISMATCH; T; V actual moduleToken/actual target |
| Function contract target mismatch | TARGET_MISMATCH; T; V actual functionToken/actual target |
| Missing signature hash | SIGNATURE_MISMATCH; T; H1/0 |
| contract moduleHash mismatch | MODULE_MISMATCH; T; H module.moduleHash/actual |
| nonzero capabilities/effects | CAPABILITY_MISMATCH/EFFECT_MISMATCH; T; V0/actual |
| forbidden module/original Function layoutHash | LAYOUT_MISMATCH; T; H0/actual |
| original sealed | SEALED; T; other fields0; precedes present-frame UNSUPPORTED |
| original frame present | UNSUPPORTED; T; other fields0 |
| SourceIdentity type/symbol ID zero or disagreement | SIGNATURE_MISMATCH; T; V actual nonzero semantic ID (or requirement1)/actual identity ID |
| canonicalSignatureHash, original/finalized signatureHash/contract.signatureHash fails | SIGNATURE_MISMATCH; T; H query.signatureHash/actual field |
| SourceIdentity explicit/return/parameter/receiver/effect profile fails | SIGNATURE_MISMATCH; T; V true/false, query returnPrimitive/actual, or0/actual respectively |
| state/symbol uniqueness/kind/ast/range relationship or AST range validity fails | UNSUPPORTED; T; other fields0; excludes symbol type and source-map contradictions |
| symbol canonical type mismatch | SIGNATURE_MISMATCH; T; V semantic.callableTypeId/actual type; locations0 |
| compiler length>UINT32_MAX | INVALID_RANGE; T; H UINT32_MAX/actual length |
| compiler/runtime pool count disagreement | INVALID_RANGE; T; H compiler length/Function count |
| same pool count not equal required1/module count | INVALID_RANGE; T; H1/actual compiler length |
| missing semantic constant index at first CONSTANT role check | INVALID_VALUE; T; V1/0; locations0 (before M) |
| pool index out of actual bound | INVALID_RANGE; T; mapped instruction/source; V actual bound/actual index |
| Semantic instruction/value/facts/place canonical type mismatch | SIGNATURE_MISMATCH; T; V query returnTypeId/actual type; locations0 (before M) |
| original/finalized instruction/value or module constant canonical type mismatch | SIGNATURE_MISMATCH; T; V query returnTypeId/actual type; established original role locations only |
| layout canonical type mismatch | SIGNATURE_MISMATCH; T; V query returnTypeId/actual type; locations0 |
| module constant flags nonzero | UNSUPPORTED; T; mapped constant instruction/source; V0/actual flags |
| compiler or Function pool tag notINT64 | UNSUPPORTED; T; mapped constant instruction/source; V INT64/actual tag |
| runtime or module bits mismatch | INVALID_VALUE; T; mapped constant instruction/source; H compiler bits/actual runtime or module bits |
| layout geometry/hash absent or invalid | LAYOUT_MISMATCH; T; positive-required field uses V1/actual size/align/id; zero hash H1/0; non-power-of-two align V0/actualalign |
| Semantic instruction/source-map count unrepresentable in UInt32 | INVALID_RANGE; T; H UINT32_MAX/actual count; locations0 |
| Semantic/original/finalized source-map count disagreement at first count comparison | INVALID_RANGE; T; V actual instruction count/actual map count; locations0 |
| Bounded instruction/source ID or live operand/result slice bound problem | INVALID_RANGE; T; V actual bound/actual index or count; locations0 before M, established original role pair after M |
| Semantic map instruction/range contradiction, or duplicate original map/source ID or original map range contradiction while establishing M | INVALID_VALUE; T; locations0; V/H0 |
| Later finalized map/logical transition/retained value/slice content/block field contradiction | INVALID_VALUE; T; established original role pair for an instruction/value/map, otherwise locations0; V/H0 |
| candidate constant type/flags/bits differs from original | INVALID_VALUE; T; constant mapped locations; V original type or flags/actual, or H original bits/actual |
| candidate layout field differs from original | LAYOUT_MISMATCH; T; V original/actual for32 fields or H layoutHash pair |
| candidate selected frameHash differs from finalized | LAYOUT_MISMATCH; T; H finalized/candidate |
| Actual query/frame/Core copy/Verify/snapshot failure | Preserve all actual lower9 unchanged |

## Intended logical transition and candidate recheck

Original source-map count equals instruction count before its profile gate;
original values3 become finalized values1 while instruction/block counts remain3/1.
Original blocks require entry id1/start0/count3/terminator3 and no live phi,
predecessor/successor/effect/memory phi content. Compare every finalized block
field in declaration order, preserving actual immediateDominator rather than
inventing one. Keyed maps may reorder storage; each instruction's bounded nonzero
sourceId and unique map index+1 must cover all three actual Semantic roles.
Complete M before constant pool reads; check all eight map fields losslessly.

CONSTANT remains CONSTANT with no operands/one actual retained result, and its
layoutId is the actual constant index. RETURN retains one operand/no results/
layoutId0. Sole original PLACE_BASE consumes provenance and defines address;
finalized NOP has no live operand/result slices. Check every range against actual
pool count before reading; regenerated slice start offsets need not be equal.

Retained value has actual id/definition/typeToken/ownership/nullability/flags.
Admit canonical type before logical equality. Original address and provenance
IDs are checked additions from actual Semantic value/place counts, with exact
producer type/flags/definition. Address has no live uses; provenance occurs once
only in PLACE_BASE. Spare/unreferenced operands are not live uses.

Rich metadata order is GC header/count/roots, memory tokens, phi/incoming, deopt
state/value/aggregate/fields, binding rows/schemaLEGACY. Present-empty StateMap
must retain header presence and Function token/signature/generation. Finalized
sealed is allowed; original sealed precedes frame-present refusal.

Candidate repeats the finalized relation, not original PLACE_BASE/frame-absence
profile. Constants compare original typeToken/flags/bits in order; layout compares
id/typeToken/byteSize/byteAlign/layoutHash. Candidate selected frame hash must
agree with finalized before actual frame revalidation. Module logical source maps
remain0. Clone capacity normalization is preserved and pointers are independent.
## Open scope

Declared consistency is not Binder/interner provenance, authenticated hashes,
durable registry lifetime, target ABI or execution admission. Genuine Source
preparation, mode4 adoption, ordinary/default compilation, Linux/full MSVC/backend
parity and all47 remain OPEN. Existing NONE/AOT/Source modes are unchanged by
this private pending seam.

- [Validation protocol](../testing-and-validation/source-finalized-module.md)
- [Evidence record](../acceptance/source-finalized-module.md)
