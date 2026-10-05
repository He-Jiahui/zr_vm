---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
plan_sources:
  - .codex/plans/20261005-ssa-dead-source-places.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - docs/acceptance/ssa-dead-source-places.md
doc_type: testing-guide
status: dead-source-places-focused-windows-green-accepted
fixture_extraction_status: focused-windows-green-accepted
---

# Dead Source Places Test Acceptance

## Scope

This finite parser/shared CoreExecIR entry removes proved unused temporary
addresses from real literal i64 SCRIPT graphs. The fixture follows Parse,
CanonicalizeAst, module Prepare, compile/validate/assemble, module Finalize and
BuildModule while the actual compiler context stays alive. SCRIPT_ENTRY is the
actual MEMBER_DEF record. Signature identity uses the actual canonical FUNCTION
structural hash and actual module hash. See the
[module contract](../../docs/parser-and-semantics/ssa-dead-source-places.md).

## Baseline

The frozen RED commit is `5b9d7d829624eaee52e9f7ec5766d17e968a5293`.
Build exited 0; prerequisites passed 2/2 and Oracle returned 9/8. The full
11-case suite had 6 failures: four unimplemented features and two diagnostic
boundaries. It did not fail to link. Logs and hashes are in the
[evidence record](../../docs/acceptance/ssa-dead-source-places.md).
No full repository baseline is claimed by this focused task.

## Test inventory

### Shared source preparation ownership

`support/ssa_literal_script_fixture.h/.c` now owns the one moved real-source
Prepare/Finalize/BuildModule path and its actual constants/place initial values,
source-map checks, Oracle callback and within-test digests. Prepare itself does
not execute Oracle or call compaction/frame APIs. The dead-place main keeps
thin adapters, the 30 cases and CLI, and the global Oracle/output/mutated owners.
Register Init'd fixtures in teardown before Prepare can assert; keep the state
alive until Free completes. Free handles partial preparation. AssertOracle
requires an initialized caller-owned result retained across assertion abort;
teardown releases results/graphs/fixtures before destroying the runtime state.
Both ordinary and direct CMake target routes compile the same shared support TU.

| Layer | Required checks |
| --- | --- |
| Actual source prerequisites | Genuine 9/8 SCRIPT identity, original graph and VERIFY_ALL, Oracle returns |
| Compaction | Input unchanged; address/provenance removed; dense value/pool ranges; source and constant identity retained; Oracle 9/8 |
| Publication | Repeat original input; replace prior 8 output; exact compressed input idempotence; null diagnostic success |
| Ownership | Function alias, shared values/operands/state owner, allocation-capacity overlaps rejected without releasing borrowed storage |
| Provenance | Exact symbolId/callableTypeId, source maps and definitions; actual address use and unknown provenance refused |
| Metadata | SEALED and invalid-range diagnostics; empty state header preserved; mismatched state identity and valid richer state/deopt rejected |
| Arguments | Null required arguments preserve existing output |
| Consumer regression | Source VM loops, straight-line CFG, returns, token/callable/SCRIPT identity, typed binding and dominator CFG |

The fresh focused run passed all 30 cases: 2 prerequisites, 8 features and
20 guards. Unsupported metadata is tested as a valid graph where
possible, so malformed graph rejection does not substitute for admission proof.

### Executed case names

| Group | Cases |
| --- | --- |
| Prerequisites (2) | `test_prerequisite_return_nine`, `test_prerequisite_return_eight` |
| Features (8) | `test_eliminate_return_nine`, `test_eliminate_return_eight`, `test_repeat_original_input`, `test_replace_actual_eight_output`, `test_compressed_nine_as_input`, `test_compressed_eight_as_input`, `test_null_diagnostic_success`, `test_actual_empty_state_header_preserved` |
| Existing graph guards (5) | `test_guard_actual_address_use_preserves_eight`, `test_guard_unknown_provenance_preserves_eight`, `test_guard_sealed_metadata_preserves_eight`, `test_guard_invalid_range_preserves_eight`, `test_guard_input_output_alias` |
| Source-map guards (2) | `test_guard_source_map_location_mismatch`, `test_guard_duplicate_source_map_identity` |
| Storage guards (6) | `test_guard_shared_value_storage`, `test_guard_shared_operand_storage`, `test_guard_shared_state_map_storage`, `test_guard_interior_value_storage`, `test_guard_interior_operand_storage`, `test_guard_interior_state_value_storage` |
| Exact identity guards (3) | `test_guard_missing_semantic_symbol`, `test_guard_missing_semantic_callable`, `test_guard_missing_semantic_identity` |
| State/argument guards (4) | `test_guard_empty_state_header_mismatch`, `test_guard_owned_state_value_pool`, `test_guard_owned_deopt_reconstruction`, `test_guard_null_required_arguments` |

## Tooling evidence

The direct CMake build uses Windows x64 clang-cl 19 Debug, assertions and UBSan.
UBSan checks runtime arithmetic/memory misuse in the selected C inputs. It does
not replace ownership and provenance assertions. Existing direct build root:
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.

```powershell
$env:UBSAN_OPTIONS = 'halt_on_error=1:print_stacktrace=1'
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' -C 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' zr_vm_ssa_dead_source_places_test -j4
& 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2\bin\zr_vm_ssa_dead_source_places_test.exe' --prerequisites-only
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' -R '^ssa_dead_source_places$' -VV --output-on-failure --no-tests=error
```

