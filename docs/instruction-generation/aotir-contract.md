---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/aot_ir_projection_descriptor.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
tests:
  - tests/parser/test_ssa_aotir_contract.c
  - tests/parser/test_ssa_aot_callable_abi.c
  - tests/parser/test_ssa_aot_canonical_abi.c
  - tests/parser/test_ssa_aot_projection_branch_descriptor.c
  - tests/acceptance/ssa-aotir-logical-map-schema.md
  - tests/acceptance/ssa-aotir-explicit-callable-abi.md
  - tests/acceptance/ssa-aotir-canonical-callable-abi.md
  - tests/acceptance/ssa-aotir-branch-descriptor.md
doc_type: module-detail
status: implemented-subset
---

# Shared AOTIR contract

`zr_vm_core/include/zr_vm_core/aot_ir.h` defines the shared ABI projection
consumed by C and LLVM AOT backends.  AOTIR is not a second language IR: its
opcode, effect, CFG, state-map, signature, and frame identities originate in
ExecIR.  Backend-specific code may legalize alignment and calling convention,
but may not infer ownership, calls, or exceptions from quickened bytecode.
The current legacy emitter dependencies and the gap between this descriptor
and the owned ExecIR projection are inventoried in
`docs/instruction-generation/aot-emitter-consumer-matrix.md`. This contract
does not yet drive the archived C/LLVM artifact writers.
The target contract requires a nonzero target-triple hash and ABI hash in
addition to the supported ABI version, pointer size, and endianness; its
required capability mask may contain only known execution capabilities.
The module and function execution contracts likewise reject unknown capability
and effect bits before any backend lowering begins.

Both execution contracts require `reserved0` and `reserved1` to be zero. A
shared file-local contract check preserves schema, ABI, then logical-version
validation before checking `reserved0`, then `reserved1`. Schema and ABI
failures keep `ZR_AOT_IR_VERSION_MISMATCH`; logical-version and reserved-field
failures use `ZR_AOT_IR_INVALID_CONTRACT`. A reserved-field diagnostic reports
`expected = 0` and the original nonzero word as `actual`. Module diagnostics
use function ID and index zero; function diagnostics retain the function ID
and its module index. Block and instruction IDs remain zero for these checks,
and a null diagnostic pointer preserves the returned status.

This requirement belongs to the module/function AOTIR validation boundary.
The separate pure `ZrCore_ExecutionContract_Check` still leaves reserved-field
admission and equal unsupported-version admission open. Its existing logical
diagnostic repair remains a separate, finite result. See
[`SSA execution contract freeze`](../testing-and-validation/ssa-contract-freeze.md)
and the source plan
[`00.02 contract freeze`](../plans/ssa/00-measurement-contracts/02-contract-freeze.md).

Validation never normalizes or clears a caller's reserved fields.
`ZrCore_AotIr_HashModule` validates before hashing and returns zero for a
nonzero reserved word. Valid zero-reserved inputs retain the existing hash
algorithm and field sequence; reserved words are not added to the hash.
Contextual signature, layout, target-token and module-hash checks stay with
their existing module/function validation callers.

Schema version 4 replaces the old `{resumeId, instructionId, stateHash}`
checkpoint summary with a borrowed view of the complete ExecIR logical
state map. A function without checkpoints may leave `logicalStateMap` null.
With a map, the function token, signature hash and generation must match the
function contract; the caller keeps map entries and all side pools alive for
the lifetime of the AOTIR view. The per-function owned projection can supply
that lifetime, but no producer connects it to this module yet. The content
hash includes checkpoint phases, effect/handler/cleanup metadata and live,
root and owner-state pools, not capacities, pointers or a caller-supplied
summary hash. No physical native frame restoration is implied.

The function instruction view also carries typed `typeToken` and
`matchTypeToken` identities plus bounded `memoryIn`/`memoryOut` ranges into a
borrowed memory-token pool. TYPE_TEST must carry a nonzero match type token;
other instructions must carry zero. Memory ranges and every pool token are
validated before hashing, so C/LLVM consumers cannot silently lose memory
ordering or type-test semantics.

Schema version 7 adds `SZrAotIrFunction.callableAbi`. `UNKNOWN` (zero) is the
default for existing descriptor-only producers. The first fixed declaration,
`NOARGS_I64`, says that a trusted callable-signature producer has established
zero parameters and an i64 return; it also names the nonzero AOTIR type token
for that i64 result. The owned `SZrAotIrProjection` and descriptor builder
copy this declaration unchanged, and the backend adapter exposes the same
qualification result. The current source `ZrParser_ExecIr_LowerAot` has no
trusted primitive return-signature producer and explicitly leaves this field
`UNKNOWN`. Neither `typeToken`, opaque `signatureHash`, constant bits, legacy
ExecBC, nor `frameLayout.parameterPrefixBytes` establishes parameter count or
the i64 ABI. ExecIR's `frameLayout.parameterCount` is not carried by this
AOTIR record and does not establish the primitive return kind. The explicit
opt-in `ZrParser_ExecIr_LowerAotWithCanonicalCallable` path below is the only
source-to-AOTIR producer that currently qualifies this narrow ABI.

