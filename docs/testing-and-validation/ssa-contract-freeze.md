---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
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
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_contract_freeze.c
  - tests/parser/test_ssa_aotir_contract.c
doc_type: testing-guide
---

# SSA execution contract freeze

## Scope and declaration owners

This 2026-10-15 source audit records the version owners, the pure contract
checker's current boundaries and the finite AOTIR consumer check below. It
does not close plan 00.02: the complete
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
  the pure checker must require zero, and its admission policy, remain open.
  The separate AOTIR module/function validator requires both fields to be zero;
  that consumer requirement does not close the pure checker's policy.
- **Generation lifecycle and hash authority:** equality checks do not establish
  a persistent-to-runtime generation mapping, a unique producer for every
  hash, or canonical hash inputs. Those parts of 00.02 remain open.
- **Historical AOT name collision:** canonical ABI 17 and the tracked historical
  ABI 2 header coexist. The include-route evidence above does not close the
  repository-wide owner and consumer boundary.

## Focused logical tests and finite RED/GREEN evidence

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
is credited. Supported-version admission, the pure checker's reserved-field
policy, historical ABI 2
consumers and the full interface-family matrix remain open; plan 00.02 and
whole-leaf acceptance remain open.

## AOTIR consumer reserved-zero boundary

The module and function execution contracts consumed by
`ZrCore_AotIr_ValidateModule` require `reserved0 == 0` and `reserved1 == 0`.
The file-local check in `aot_ir.c` preserves this order:
schema, ABI, logical version, `reserved0`, then `reserved1`. Schema/ABI
failures retain `ZR_AOT_IR_VERSION_MISMATCH`; logical-version and reserved-word
failures use `ZR_AOT_IR_INVALID_CONTRACT`. A reserved-word diagnostic carries
`expected = 0` and the original nonzero word as `actual`. Module diagnostics
use function ID/index zero; function diagnostics preserve the function ID and
module index. Block/instruction IDs remain zero. NULL diagnostics preserve
the return status, and validation leaves the caller's input unchanged.

`ZrCore_AotIr_HashModule` already calls module validation before hashing, so a
nonzero reserved word now reaches the existing invalid-input return of zero.
Valid zero-reserved inputs retain the prior hash algorithm and field order.
This is the AOTIR consumer's admission rule. Equal unsupported versions and
reserved-field admission in `ZrCore_ExecutionContract_Check` remain open; the
logical checker repair and its evidence above retain their finite scope.

The related fixture is
[`test_ssa_aotir_contract.c`](../../tests/parser/test_ssa_aotir_contract.c).
It defines eight combinations: module/function ownership, `reserved0` or
`reserved1`, and value `1` or `UINT32_MAX`. Its checks cover every diagnostic
field, NULL diagnostics, schema/ABI/logical priority, invalid hash zero,
byte-preserved inputs, repeated rejection and recovery to the original valid
hash. It uses a single function at index zero. Nonzero function-index
propagation and simultaneous contamination of both reserved words have only
been statically reviewed and are not credited as executed cases.

### Original standalone RED: reproduced defect, failed invocation

The three real C11 translation units were the fixture, `aot_ir.c` and
`exec_ir_state_map_storage.c`. Across Clang-cl and MSVC, six compiles exited 0
with zero warnings or unknown output, and two links exited 0 with empty
streams. Both executable children exited 1 with stdout empty and stderr
exactly `FAIL: AOTIR reserved validation status differs\r\n` (48 bytes,
SHA256 `02BA6701CDD6922DA293F95765F154420FEF2C036C3CE1A91AF10D1510C547B1`).
Each child stopped at the first reserved-status expectation; the remaining
fixture checks are not established by this RED run.

