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
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
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
  - tests/core/test_ssa_backend_admission.c
  - tests/core/test_ssa_backend_service.c
doc_type: testing-guide
---

# SSA execution contract freeze

## Scope and declaration owners

This 2026-10-15 source audit records the version owners, the pure contract
checker's current boundaries and the finite AOTIR consumer check below. The
historical evidence is retained. The current four-page dictionary now freezes
the declared interface/field owners, hash domains and compatibility policies;
see the current delivery and original assertion ledger below. Plan00.02 remains
open for its strict numeric operation baseline, unverified original runtime
assertions and explicit00.01 prerequisite.

The current Core API declarations of `SZrExecutionContract`, the nine-field
`SZrExecIrDiagnostic`, their status/code enums, and the two
`ZrCore_ExecutionContract_*` entry points are in
[`execution_contract.h`](../../zr_vm_core/include/zr_vm_core/execution_contract.h).
The checker and status-name implementation belong to
[`runtime/execution_contract.c`](../../zr_vm_core/src/zr_vm_core/runtime/execution_contract.c).
No parallel diagnostic declaration is needed in a plan or consumer.
The records contain scalar identities and masks, with no AST, runtime pointer,
or host address. D4 now freezes persisted generation separately from the
process load epoch: the Core loader registry is the unique signer of nonzero,
nonreused u64 epochs. Actual registry/lease/invalidation integration is planned
in08.03/08.04/10.01; the current public generation-key ABI is unchanged.

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

### Historical AOT header and current isolation policy

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
known-route inventory and D5 isolation policy are now recorded in the
four-page compatibility guide: top-level ABI17 is production; nested ABI2 is
reference-only. Any future historical build requires a separately named target
and explicit include closure, with no ABI2/current17 object mixing. All68 original
known routes plus two library-header additions are indexed; external/manual
consumers remain outside this observed closure. This documentation neither merges, deletes, nor synchronizes either
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

### Current frozen decisions and implementation limits

- CoreIR is the legacy name for ExecIR, not another semantic IR. ExecBC and
  AOTIR are projections. Failed inference preserves only legal original semantics;
  invalid borrow or a missing static contract is not a dynamic fallback.
- The generic checker remains an equality comparator, including equal logical11.
  Each admission owner chooses supported versions. Current backend requires
  schema6/logical1 and the selected registration ABI, including legal23; this
  does not alter generic checker acceptance.
- Reserved words are record/consumer-specific: generic checker unchanged,
  backend/AOTIR owner gates strict zero; ZRAF writer-zero/reader-ignore fields
  retain current ignore behavior and gain no semantics or authority. New use
  or tightening requires explicit compatibility/version review.
- D1 GetContract is a planned Core GC-domain policy snapshot, not GetIdentity
  renamed. D2 TransferValue is a planned independent-copy transaction, not
  cross-domain GC pointer/control/lease sharing. D3 Gc_GetStats retains its
  actual budget telemetry meaning, not a general heap census.
- D4 separates disk semantic generation from planned process nonzero u64 epoch,
  with no wrap/reuse across reload or manager recreation. Old leases live until
  release; rollback publishes a new epoch. Registry/provider integration is not
  implemented by this policy delivery.
- D5 freezes production ABI17 and all known ABI2 route isolation; external/manual
  build resolution remains the consuming owner's responsibility.
- Hot-reloadable cross-target inline requires a version entry plus
  invalidation/deopt dependencies. Only a proven frozen target may inline
  unconditionally. This is a requirement, not an implemented compiler claim.
- H31 input widths/order/domain and C315 consumers are a finite deduplicated
  inventory. Context-local TypeIds may enter current hashes/ExecIR contracts:
  cross-context reuse requires mapping/recomputation or verified same-context
  identity. Independent hash-construction failure reporting remains downstream.