These commands describe the focused route; fresh Root receipt/logs pin the
executed commands, environment and results. No source copying, network,
FFI/provider calls, capability tests or hotpatch actions occur in this fixture.

Selected consumer reproduction uses the actual executable target for loops:

```powershell
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' -C 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' zr_vm_ssa_source_execbc_vm_test zr_vm_ssa_source_straight_line_cfg_test zr_vm_ssa_source_callable_return_test zr_vm_ssa_source_script_entry_tokens_test zr_vm_ssa_source_callable_identity_test zr_vm_ssa_source_script_entry_identity_test zr_vm_ssa_typed_binding_contract_test zr_vm_ssa_dominator_cfg_test -j4
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' -R '^(ssa_source_execbc_vm_loops|ssa_source_straight_line_cfg|ssa_source_callable_return|ssa_source_script_entry_tokens|ssa_source_callable_identity|ssa_source_script_entry_identity|ssa_typed_binding_contract|ssa_dominator_cfg)$' -VV --output-on-failure --no-tests=error
```

The pre-extraction primary receipt preserves the exact MSVC compiler, argument array,
INCLUDE and temporary-directory environments. To reproduce that compile-only
command without guessing any include or define:

```powershell
$smokeReceipt = Get-Content -LiteralPath 'E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\dead-source-places-current-rebuilt-green-receipt.json' -Raw | ConvertFrom-Json
$env:INCLUDE = $smokeReceipt.msvc_smoke.include_env
$env:TEMP = $smokeReceipt.msvc_smoke.temp_env
$env:TMP = $smokeReceipt.msvc_smoke.temp_env
$smokeArgs = $smokeReceipt.msvc_smoke.args
& $smokeReceipt.msvc_smoke.compiler @smokeArgs
wsl.exe --list --quiet
```

MSVC receives the current production TU directly with `/c /TC /std:c11 /MD
/Od /W4 /utf-8 /UNDEBUG`, actual project defines and includes. The final object
stays under the E: build root. The WSL command is the bounded enumeration probe
that failed; it did not run Linux validation or install tools.

## Results and acceptance decision

Behavioral RED is established and the focused producer is GREEN: build 0,
independent prerequisites 2/2, full CTest 0 with 30/30 and no observed UBSan
diagnostic. The fresh Root receipt and frozen source/log hashes are recorded in
the [evidence record](../../docs/acceptance/ssa-dead-source-places.md).
After current selected shared libraries were rebuilt, fresh configure/relink/
focused CTest all exited 0 and 30/30 passed again. The eight selected consumer
suites passed 8/8, comprising 85 Unity cases plus standalone dominator CFG,
with build/CTest exit 0 and no observed UBSan diagnostic. The first regression
build used a nonexistent loops target and ran no tests; correcting the target
resolved that invocation failure. Existing shared-source compiler warnings are
recorded separately from runtime results. These focused Windows gates are
**accepted**, with all Root-owned finite native jobs naturally terminal.
Production-TU MSVC compilation passed after correcting PATH
name resolution with the absolute compiler path; MSVC runtime is not claimed.
WSL enumeration exited 1 with `Wsl/EnumerateDistros/Service/E_ACCESSDENIED`, so
Linux GCC/Clang remain OPEN. No full repository GREEN is claimed.
Full SSA47 stays **OPEN**; primitive frame construction, canonical AOT/native
execution, returned artifact retention, other toolchains and exhaustive OOM
testing are subsequent gates.

## Independent extraction validation

The preceding RED/DD09/CC31/B5 results cover the committed pre-extraction
fixture. The shared-support extraction has separate completed validation:

| Gate | Required observation |
| --- | --- |
| Static review | Independent gpt-6-sol specification/lifecycle review: no blocker; one moved route without hidden compaction/frame/Oracle |
| Configure/build | Exit 0/0; shared support is a real target TU and its header is in refreshed metadata |
| Independent prerequisites | Exit 0; 2/2 actual source identity graphs verify and return Oracle 9/8 |
| Full focused suite | CTest exit 0; unchanged 30-case CLI inventory passes with 0 failures/ignored and no UBSan diagnostic |
| Ownership | Main keeps result/graph teardown; partial fixture Free precedes runtime-state destruction |
| Evidence | New main/support/CMake/header metadata pins and actual logs; old main/binary pins remain historical |

This refactor is **focused Windows GREEN accepted**. Root's immutable
`literal-fixture-extraction-green-receipt-v2.json` has SHA-256
`93DCDB420BCDB4E549405933F008CEDAAD836A792BA921DD174A474C47A65A3C`;
the [evidence record](../../docs/acceptance/ssa-dead-source-places.md) lists
its seven current source pins, four logs, three metadata hashes and current
binary. The current main hash is E02A7F8F…CDD55, not historical B6A5.
All commands read the live checkout directly and use the E: build root above;
configure precedes the same build, `--prerequisites-only` and full focused CTest
commands. Root's staged check found and removed one extra support-TU EOF blank
line; its final hash is 05FB2408…CCA23, with all other source pins unchanged.
The original extraction receipt SHA-256
`5524AE58BD9F3E8B583FAD1FEF6A90E4995CB6DD0839BA2F1D7319E51A345673`
is retained as first-run history. After the correction, the same focused gates
were rerun successfully. Final native session 4729 exited naturally, active
0/stopped 0. Prior MSVC
evidence is production-TU compilation only; Linux access denial and SSA47 OPEN
remain unchanged. No source snapshot or cross-drive copy is generated.