The immutable receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/aotir-reserved-red-receipt.json`,
SHA256 `16C494AFD093CE2E6E194903A91491D62F1F7F6D7FDD8C92DE18A3A137B5EB7D`.
All 14 phases were recorded before the original final HEAD/diff guard failed
with `HEAD or selected-path diff changed during validation`. The controller
naturally exited 1; `validated`, `functional_pass` and `red_failure_accepted`
are all false.

HEAD changed from `8809939b34f8106c5cc08d4ba20bea12a1ab5795` to
`b321725320b95556d0762ba7a39411b6d2ec23b3`. The selected tracked diff bytes
were identical. Independent review verified all raw ranges and hashes,
214 unchanged input pins, eight unchanged output pins and the actual
199-header dependency union; there were no missing inputs or dependency-list
differences. The two child failures establish the pre-fix defect reproduction;
the overall invocation remains failed. Its recorded false flags are preserved.
The physical source hashes are listed in
[`Shared AOTIR contract`](../instruction-generation/aotir-contract.md).

The independently reviewed helper revision
`06272B45165ECEE3FA74CF5A44C0DFDE3489DB48483D01FA64A6FC96FA58BB28`
changes only final Git provenance handling. It validates final Git output and
records the after-HEAD, after-diff hash and actual comparison booleans. Repeated
physical source/test/tool/dependency/output pins remain authoritative; actual
`showIncludes` closure, 14 phases, eight outputs, 20 registered paths, warning
and unknown-output rejection, telemetry/encoding controls and exact stream
protocols remain enforced. This policy revision does not alter the old receipt.

### AOTIR GREEN: accepted finite reserved-word repair

The repaired production file had SHA256
`AEC33CD3304BB67FF796EEF3C2D7DE482C8C846915DD57D53A031237CD2F4E1A`;
the fixture and storage translation unit retained their RED identities.
The invocation ran from `2026-10-16T04:41:32.5987955Z` to
`2026-10-16T05:06:01.7498359Z`. Its receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/aotir-reserved-green-receipt.json`,
SHA256 `931C57FA57DB197E5257791C145F03116A075667D4EC9C69C1824FF1F8943023`.
It records `validated = true`, `functional_pass = true`,
`red_failure_accepted = false` and a null error. Controller 17712, native
handle 39899, completed naturally with exit 0 (terminal chunk `6a6502`) and
reported `GREEN pure AOTIR reserved protocol accepted; functional_pass=True\r\n`.

Independent evidence review accepted the exact 14-phase order and natural
terminal results. All six compiles exited 0 with zero warnings or unknown
output. Both links and both executable runs exited 0 with stdout/stderr empty;
the controller's success message is a separate stream. All 28 raw ranges were
contiguous and matched their hashes. Aggregate stdout was 103346 bytes,
SHA256 `5986A3DDBF7B45950D7D4AB7CD0C83B51F9E81BED59B845F7EA098BC2C1152BC`;
aggregate stderr was empty,
SHA256 `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855`.

The review independently matched all 214 current input pins, including tools
and language resources, and eight output pins, with zero drift, missing files
or reparse paths. Each Clang-cl compile's 183 dependencies and each MSVC
compile's 78 dependencies exactly matched the strictly decoded `showIncludes`
output. Their union was exactly the 199 actual dependency pins, each matching
its fresh baseline and input hash. Filtering the authenticated 3084-path
universe independently produced the same 2396 permitted baseline paths.
The published output set contained exactly eight files and native temporary
storage was empty.

Raw Git output confirmed unchanged HEAD
`2e5ec42f6db4769e57d6049f758aad1a7e0ad1a3` and selected tracked diff SHA256
`11EDA8424F65BD0CCD2B0BCDB8D03C56AB0D36969ADC332943B9F0723B8C59B5`.
All three recorded comparison booleans were independently confirmed true.
The Parent pin remained
`9FDA62C83D1B129EA015B3A3DE4DD32CF38E3D4DB6C0F0D010094B8B3DF422E8`
through this review; neither of these documentation files was a GREEN input.

This accepts only the eight reserved-word combinations and their recorded
diagnostic, hash-zero, input-preservation and recovery behavior. Nonzero
function-index propagation and simultaneous contamination of both reserved
words remain statically reviewed only. The pure checker's reserved policy and
equal unsupported-version admission remain open. The standalone work credits
no CTest, Linux, Source, Global, VM, backend artifact execution, network, FFI or
full-project validation. Plan 00.02, whole-leaf acceptance and the overall
47-leaf plan remain open.