- The current strict numeric per-operation baseline still needs source-grounded
  overflow/shift/division/conversion/NaN/signed-zero/rounding/exception rules.
  Future09.02 profiles/vector/fast-math do not substitute for that current table.

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
is credited by this historical run. The later four-page delivery freezes
support-owner/reserved/known ABI2/I-family policy without enlarging these native
results. Plan00.02 and whole-leaf acceptance remain open.

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
This is the AOTIR consumer's admission rule. Support-membership and
reserved admission belong to the consuming owners; the generic equality checker
is not a supported-version gate. Historically those policies in `ZrCore_ExecutionContract_Check` were still open at that run; the current policy above supersedes that pending
statement. The logical checker repair and evidence retain their finite scope.

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
words remain statically reviewed only. The later owner policy preserves the
generic equality checker and assigns support/reserved admission to consumers. The standalone work credits
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
Equal unsupported values may compare successfully; current supported-set and
reserved admission now belong to each consumer under the frozen policy. This test does not
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

## Four-page dictionary delivery and original exit ledger

- [Runtime owner and D1–D4](../core-runtime/execution-contract-ownership.md).
- [Persistent field dictionary](../module-system/persistent-contract-fields.md).
- [I19/20 interface/status matrix](ssa-interface-contract-matrix.md).
- [Compatibility, H31/C315 and D5 ABI isolation](ssa-contract-compatibility.md).

The original semantic review100C1763 accepted bounded core policy.
Initial data review32813B7A found defects and stays failed; correctionFF4067A7
plus corrected-data review B5CB0E4D8D2CF6CDE76DBB3FBAA5C9CFEBCE3F774F92B730A32CAB3899D3FD0F
provide the accepted current four-page view. Reports are under
E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/.
This does not attach corrected-page hashes to an earlier review without its bridge.

The declared index is I19 families/20 ReadWrite operations; F117 records and
836 paths (824 wire,12 execution semantic-only). Legacy117 functions/976 events
and15 runtime records/115 paths are separate denominators, not additive fields.
H31/C315 is the bounded deduplicated source union. Eight-record/208-field
coordinate correction changes no wire count; legacy sourcepins35→18 is a data
correction. The packed prototype14+31=45 fields have existing layout guarantees.
Opaque inner payloads remain bounded outer windows with named downstream owners;
no complete unknown inner codec is inferred from byte count.

### Source, artifact, profile and patch compatibility

| Case / frozen policy | Current actual boundary / evidence | Planned integration or missing evidence |
| --- | --- | --- |
| Changed source: recompile; reject mismatched complete cache key | H16 Parser IR-cache key binds source/dependency/profile/compilerABI/pipeline/target/numeric/layout/profile identities. Pure checker version mismatch returns RECOMPILE_REQUIRED. | Neither establishes a full source-change→miss→successful-recompile caller transaction.11.01/Parser cache owner provides that fixture. |
| Old artifact: reject unsupported schema and publish no partial module | Core artifact/codec owner has version/range admission; pure checker version-payload cases are actually tested below. | An in-memory checker rejection is not old-file loading or publication rollback.08.01 loader supplies old-artifact typed rejection and zero partial publication/storage/lease proof. Unverified here. |
| Profile mismatch: ignore imported profile while legal compilation continues | Parser profile validation/hash and cache key distinguish identities; actual producer/consumer boundaries are in I/H tables. | Validator rejection alone does not prove caller ignore-and-continue.02.05/11.01 needs an importedProfile.compilerABI mismatch fixture and legal compilation without that profile. No such run credited here. |
| Patch contract mismatch: reject patch and retain active generation | Current Core validator has base/publicABI/profile/capability/signature/content checks. I12 records no production host caller in the finite scan. |08.02–08.04 production validate→publish/lease/rollback integration requires separate evidence. No real patch/loader/security execution is credited here. |

### Original 00.02 assertion ledger

This retains the original four test requirements and three assertion drafts.
Each row states actual evidence and gaps; a source declaration or policy is not
a passing runtime fixture. Pure checker diagnostic block/instruction/source IDs
are intentionally0; N/A below does not mean measured zero.