`ZrParser_ExecIr_LowerAotWithCanonicalCallable` accepts a semantic context and
canonical callable type ID, then requires the canonical function node's
structural hash to match both the ExecIR signature and execution-contract
signature. It currently accepts only an effect-free function with no receiver,
no parameters, and canonical primitive i64 return type; its frame must also
have zero parameter counts, and every ExecIR RETURN value must carry that same
return type token. It first lowers into a private candidate and publishes
`NOARGS_I64` only after the existing ExecIR/AOT projection checks and these
canonical checks succeed. Rejected calls preserve the caller's existing output.
The legacy lowering APIs continue to publish `UNKNOWN`.

The canonical node's `structuralHash` is used only to bind this producer to the
canonical function node in the supplied context. Its current implementation
mixes child `TZrTypeId` values, so this bridge does not claim cross-context or
persistent signature-hash stability. Qualifying the ABI also leaves
`runnable` false: this is an AOTIR projection seam, not an executable native
artifact or a claim that a C/LLVM emitter is available.

`ZrCore_AotIr_ValidateModule` rejects unknown ABI enum values, a nonzero
return token on `UNKNOWN`, a zero return token on `NOARGS_I64`, and RETURN
instructions whose arity or type token disagrees with the declaration.
`ZrCore_AotIr_RequireExecutableAbi` additionally requires a validated,
relocation-free module, a known declaration, and a RETURN value with exactly
one matching typed definition across the whole function, earlier in instruction
order than that RETURN. It returns `UNSUPPORTED` for `UNKNOWN` or a missing
RETURN and clears its output on failure. This qualifies ABI/SSA shape; it does
not prove CFG dominance, authenticate a source-signature producer, or show that
an emitter can produce an artifact. The C/LLVM artifact entry points remain
unavailable. The canonical module hash includes ABI kind and return type token,
so descriptor identity changes when this declaration changes.

Frame legalization is represented by borrowed frame-slot records and a
logical-value-to-physical-slot pool. Source spans are borrowed as numeric
source maps and must resolve to the instruction/source identity they describe;
their offsets and one-based line/column endpoints are validated and hashed.
This keeps ABI layout and source diagnostics available to both consumers
without persisting host pointers.

The public records contain pointers only as in-memory views over caller-owned
arrays.  Semantic references are numeric IDs and bounded ranges, so the
canonical `ZrCore_AotIr_HashModule` ignores host addresses and is stable for
identical input.  `ZrCore_AotIr_ValidateModule` checks schema/execution
contract versions, target ABI, IDs, ranges, opcode bounds, CFG block instruction
partition, last-in-block terminator opcode and edge-target membership,
state-map storage shape, function identity, instruction/source membership,
bounded live/root/owner pools and phase-aware resume identity, effect pairing,
typed match-token and memory-token identities/ranges,
frame layout, phi incoming cardinality and ordered
predecessor-edge membership in the containing block's predecessor edge range,
nonzero state-map resume identity, and
module/function hash identity. Effect tokens must be present together or both
be absent. A
non-PHI instruction carrying phi incoming rows is rejected.
Module and function contract identity failures report the specific canonical
field and received value, so a signature, layout, target-token, module-hash, or
version mismatch cannot be misdiagnosed as a different contract field.
Block flags are likewise limited to the canonical entry/cold/cleanup/exception
bits. Storage slots may not outnumber logical slots, the parameter prefix must
fit before the return area, and frame byte size must cover the return area and
be a multiple of the declared frame alignment. Operand and result pools may not
contain the invalid zero value ID. Every function must publish exactly one entry block. Paired
effect tokens must advance strictly (`effectOut > effectIn`). Instruction flags
must include the dynamic effects required by their opcode schema (allocation,
throw, GC, or suspend); unknown flags and missing required effects are rejected
before lowering. Any opcode with a required dynamic effect must also publish a
nonzero paired effect-token range, and successive effectful instructions in a
block must consume the previous instruction's effect output.

Block predecessor and successor ranges are views over the same numeric edge
pool; their bounds use that pool's count, so parallel edges remain representable
without treating the block count as an edge-capacity limit. Each source/target
edge occurrence must have a matching predecessor occurrence in the target
block.