## Pure checker diagnostic matrix: finite Windows validation

`tests/parser/test_ssa_contract_freeze.c` adds six groups to the ten logical
and identity groups above. It calls the public checker directly using stack
contracts and poisoned diagnostics. `expect_true` checks remain active when
`NDEBUG` is defined. The original equal-logical-version 11 case and success
output remain part of the test.

| New group | Exercised boundary |
| --- | --- |
| `test_null_arguments_clear_all_diagnostic_fields` | Three NULL input combinations, optional diagnostic, live input preservation and recovery |
| `test_scalar_mismatch_payloads_preserve_both_boundary_directions` | Fourteen scalar boundary directions; the earlier two logical directions remain covered separately |
| `test_mask_payloads_reject_unknown_bits_on_either_or_both_sides` | Twelve unknown-mask combinations and four known-mask mismatch cases, with complete payloads |
| `test_equal_known_masks_clear_all_diagnostic_fields` | Four equal valid mask combinations and success clearing |
| `test_each_earlier_failure_precedes_each_later_failure` | All 45 pairs of the ten field entries, checking the declared first failure and all nine diagnostic fields |
| `test_each_failure_repeats_and_recovers_without_stale_diagnostics` | Two failure/success cycles for each of ten fields, optional diagnostic and byte-preserved inputs |

Schema and ABI occupy two payload entries in the same implementation branch.
The 45 field pairs do not establish separate schema-versus-ABI priority.
Equality of unsupported versions and the generic checker's reserved-field
admission policy still require their separate decisions. This test does not
supply runtime or persisted-format admission rules.

### Actual standalone execution

The same checkout bytes were compiled with Windows Clang-cl and MSVC. Each
compiler compiled the test and `runtime/execution_contract.c`, linked the
standalone executable and ran it directly. Compilation used C11, `/W4`,
UTF8 source encoding and `/UNDEBUG`. Four compile phases were warning-free;
both links were silent; each run exited 0 with stderr empty and exactly
`ssa contract freeze PASS` plus CRLF (26 bytes), SHA256
`3A1948D09C3D68928A6D7BE8994FCF5619503D33D25586B191B3984DC2D891F5`.

The controller 20956, unified session 61261, naturally exited 0 at terminal
`f41089`; the receipt finished at 2026-10-16T05:56:21.5802335Z. All 12 recorded
phases were naturally terminal with both streams drained: four Git metadata
observations, four compilations, two links and two checker runs.

| Evidence | Actual identity |
| --- | --- |
| Test input | `6EA99BB6FB842B649F6F3458C2EA65DA3E96D544EF8FC69E4FEF49E443E25122` |
| Production input | `D82E8233679CF18697FA5422545AC3AB99C93CCE058D857BB27FD62619C889AC` |
| Receipt | `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/contract-freeze-matrix-green-receipt.json`, SHA256 `9B7B2936B67ECED2CEC8D72D3E6B5B83739F903E4D543EEF47FC570FF73EDC88` |
| Raw stdout | 36787 bytes, SHA256 `98CC03DAE0A7D2A6A0FA1C7F7FD1E7C7B33C70D9BB53B63D9E2B8825A1CD1C98` |
| Raw stderr | 0 bytes, SHA256 `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |
| Physical checks | 209 input pins, 195 actual header dependencies and 6 published products; raw stream ranges and aggregate hashes checked |

The 2395 allowed header identities were captured before compilation. Actual
includes were decoded as strict UTF8 for Clang and CP936 for MSVC and checked
against those identities throughout execution. Git observations used raw
NUL-framed metadata, containing no source blob bodies. HEAD and selected
diff were observed equal before/after; acceptance also required unchanged
physical source, tool, actual dependency and output identities.

This accepts the pure diagnostic matrix on the two recorded Windows tools.
Source, Global, VM, Function execution, plugins, FFI, network, Linux, CTest,
backend semantics and the complete 00.02 leaf were outside this invocation.