| Original requirement | Exact available test / platform and scope | Diagnostic position / verifier | FI and ownership evidence / missing gate |
| --- | --- | --- | --- |
| T1 same token, different signature/layout: reject before pointer resolve | test_signature_and_layout_mismatch_are_structured; test_scalar_mismatch_payloads_preserve_both_boundary_directions; Windows Clang-cl/MSVC standalone matrix9B7B2936 | Pure status/payload, expected token; no resolver exists and no resolver-order trace or ExecIR verifier run | Stack-only checker: allocations/FI/leases N/A. Backend result_signature/result_layout PASS with cleanup, but not linker lookup proof. |
| T2 guard miss versus stale generation: deopt versus link error, no static string lookup | test_version_and_generation_fail_before_identity_resolution and matrix cover checker stale status; backend result_generation_reserved/result_contract_generation cover key/contract mismatch | Backend expected1/actual2 asserted through diagnostic helper. No actual deopt/name resolver invoked; no runtime source-position proof | Backend failure U→R cleanup checked. Real guard-miss provider, deopt/link-position/no-lookup trace remain unverified;01.04/08.03/10.01. |
| T3 cross-domain borrowed/thread-affine native handle rejected or independently materialized | D1/D2 policy and clone/share/ownership source boundaries only; no corresponding actual run credited here | No materializer, verifier or source-position execution credited |06.03/06.04 preflight/host permission, independent resource, Throw/OOM/abort/claims/release fixture still required. Backend counters do not cover it. |
| T4 hot reload with cross-target inline requires invalidation/deopt dependency | Frozen policy above; no actual compiler+hotreload case attached | No inline decision/verifier/event trace credited |04.02/08.03/10.01 must prove absent-dependency rejection and old-lease retention. Unverified here. |
| A1 same token/layoutHash differs: checker fails before pointer resolve | T1 plus backend result_layout covers scalar rejection | Expected/actual layout payload tested; actual pointer-resolution ordering outside pure checker | Reuse T1 finite evidence, not an extra case count. |
| A2 importedProfile.compilerABI differs: ignore profile, compile legally | Four-row policy table; no matching actual import-and-compile fixture attached | No compilation/verifier/profile-ignore event credited |02.05/11.01 caller fixture missing; no fabricated FI or ledger values. |
| A3 old artifact schema: RECOMPILE_REQUIRED, no partial module | Checker schema mismatch is tested; old-artifact load transaction is not | Checker version payload proven, loader mapping/publication diagnostic not proven |08.01 module/storage/lease rollback, failure and retry fixture unverified here. |

The logical/pure-checker Windows standalone receipts above retain their actual
scope, including matrix9B7B2936; they are not Linux/CTest/full SSA validation.
Source42 GREEN review221A4D88 separately accepts Windows Clang-cl Debug UBSan
metadata admission33 plus seven serial service scenarios via2/2 CTest.
Generation review07C498FE binds the unchanged test's sole expanded-RED failure
result_contract_generation (32PASS/1FAIL) becoming GREEN33PASS/0FAIL.
Actual CASE totals query/compile/U/R19 each and D28 are finite callback/cleanup
counts, not allocator FI or concurrent lease telemetry. MSVC394EB90F accepts
six C syntax checks only, no MSVC runtime. PostcommitF02D1081 proves finite backend
publication, not the runtime requirements in this ledger.

### Exit accounting

D1 GetContract/06.03, D2 TransferValue/06.04, D4 process epoch/08.03 with08.04/10.01
consumers, and real resume/01.04 remain planned APIs or integration. Their owner
and policy can freeze without pretending their implementations exist.
The declared dictionary and D1–D6 policy are finite accepted documentation.

Original00.02 Task2 still lacks its current source-grounded numeric operation
table: checked/wrapping behavior, shift rules, division by zero, conversions,
NaN/±0, rounding and visible exception order.09.02 owns subsequent numeric
profiles/vector/fast-math permissions, not a substitute for the current baseline.
Original runtime/transaction assertions above remain partly unverified.
00.01 remains an explicit planned prerequisite; its M0 closure depends on07.04.
No performance benefit, completed00.01 baseline or M0 completion is asserted.

Navigation integration and this finite dictionary delivery do not complete all
four numbered tasks, all three batches,00.02 or the47-leaf plan. No new native
execution is authorized or performed by this documentation integration.
