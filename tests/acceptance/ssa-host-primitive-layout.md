---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
tests:
  - tests/parser/test_ssa_host_primitive_layout.c
  - docs/acceptance/ssa-host-primitive-layout.md
doc_type: testing-guide
status: host-primitive-layout-focused-windows-green-accepted
---

# Host Primitive Layout Test Acceptance

## Scope and baseline

This finite task adds a host-only actual canonical i64 layout row producer.
Shared support moves no additional preparation code: the single genuine
Parse/Canonicalize/Prepare/compile/validate/assemble/Finalize/BuildModule route
supplies live context and actual TypeIds from source 9/8. The new API starts as
a callable UNSUPPORTED stub. Successful prerequisites plus failing behavior,
rather than a compile/link error, are required to establish RED. No repository
wide baseline is claimed. Immutable RED commit
`57c0dc8a2c9c0f21ec9fcb5fa05504168b8bd2fc` built successfully, passed independent
prerequisites 2/2 and compaction 30/30, then host CTest exited 8 with 15 cases,
12 failures, 0 ignored and no observed UBSan diagnostic. Three creation and nine
diagnostic boundaries failed; real callable refusal passed. The
[evidence record](../../docs/acceptance/ssa-host-primitive-layout.md) pins RED.
Fresh GREEN passed host 15/15 and compaction 30/30 after independent 2/2 prerequisites.

## Executed test inventory

| Group | Case names and assertions |
| --- | --- |
| Source prerequisites (2) | `test_prerequisite_nine`, `test_prerequisite_eight`: genuine canonical i64/source maps and actual Oracle 9/8 |
| Host row/table (1) | `test_host_i64_rows_append_and_layout_id_does_not_change_hash`: host size/alignment, independent 63-byte hash, actual checked table appends, differing layout IDs |
| Context identity (1) | `test_two_real_contexts_produce_same_host_hash`: different live context addresses, actual matching host row hash; no assertion of different TypeIds or salts |
| Optional diagnostic (1) | `test_success_accepts_null_diagnostic`: successful actual row append |
| Argument/range (2) | `test_required_arguments_and_zero_ids`, `test_unknown_type_id`: diagnostic class and byte-for-byte failure preservation |
| Array shape (4) | `test_invalid_canonical_array_valid_flag`, `test_invalid_canonical_array_element_size`, `test_invalid_canonical_array_length`, `test_invalid_canonical_array_null_head`: INVALID_RANGE without invalid storage reads |
| Arithmetic (2) | `test_canonical_capacity_product_overflow`, `test_canonical_address_span_overflow`: CAPACITY_OVERFLOW before native span use |
| Admission/hash (2) | `test_actual_callable_is_unsupported`, `test_actual_node_zero_hash_preserves_output`: real FUNCTION refusal and zero structural hash LAYOUT_MISMATCH |

The same 15-case GREEN inventory passed all assertions. The unknown-ID test
also proves canonical Find returns null before API invocation. Faulted canonical
array descriptors and node hashes are restored before assertions; teardown
retains real fixture allocations. Required caller storage remains valid,
independent and live. Arbitrary pointer ownership and failure injection outside
that contract are not inferred from these finite tests.

## Tooling route

The direct Windows clang-cl 19 x64 Debug tree enables assertions and UBSan.
Use the current checkout and existing configured E: build root:

```powershell
$env:UBSAN_OPTIONS = 'halt_on_error=1:print_stacktrace=1'
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' -C 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' zr_vm_ssa_host_primitive_layout_test -j4
& 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2\bin\zr_vm_ssa_host_primitive_layout_test.exe' --prerequisites-only
& 'E:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir 'E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2' -R '^ssa_host_primitive_layout$' -VV --output-on-failure --no-tests=error
```

The ordinary and direct targets both compile the shared support TU. Direct
source metadata keeps one source occurrence per target and records distinct
target/source pairs across the host-row and compaction targets. A test is not
marked WILL_FAIL, and no source copy provides its inputs. Root pinned executed
commands, logs, source/metadata/binary hashes and actual exit codes in the
immutable GREEN receipt.

### Current GREEN and MSVC reproduction

The CTest GREEN selection was
`^(ssa_host_primitive_layout|ssa_dead_source_places)$`; both suites passed.
The absolute-path MSVC command is reproducible from its exact recorded args:

```powershell
$hostReceipt = Get-Content -LiteralPath 'E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\host-primitive-layout-green-receipt.json' -Raw | ConvertFrom-Json
$env:INCLUDE = $hostReceipt.msvc_smoke.include_env
$env:TEMP = $hostReceipt.msvc_smoke.temp_env
$env:TMP = $hostReceipt.msvc_smoke.temp_env
$hostArgs = $hostReceipt.msvc_smoke.args
& $hostReceipt.msvc_smoke.compiler @hostArgs
```

This compiles only the live host-layout TU into the E: build root. MSVC emitted
C4127 at lines 54 and 106; successful exit 0 is not a warning-free runtime claim.

## Results and acceptance decision

Behavioral RED is established and the finite host-row Windows gate is **GREEN
accepted**. Immutable receipt SHA-256
`A6D1A8E883E3A8F3890057AE4635502920826BB89E5CCEE25227A5FB173CC107`
records configure/build/prerequisite/CTest exit 0, independent 2/2, host 15/15 plus
compaction 30/30, failures 0/ignored 0 and no observed UBSan diagnostic. Native 6885
and the separate MSVC compile job ended naturally, active 0/stopped 0. The
[evidence record](../../docs/acceptance/ssa-host-primitive-layout.md) pins nine
current sources, five logs, three metadata files, host binary and MSVC object.
No new eight-suite consumer regression is claimed.
The exact-byte oracle proves omitted hash fields without pretending that local
TypeIds or runtime salts dynamically differ. Source inspection also confirms
the current primitive canonical hash byte-encodes PRIMITIVE kind/valueType with
fixed offset/prime and reserves node ID separately; this is not a claim about
nominal/generic hashes or future enums/schema. Existing packed-frame hashes do
not cover these row hashes; future AOT must retain the layout table. Row success
alone establishes neither source/callable provenance nor complete ABI/frame
geometry/native execution/retained artifact ownership.

Linux GCC/Clang retain the existing WSL service-denied blocker. The new MSVC TU
smoke remains compile-only. Full SSA47 stays OPEN. No network,
FFI, providers, security probes or cross-drive copies are executed.