The owned ExecIR AOT projection keeps successors and predecessors in separate
arrays. `ZrParser_AotIrProjection_BuildDescriptor` validates their original
range bounds, then copies successors followed by predecessors into one
descriptor-owned AOTIR edge pool. It leaves instruction and block successor
ranges on the first segment and rebases nonempty block predecessor ranges onto
the second segment. `ZrParser_AotIrProjection_FreeDescriptor` frees that pool
through `function.successorPool`; no public descriptor or persisted schema
field is added. Overflow, malformed source ranges, and invalid edge targets
reject the candidate while an existing descriptor remains intact.

`ZrCore_AotIr_IsRelocationFree` rejects module or function relocation rows.
Unimplemented operation families should be reported by a lowering diagnostic;
they must not silently fall back to semantic decoding of `SZrInstruction`.
Emitter contract hashes include the target, strict-floating mode, and both
runtime-bridge and interpreter-fallback policies, so changing a permitted
degradation path cannot reuse an artifact identity from another policy.
The lowering result separately publishes a target-independent `loweringHash`;
C and LLVM results must agree on it while their target-specific contract hashes
may differ.

The focused fixture is
`tests/parser/test_ssa_aotir_contract.c`.  It exercises deterministic hashing,
contract validation (including multiple phases per resume ID, deep map
contents, malformed ranges and mismatched identities), and
relocation rejection. The reserved-field fixture covers all eight combinations
of module/function ownership, `reserved0`/`reserved1`, and values `1`/`UINT32_MAX`.
It checks every diagnostic field, null-diagnostic behavior, version-check
priority, invalid hash zero, input preservation, repeated rejection and
recovery to the original valid hash. These cases use deterministic
`fprintf(stderr)` plus `exit(EXIT_FAILURE)` expectations while retaining the
pre-existing assertions.

The fixture uses one function at module index zero. Propagation of a nonzero
function index and precedence when both reserved words are nonzero have only
been statically reviewed; neither is credited as an executed regression case.

### Finite reserved-word validation record

The original standalone RED invocation compiled the three real translation
units below with Clang-cl and MSVC. It performed six successful, warning-free
compiles, two successful links with empty streams, and two executable runs.
Both runs exited 1 with empty stdout and exactly 48 stderr bytes:
`FAIL: AOTIR reserved validation status differs\r\n`. Each stderr range has
SHA256 `02BA6701CDD6922DA293F95765F154420FEF2C036C3CE1A91AF10D1510C547B1`.
Each run stopped at the first reserved-status failure, so this RED result
does not establish execution of the remaining fixture checks.

| RED physical input | SHA256 |
| --- | --- |
| `tests/parser/test_ssa_aotir_contract.c` | `CAF31847C9DE31DEFF2ECC0163EC558188B3C11802921B872A533769A0462CD7` |
| `zr_vm_core/src/zr_vm_core/aot/aot_ir.c` before the repair | `89047DDDC5338EA41C63425FA9445051F07AB1664A00AD94457BB01E6E028BC4` |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c` | `E89BD5C701C51B6CC61F0366D45636AF6C4C8A0CC082848BD8074CFAC3F6F057` |

The receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/aotir-reserved-red-receipt.json`,
SHA256 `16C494AFD093CE2E6E194903A91491D62F1F7F6D7FDD8C92DE18A3A137B5EB7D`.
It finished all 14 recorded phases, then failed the original HEAD/diff guard;
the controller naturally exited 1. Its `validated`, `functional_pass` and
`red_failure_accepted` fields are all false. The error is
`HEAD or selected-path diff changed during validation`.

HEAD moved from `8809939b34f8106c5cc08d4ba20bea12a1ab5795` to
`b321725320b95556d0762ba7a39411b6d2ec23b3`, while the selected tracked diff
bytes were identical. Independent review checked the contiguous raw log
ranges and their hashes, all 214 input pins, eight output pins, and the actual
199-header dependency union with no drift or missing input. Each compile's
`showIncludes` set matched its recorded dependency list and the freshly pinned
allowed header baseline. Independent review accepts the two child failures as
the pre-fix defect reproduction. The overall invocation remains failed and
its receipt is preserved with the original false flags.

The independently reviewed helper revision has SHA256
`06272B45165ECEE3FA74CF5A44C0DFDE3489DB48483D01FA64A6FC96FA58BB28`.
It changes only the final Git comparison policy: final Git output format is
validated, and the after-HEAD, after-diff hash and actual comparison booleans
are recorded as provenance. Source identity remains bound by repeated physical
SHA256 checks of source, test, tools, actual `showIncludes` dependencies and
published outputs. Its dependency closure, exact 14 phases/eight outputs/20
registered paths, warning/output rejection, encoding and RED/GREEN stream
protocol gates remain unchanged. This revision does not change the historical
RED receipt.

