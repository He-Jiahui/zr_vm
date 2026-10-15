---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_parser/CMakeLists.txt
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
tests:
  - tests/parser/test_ssa_contract_freeze.c
doc_type: testing-guide
---

# SSA execution contract freeze

## Scope and declaration owners

This 2026-10-15 source audit records the version owners and the pure contract
checker's current boundaries. It does not close plan 00.02: the complete
interface-family and persistent-field ownership dictionary, hash input rules,
and compatibility matrix remain open.

The current Core API declarations of `SZrExecutionContract`, the nine-field
`SZrExecIrDiagnostic`, their status/code enums, and the two
`ZrCore_ExecutionContract_*` entry points are in
[`execution_contract.h`](../../zr_vm_core/include/zr_vm_core/execution_contract.h).
The checker and status-name implementation belong to
[`runtime/execution_contract.c`](../../zr_vm_core/src/zr_vm_core/runtime/execution_contract.c).
No parallel diagnostic declaration is needed in a plan or consumer.
The records contain scalar identities and masks, with no AST, runtime pointer,
or host address. The lifecycle mapping of persisted generation to a process
load epoch still requires a separate ownership decision.

## Version owners and consumers

The table identifies the canonical owners used by the inspected top-level
workspace targets. Five of these macros have one definition in the repository
C/H search; `ZR_VM_AOT_ABI_VERSION` has a second, historical definition listed
below. Equal values across domains do not establish format compatibility or
transfer ownership.

