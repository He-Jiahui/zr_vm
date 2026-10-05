---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
  - docs/parser-and-semantics/ssa-host-primitive-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame.c
  - docs/acceptance/ssa-primitive-source-frame.md
doc_type: testing-guide
status: primitive-source-frame-focused-windows-green-accepted
---

# Primitive Source Frame Test Acceptance

## Scope and baseline

The finite frame entry follows actual source compaction and explicit primitive
row creation. It starts as a callable UNSUPPORTED stub; baseline RED requires
passing real-source preparation and behavior failure, not missing symbols.
The fixture retains live compiler/context and caller-owned Oracle, graph,
projection and fixture teardown owners. State is destroyed last.

The actual frozen RED commit is `99b02fd79cffdf7801945054d357a5f3dc8508d0`.
Configure/build/independent prerequisites/regressions exited 0: prerequisite2/2,
host15/15 and compaction30/30. Full frame CTest exited8:18 cases/9 failures/
0 ignored, no observed UBSan diagnostic. The three features and repeat-attach's
first feature failed, along with frame-limit/sealed/range/range-before-sealed/
required-argument diagnostic checks. The immutable RED receipt and logs are in
the [evidence record](../../docs/acceptance/ssa-primitive-source-frame.md).

## Executed inventory and historical RED

| Group | Cases and assertions |
| --- | --- |
| Prerequisites (2) | `test_prerequisite_nine`, `test_prerequisite_eight`: real source → Oracle place1 → same-context compaction → VERIFY_ALL/Oracle place0 → actual host row/module append, no attachment |
| Features (3) | `test_attach_nine`, `test_attach_eight`, `test_attach_null_diagnostic`: owned whole-lifetime frame, exact three counts/mapping/geometry, unchanged graph/source/tables, VERIFY_ALL/Oracle9/8 and canonical AOT NOARGS_I64 nonrunnable projection |
| Input representation (2) | `test_original_address_unsupported`, `test_core_valid_external_unsupported`: original residual address and separately Core-valid external contract mutation refused |
| Ordinary definition (1, GREEN only) | `test_core_valid_missing_definition_unsupported`: real Core-valid definition0 refused before definition-1 indexing; restore metadata before assertion |
| Explicit rows (3) | `test_missing_row`, `test_duplicate_actual_type_row`, `test_invalid_row_fields`: no missing/ambiguous/invalid target facts accepted |
| Budget/state (3) | `test_frame_limit`, `test_sealed`, `test_repeat_attached_frame`: limit refusal, SEALED, preservation of full existing frame digest |
| Verifier priority (2) | `test_bad_return_range`, `test_bad_return_range_precedes_sealed`: precise malformed diagnostic before sealed/admission |
| Rich metadata (2) | `test_owned_state_pool`, `test_owned_deopt`: Core-valid richer owned state/deopt is UNSUPPORTED, never erased |
| Arguments (1) | `test_required_arguments`: required null/count arguments preserve graph |

Historical RED executed 18 cases/9 failures. Final GREEN executed 19 cases,
2 prerequisites + 3 features + 14 guards, all passing. Full frame observations include owned header and slot contents;
successful body observations mask only frame pointer and contract.layoutHash.
Expected AOT physical indexes are zero-based, while frame slotId is actual
ValueId. The original module constants/layout table and source maps are compared.
No source/callable/BOOL/ABI proof is inferred from mutated or synthetic rows.

The final GREEN inventory adds one Core-valid optional-definition guard:
Core permits an ordinary value's definition zero, while this narrow producer
requires a valid nonzero definition and must check it before indexing. The
review correction was made before execution of the intermediate version.
The added guard restores actual metadata before assertion; it is not part of
the immutable 18-case RED. Its actual final run passed as part of the 19-case GREEN.

## Tooling route

Root uses Windows x64 clang-cl19 Debug assertions plus UBSan in the existing
direct checkout build. The command route after configuration is:

```powershell
$env:UBSAN_OPTIONS = 'halt_on_error=1:print_stacktrace=1'
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' -C 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' zr_vm_ssa_primitive_source_frame_test -j4
& 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2\bin\zr_vm_ssa_primitive_source_frame_test.exe' --prerequisites-only
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' -R '^ssa_primitive_source_frame$' -VV --output-on-failure --no-tests=error
```

Frozen target/CLI names, logs and executed command results must be checked
against actual CMake and immutable receipts. The final CTest selection was
`^(ssa_primitive_source_frame|ssa_host_primitive_layout|ssa_dead_source_places)$`.
No WILL_FAIL or source-copy route
is used. Outputs stay under the managed E: build/report roots.

The separate new-TU MSVC command can be reproduced from the exact recorded
parameters without guessing defines, include directories or object paths:

```powershell
$frameReceipt = Get-Content -LiteralPath 'E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\primitive-source-frame-green-receipt.json' -Raw | ConvertFrom-Json
$env:INCLUDE = $frameReceipt.msvc_smoke.include_env
$env:TEMP = $frameReceipt.msvc_smoke.temp_env
$env:TMP = $frameReceipt.msvc_smoke.temp_env
$frameArgs = $frameReceipt.msvc_smoke.args
& $frameReceipt.msvc_smoke.compiler @frameArgs
```

## Results and decision

Actual RED is established and the finite Windows frame gate is **GREEN accepted**.
Receipt SHA-256 is `F53F39E2D3DC0E4EC44D2CED7EC2425841C06D464C7C0AD22C4FA0C380405063`.
Configure/build/independent prerequisites/CTest exited 0: independent 2/2 and
frame 19/19, host 15/15, compaction 30/30 across three suites, failures 0/ignored 0,
no UBSan diagnostic. Native 7817 and the MSVC compile job ended naturally.
The evidence record pins 14 sources, five logs, three metadata files, frame
binary and MSVC object. MSVC compile-only exited 0 with no warnings in that TU
log, not a full runtime or repository-warning claim. No new eight-suite
regression is claimed. Row hashes are nonzero evidence, while the current packed geometry
hash does not include row fingerprints; AOT must retain explicit rows. Attach
cannot independently certify original context/callable/source provenance. The
fixture supplies actual context and SemIR.callableTypeId to canonical projection.
That projection's runnable=false does not establish native/descriptor runtime
or returned artifact retention. Exhaustive OOM, cross-target ABI and broader
metadata remain separate gates.

Linux GCC/Clang remain OPEN after the existing WSL service access denial. Any
MSVC evidence is new production-TU compilation only. Full SSA47 stays OPEN.
Network, FFI, providers, capability/hotpatch and security functions are outside
this local pure test scope; no source snapshot or cross-drive copy is produced.