### GREEN: accepted finite reserved-word repair

The repaired `aot_ir.c` had SHA256
`AEC33CD3304BB67FF796EEF3C2D7DE482C8C846915DD57D53A031237CD2F4E1A`.
The test and storage translation units retained the RED hashes above. The
GREEN invocation ran from `2026-10-16T04:41:32.5987955Z` to
`2026-10-16T05:06:01.7498359Z`. Its receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/aotir-reserved-green-receipt.json`,
SHA256 `931C57FA57DB197E5257791C145F03116A075667D4EC9C69C1824FF1F8943023`.
It records `validated = true`, `functional_pass = true`,
`red_failure_accepted = false` and a null error.

All 14 phases completed naturally with both streams drained. Six real C11
compiles exited 0 with zero warnings or unknown output; two links and two
executable runs each exited 0 with both streams empty. Controller 17712
(native handle 39899, terminal chunk `6a6502`) naturally exited 0 and reported
`GREEN pure AOTIR reserved protocol accepted; functional_pass=True\r\n`.
The controller message is separate from the empty executable streams.

Independent review checked all 28 contiguous phase ranges and the complete
raw logs. Aggregate stdout was 103346 bytes, SHA256
`5986A3DDBF7B45950D7D4AB7CD0C83B51F9E81BED59B845F7EA098BC2C1152BC`;
aggregate stderr was empty, SHA256
`E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855`.
The 214 input pins, including compiler/linker tools and language resources,
and eight published output pins matched the physical files without drift,
missing files or reparse paths. Each Clang-cl compile listed 183 dependencies
and each MSVC compile listed 78; all six decoded `showIncludes` sets matched
their declared lists. Their 199-header union matched both the actual input
pins and the fresh baseline hashes. The 2396-path fresh baseline exactly
matched the permitted subset of the authenticated 3084-path universe.
Native temporary storage was empty after completion.

The raw before/after HEAD was
`2e5ec42f6db4769e57d6049f758aad1a7e0ad1a3`; the selected tracked diff SHA256
was `11EDA8424F65BD0CCD2B0BCDB8D03C56AB0D36969ADC332943B9F0723B8C59B5`
at both ends. Independent comparison confirmed that `head_unchanged`,
`selected_tracked_diff_unchanged` and `before_after_same` were all actually
true. The run's Parent pin remained
`9FDA62C83D1B129EA015B3A3DE4DD32CF38E3D4DB6C0F0D010094B8B3DF422E8`
through this review. These two documentation files were not validation inputs.

This accepts the fixture's eight reserved-word combinations and the diagnostic,
invalid-hash, input-preservation and recovery checks described above. Nonzero
function indices and two simultaneously nonzero reserved words retain their
static-review-only status. No CTest, Linux, Source, Global, VM, backend artifact
execution, network, FFI or full-project validation is credited. Plan 00.02,
whole-leaf acceptance and acceptance of the overall 47-leaf plan remain open.

CMake registers the fixture as `ssa_aotir_contract`. The following command is
an example route; CTest was not invoked in this finite standalone validation:

```text
ctest --test-dir E:/cargo-targets/zr_vm/build/ssa-gcc-debug -R '^ssa_aotir_contract$' \
  --output-on-failure --no-tests=error
```

`tests/parser/test_ssa_aot_callable_abi.c` exercises the explicit declaration,
unknown and mismatched fail-closed cases, hash sensitivity, owned projection
transfer, and backend adapter qualification. CMake registers it as
`ssa_aot_callable_abi`.

`tests/parser/test_ssa_aot_canonical_abi.c` checks the opt-in same-context
canonical callable producer, its accepted noargs-i64 shape, legacy `UNKNOWN`
behavior, and transactional rejection for invalid/mismatched type, effect,
receiver, parameter, return, and ExecIR inputs. Its scope does not include
cross-context hash identity or runnable native code. See
`tests/acceptance/ssa-aotir-canonical-callable-abi.md` for the recorded RED,
GREEN, and adjacent CTest results.

`tests/parser/test_ssa_aot_projection_branch_descriptor.c` builds verified
two-block ExecIR, lowers it with the legacy `UNKNOWN` ABI, and checks the
descriptor's owned edge pool, reversed predecessor range, module validation,
malformed-edge rejection, and transactional replacement. CMake registers it as
`ssa_aot_projection_branch_descriptor`.