| Domain / macro | Current value and type | Definition owner | Observed consumer / boundary |
| --- | --- | --- | --- |
| CallBinding / `ZR_CALL_BINDING_SCHEMA_VERSION` | 1, `TZrUInt32` | [`call_binding.h`](../../zr_vm_core/include/zr_vm_core/call_binding.h) | Its persistent binding contract and fixed-width encoding API; runtime targets are separate |
| Artifact / `ZR_ARTIFACT_SCHEMA_VERSION` | 6, `TZrUInt16` | [`artifact_schema.h`](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | ZRS/ZRI/ZRO wire-format schema; not the C execution-contract record layout |
| AOT / `ZR_VM_AOT_ABI_VERSION` | 17, unsigned integer literal | [`zr_aot_abi.h`](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | Common AOT registration ABI; not CallBinding's schema |
| Execution / `ZR_EXECUTION_CONTRACT_SCHEMA_VERSION` | 6, `TZrUInt32` | `execution_contract.h` | ExecIR function construction writes `contract.schemaVersion` |
| Execution / `ZR_EXECUTION_CONTRACT_ABI_VERSION` | 17, `TZrUInt32` | `execution_contract.h` | ExecIR function construction writes `contract.abiVersion` |
| Execution / `ZR_EXECUTION_CONTRACT_LOGICAL_VERSION` | 1, `TZrUInt32` | `execution_contract.h` | ExecIR function construction writes `contract.logicalVersion` |

### Historical AOT header and unresolved repository-wide uniqueness

[`zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h`](../../zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h)
also defines `ZR_VM_AOT_ABI_VERSION`, as **2u**. It is tracked by this repository
(index blob `8acdfc01e1ccbdb08669776d21c3ec3441d10498`) and was unchanged from HEAD
at this audit; it is not an untracked generated header.
[`zr_vm_aot/README.md`](../../zr_vm_aot/README.md) explicitly calls it a historical
ABI reference and identifies the top-level common header as the production ABI.

The inspected module include paths in
[`CommonMacros.cmake`](../../zr_vm_common/CommonMacros.cmake), the parser AOTIR
interface in [`zr_vm_parser/CMakeLists.txt`](../../zr_vm_parser/CMakeLists.txt),
and the Source direct-validation include list in
[`CMakeLists.txt`](../../tests/cmake/ssa-source-direct-validation/CMakeLists.txt)
use the top-level `zr_vm_common/include`, not the nested reference directory.
The current parser backend sources live under
`zr_vm_parser/src/zr_vm_parser/backend_aot`; the nested header's existence does
not mean the active AOT backend is absent or uses ABI 2.

These observations establish the inspected targets' canonical include route,
not repository-wide uniqueness or an inventory of every external/manual
consumer. Both trees expose the same include-relative header name and macro;
selecting the historical include root can select ABI 2 instead. The complete
consumer inventory and ownership policy for that collision remain open under
00.02. This documentation neither merges, deletes, nor synchronizes either
header, and does not authorize changing the historical reference.

The execution-field writes occur in `ZrCore_ExecIr_ModuleAddFunction` in
[`exec_ir.c`](../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c); the checker
consumes the supplied fields without choosing or upgrading a version.
`test_ssa_contract_freeze.c` currently asserts CallBinding 1, artifact 6, AOT 17,
and execution schema/ABI 6/17; it does not assert the logical-version macro.

The earlier description of artifact 5 / AOT 16 with execution 6/17 as a
candidate describes the prior state. The current headers and existing test
use the values above. This update does not prove that any particular old
artifact is readable, authorize another version bump, or merge independent
version domains. The execution header's candidate wording alone is not a
reader compatibility rule.

## Pure checker policy matrix

`ZrCore_ExecutionContract_Check` clears a supplied diagnostic first and follows
the order below. The first applicable failure wins. It performs no pointer
resolution, hash recomputation, allocation, artifact loading, or publication.

| Order / input | Current result | Diagnostic payload beyond the code |
| --- | --- | --- |
| 1. Either contract is NULL | `INVALID_ARGUMENT` | Other fields remain zero |
| 2. Schema, ABI, or logical version differs | `RECOMPILE_REQUIRED` | Schema pair in `expectedVersion/actualVersion`; ABI pair in `expectedHash/actualHash` |
| 3. Target token differs | `TARGET_MISMATCH` | Token pair in the version fields |
| 4. Generation differs | `STALE_GENERATION` | Generation pair in the hash fields |
| 5. Signature hash differs | `SIGNATURE_MISMATCH` | Signature pair in the hash fields |
| 6. Layout hash differs | `LAYOUT_MISMATCH` | Layout pair in the hash fields |
| 7. Module hash differs | `MODULE_MISMATCH` | Module pair in the hash fields |
| 8. Capability masks differ, or either has unknown bits | `CAPABILITY_MISMATCH` | Mask pair in the version fields |
| 9. Effect masks differ, or either has unknown bits | `EFFECT_MISMATCH` | Mask pair in the version fields |
| 10. All preceding checks pass | `OK` | Entire supplied diagnostic remains zero |

Results use the `ZR_EXECUTION_CONTRACT_` prefix. For non-NULL contract failures,
`functionToken` is the expected target token; block/instruction/source IDs stay
zero. The public header owns the precise diagnostic enum codes and field types.
A NULL diagnostic is permitted. Capability and effect checks compare masks;
they do not infer capability authorization from effects or validate a host
allowlist.

### Decisions still open

- **Equal unsupported versions:** the checker tests equality, not membership
  in supported versions. Equal arbitrary schema/ABI/logical versions can pass
  when the other checks pass. The admission owner and rejection policy remain
  to be assigned; `OK` here is not artifact compatibility acceptance.
- **Logical-version diagnostics:** a logical-only mismatch is rejected, but
  its expected/actual logical values are not represented by this failure
  payload. The reporting policy remains open.
- **Reserved fields:** `reserved0` and `reserved1` are not checked. Whether
  they must be zero, and which boundary enforces that, remains open.
- **Generation lifecycle and hash authority:** equality checks do not establish
  a persistent-to-runtime generation mapping, a unique producer for every
  hash, or canonical hash inputs. Those parts of 00.02 remain open.
- **Historical AOT name collision:** canonical ABI 17 and the tracked historical
  ABI 2 header coexist. The include-route evidence above does not close the
  repository-wide owner and consumer boundary.

## Existing focused test and evidence limits

[`test_ssa_contract_freeze.c`](../../tests/parser/test_ssa_contract_freeze.c)
contains these focused checks:

| Test function | Assertions currently present |
| --- | --- |
| `test_identical_contract_is_accepted` | `OK` and cleared diagnostic code |
| `test_signature_and_layout_mismatch_are_structured` | Signature/layout status; expected/actual signature hashes |
| `test_version_and_generation_fail_before_identity_resolution` | Schema/generation status; expected/actual schema versions |
| `test_capability_is_not_inferred_from_effects` | Added capability and added effect are classified separately |

The existing CMake target `zr_vm_ssa_contract_freeze_test` compiles only this
test and `runtime/execution_contract.c`; CTest registers `ssa_contract_freeze`
in [`ssa-tests.cmake`](../../tests/cmake/ssa-tests.cmake). Its function names do
not supply a resolver-order trace: the checker itself contains no resolver.
The test does not cover every branch, complete nine-field diagnostics,
multiple simultaneous mismatches, or the open policies above.

This documentation change was checked against current declarations,
implementation, test source, and registration using read-only searches and
Markdown diff checks. No compilation or test execution was performed for this
update, and no runtime, security, network, FFI, or whole-plan acceptance is
claimed.
