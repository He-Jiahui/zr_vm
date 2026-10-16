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
| 2a. Schema or ABI version differs, including a simultaneous logical mismatch | `RECOMPILE_REQUIRED` | Legacy payload: schema pair in `expectedVersion/actualVersion`; ABI pair in `expectedHash/actualHash` |
| 2b. Schema and ABI match, but logical version differs | `RECOMPILE_REQUIRED` | Logical pair in `expectedVersion/actualVersion`; both hash fields zero |
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

Both version branches use `VERSION_MISMATCH`. The logical-only branch reports
all nine diagnostic fields: code, expected function token, logical version
pair, zero block/instruction/source IDs, and zero hash pair. Schema/ABI failure
keeps its earlier priority and payload. This change does not alter the accepted
contract set or require the current logical macro value: the test baseline uses
equal logical versions of 11, while the current producer macro remains 1.

### Decisions still open

- **Equal unsupported versions:** the checker tests equality, not membership
  in supported versions. Equal arbitrary schema/ABI/logical versions can pass
  when the other checks pass. The admission owner and rejection policy remain
  to be assigned; `OK` here is not artifact compatibility acceptance.
- **Reserved fields:** `reserved0` and `reserved1` are not checked. Whether
  they must be zero, and which boundary enforces that, remains open.
- **Generation lifecycle and hash authority:** equality checks do not establish
  a persistent-to-runtime generation mapping, a unique producer for every
  hash, or canonical hash inputs. Those parts of 00.02 remain open.
- **Historical AOT name collision:** canonical ABI 17 and the tracked historical
  ABI 2 header coexist. The include-route evidence above does not close the
  repository-wide owner and consumer boundary.

## Focused tests and finite RED/GREEN evidence

[`test_ssa_contract_freeze.c`](../../tests/parser/test_ssa_contract_freeze.c)
contains these focused checks:

| Test function | Assertions currently present |
| --- | --- |
| `test_identical_contract_is_accepted` | `OK` and cleared diagnostic code |
| `test_signature_and_layout_mismatch_are_structured` | Signature/layout status; expected/actual signature hashes |
| `test_version_and_generation_fail_before_identity_resolution` | Schema/generation status; expected/actual schema versions |
| `test_capability_is_not_inferred_from_effects` | Added capability and added effect are classified separately |
| `test_logical_mismatch_reports_logical_versions` | Logical-only status and all nine diagnostic fields |
| `test_logical_version_diagnostic_preserves_uint32_boundaries` | Logical 0 / `UINT32_MAX` in both mismatch directions |
| `test_schema_and_abi_diagnostics_take_priority_over_logical` | Simultaneous schema/ABI/logical and ABI/logical mismatches preserve the legacy payload |
| `test_logical_diagnostic_precedes_identity_checks` | Logical mismatch wins over token, generation, hashes, capabilities and effects |
| `test_logical_checks_allow_null_diagnostic` | Logical mismatch and equal contracts permit NULL diagnostic |
| `test_repeated_logical_failure_then_success_clears_diagnostic` | Repeated failure/success cycles clear all nine fields on success |

The original four groups are retained; six logical diagnostic groups were added.
The new groups compare both input structs byte-for-byte before and after each
check. Poisoned diagnostics expose stale fields, while equal logical version 11
remains accepted. These cases do not establish supported-version membership,
reserved-field policy or every branch of the full interface matrix.

The existing CMake target `zr_vm_ssa_contract_freeze_test` compiles only this
test and `runtime/execution_contract.c`; CTest registers `ssa_contract_freeze`
in [`ssa-tests.cmake`](../../tests/cmake/ssa-tests.cmake). Its function names do
not supply a resolver-order trace: the checker itself contains no resolver.
The registered CTest was not invoked in the finite validation below.

### Windows standalone RED/GREEN evidence

Execution date: 2026-10-15 Pacific / 2026-10-16 UTC.

Both RED and GREEN compiled the test and the real checker as two C11
translation units, linked and directly ran separate Windows x64 executables
with Clang-cl 19.1.5 and MSVC cl 19.44.35229.0. The imported VC toolset was
14.44.35207 with Windows SDK 10.0.26100.0. Each invocation recorded exactly
12 naturally terminal phases with both streams drained and zero warnings.

| Evidence | Actual result | Receipt SHA256 |
| --- | --- | --- |
| RED7, unchanged production D7D | Run children 33676 / 40336 each exited 1, stdout empty, stderr exactly `FAIL: diagnostic expected version differs\r\n`; other phases exited 0 | `5850DC2E7FD275C8859E65EF568A5EC0E997F0030BD89246E3B6B42EFFD4F4B9` |
| GREEN, minimal production D82 | Run children 7240 / 3780 each exited 0, stdout exactly `ssa contract freeze PASS\r\n`, stderr empty; all phases exited 0 | `53E4C6111EC3A365E2377AA6F15AA8042FF124B1EE39507C0407C9B52F12DC31` |

Receipts are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`, named
`contract-freeze-logical-red-receipt.json` and
`contract-freeze-logical-green-receipt.json`. Their sibling `*-stdout.log` and
`*-stderr.log` preserve the original bytes. Both aggregate log hashes and all
24 phase-range hashes were independently checked for each invocation.
RED controller 33320 completed naturally with code 0 (unified 76600,
terminal `e81e37`); GREEN controller 20988 did likewise (unified 61894,
terminal `ea2ca7`). GREEN ran from `2026-10-16T01:54:17.5162977Z` to
`2026-10-16T02:07:05.5649903Z`.

| Physical input | SHA256 |
| --- | --- |
| Test, unchanged across RED/GREEN | `D8621163693F2ED919F1265806E0A0BA950D26768DD9A1BCA4B2D0F02CC9DE55` |
| Production before logical payload fix | `D7D5240BE5EC2DE58EE4A45FB277D9BFC135D0C9C7F1FC3A806A603C5EF369C2` |
| Production after logical payload fix | `D82E8233679CF18697FA5422545AC3AB99C93CCE058D857BB27FD62619C889AC` |

RED bound HEAD `619c9eebfe9ed9a3ed86b613b6fc6eb69981b4d3`; GREEN bound
`67a3fed011c3dc971560a772b293d25f7b7d1edc`. Each remained unchanged within
its invocation. The selected tracked diff SHA was
`143C5D6AAFE91E6AAEA4D5809C6BB94BEF87E6BF8405E4C4B6F4599D6001A1C3`
in both receipts; this tracked-only view does not cover untracked production
or layout files, so the separate physical hashes above bind the actual
checker change. Neither selected diff supplies a complete dirty-tree digest.

This accepts only the focused logical diagnostic repair. No CTest, Linux,
UBSan, Source, VM, Oracle, Global initialization, network or FFI execution
is credited. Supported-version admission, reserved fields, historical ABI 2
consumers and the full interface-family matrix remain open; plan 00.02 and
whole-leaf acceptance remain open.
