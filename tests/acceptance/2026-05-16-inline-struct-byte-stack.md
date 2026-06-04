# Inline Struct Byte Stack Migration

## Scope

This acceptance record covers the current 2026-05-16 increment of the inline struct stack plan:

- core runtime layout descriptors and byte-offset stack copy helpers from the earlier slice
- struct prototype whole-value `layoutByteSize` and `layoutByteAlign` metadata from the earlier slice
- new function frame byte-layout sidecar metadata on `SZrFunction`
- `.zro` writer, reader, and runtime-load preservation of that frame metadata
- typed stack frame places for resolving frame-relative byte offsets into checked stack byte spans
- function-level frame slot place/copy helpers over `SZrFunctionFrameSlotLayout`
- VM precall reservation of enough legacy stack allocation units to cover `frameByteSize`

Affected layers:

- parser/compiler function metadata
- core runtime function metadata
- core runtime stack place helpers
- core runtime function frame place helpers
- core runtime VM precall and tail-call frame reuse sizing
- binary IO format and runtime IO loading
- focused compiler integration tests
- core runtime inline copy tests

This increment does not switch VM instruction operand access, actual argument payload movement, return payload movement, GC frame scanning, or native ABI marshaling to the byte-frame ABI yet. The existing fixed-slot execution stack remains active, but VM call setup now reserves enough stack storage units for the sidecar byte frame.

2026-05-18 continuation note: the original GC/native boundary above has since been narrowed by `tests/acceptance/2026-05-18-inline-frame-gc-native-entry.md`. GC mark/rewrite, post-call/tail-reuse inline drop, native inline span access with stack-relocation refresh, span-only native access for inline struct parameters, the first resolved/prepared VM pre-call copy for already-inline parameter payloads, already-inline single-result post-call return copy, and conservative tail-reuse fallback for inline-parameter callees are now wired for layout-proven inline frame payloads. Source-to-argument lowering, the full return/tail by-value payload model, and platform native ABI marshaling remain out of scope.

## Baseline

The repository already had unrelated dirty files and known broader test failures before this slice. This work only treats the focused layout, IO, and compiler integration checks as acceptance gates.

The TDD red check was run before adding the frame metadata implementation. Building `zr_vm_compiler_integration_test` failed as expected because the test referenced the not-yet-existing frame layout API:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_integration_test -j 8
```

Expected red symptoms:

- unknown type `SZrFunctionFrameSlotLayout`
- `SZrFunction` missing `frameSlotLayouts`, `frameSlotLayoutLength`, `frameByteAlign`, and `frameByteSize`
- missing `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT` and `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`

During the first green attempt, `test_function_frame_layout_metadata_marks_struct_parameter_inline` exposed a real bug: the inline-struct lookup helper cleared the caller's default `byteSize` to `0` for non-struct value slots. `gdb` showed slot 0 as the inline struct span and slots 1 and 2 as zero-byte value slots. The helper now only writes output values after a struct prototype match.

During Clang validation, inserting the new frame layout fields in the middle of `SZrFunction` shifted existing field offsets and produced bad reads of existing runtime data such as call-site caches and constant values. Moving the frame layout fields to the end of `SZrFunction` fixed the Clang-only crashes while preserving the same metadata behavior. The sidecar is now treated as append-only ABI state.

The 2026-05-17 continuation added typed stack frame places after the metadata layer passed. Its red check failed because `SZrStackFramePlace`, `ZrCore_Stack_MakeFramePlace`, and `ZrCore_Stack_CopyInlinePlace` did not exist yet. The implementation now resolves frame-base-relative byte offsets to checked stack byte spans and copies through the existing layout-aware inline copy path.

The next 2026-05-17 red check added function-level frame slot place/copy tests. The focused build failed with undefined references to `ZrCore_Function_MakeFrameSlotPlace` and `ZrCore_Function_CopyFrameSlotInline`, proving the test was covering missing API rather than existing behavior. The implementation now resolves places from `SZrFunctionFrameSlotLayout` and rejects struct copies into legacy value slots.

The following precall red check added functions whose `frameByteSize` required more legacy allocation units than `stackSize`. `zr_vm_precall_frame_slot_reset_test` failed because `functionTop` still used `stackSize`, and the prepared exact-args probe returned a call info instead of falling back to byte-frame-aware setup. VM precall now uses `ZrCore_Function_GetFrameStorageSlotCount`, resets extra storage slots to null, and makes the prepared steady-state probe fall back when a byte frame needs more storage than the old logical slot count.

## Test Inventory

Focused unit and subsystem cases:

- `tests/core/test_type_layout_inline_copy.c`
- `tests/core/test_precall_frame_slot_reset.c`
- `test_struct_prototype_metadata_serializes_layout_size_and_align`
- `test_function_frame_layout_metadata_marks_struct_parameter_inline`
- `test_binary_roundtrip_preserves_function_frame_layout_metadata`

Integration and project cases:

- `zr_vm_compiler_integration_test`
- `zr_vm_module_system_test`
- WSL CLI `hello_world` smoke
- Windows MSVC CLI `hello_world` smoke

Boundary cases covered in this increment:

- struct parameter slot uses whole-struct byte size and alignment instead of `sizeof(SZrTypeValue)`
- ordinary local value slot remains `sizeof(SZrTypeValue)` after an inline struct slot
- frame byte offsets are deterministic and aligned
- stack frame place resolution preserves absolute stack byte offsets and rejects frame-relative misalignment
- stack frame place copy rejects destination/source spans smaller than the layout size
- function frame slot place lookup rejects missing logical slots
- function frame slot inline copy rejects struct layouts targeting legacy value slots
- VM precall rounds `frameByteSize` up to `SZrTypeValueOnStack` storage units for `functionTop`
- byte-frame padding slots beyond logical `stackSize` are reset to null during call setup
- prepared exact-args fast probe falls back when byte-frame storage exceeds logical `stackSize`
- `.zro` roundtrip preserves child function frame layout metadata
- older binary compatibility is gated by `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT`

Not covered yet:

- byte-frame execution for call/return/tail-call
- actual by-value struct argument payload movement during VM calls
- inline struct GC traversal and drop during frame teardown
- native by-value struct ABI marshaling from inline spans
- `.zri` textual frame layout emission

## Tooling Evidence

WSL GCC focused build and test:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_integration_test -j 2
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
```

Observed result after the frame metadata and roundtrip fixes:

- `101 Tests 0 Failures 0 Ignored`

WSL `gdb` was used against `build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test` to inspect the failed frame layout before fixing the value-slot zero-size bug:

```bash
gdb -q -x build/codex-wsl-gcc-debug/frame-layout-debug.gdb
```

Key observed values before the fix:

- `frameSlotLayouts[0]`: inline struct, `byteSize = 64`, `byteAlign = 32`
- `frameSlotLayouts[1]`: value slot, `byteSize = 0`
- `frameSlotLayouts[2]`: value slot, `byteSize = 0`

The remaining matrix validation commands and results are added below as they are run.

WSL GCC focused matrix:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_integration_test zr_vm_module_system_test zr_vm_type_layout_inline_copy_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_vm_closure_precall_test -j 2
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_module_system_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed results:

- type layout inline copy: `9 Tests 0 Failures 0 Ignored`
- precall frame slot reset: `12 Tests 0 Failures 0 Ignored`
- tail reuse callinfo reset: `2 Tests 0 Failures 0 Ignored`
- VM closure precall: `6 Tests 0 Failures 0 Ignored`
- compiler integration: `101 Tests 0 Failures 0 Ignored`
- module system: `87 Tests 0 Failures 0 Ignored`
- CLI smoke: `hello world`

WSL Clang focused matrix:

```bash
cmake --build build/codex-wsl-clang-debug --target zr_vm_compiler_integration_test zr_vm_module_system_test zr_vm_type_layout_inline_copy_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_vm_closure_precall_test -j 2
./build/codex-wsl-clang-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-clang-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-clang-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-clang-debug/bin/zr_vm_module_system_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed results:

- type layout inline copy: `9 Tests 0 Failures 0 Ignored`
- precall frame slot reset: `12 Tests 0 Failures 0 Ignored`
- tail reuse callinfo reset: `2 Tests 0 Failures 0 Ignored`
- VM closure precall: `6 Tests 0 Failures 0 Ignored`
- compiler integration: `101 Tests 0 Failures 0 Ignored`
- module system: `87 Tests 0 Failures 0 Ignored`
- CLI smoke: `hello world`

WSL Clang `gdb` evidence from the offset-shift triage before the append-only fix:

```bash
gdb -q -x build/codex-wsl-clang-debug/frame-layout-clang-compiler-crash.gdb
```

Key backtrace:

- runtime raises `SUPER_META_CALL_CACHED: invalid callsite cache`
- exception stack-frame capture then dereferences invalid raw object pointer `0x1300000020`
- crashing test: `test_ownership_release_preserves_unrelated_stack_values_after_weak_expiry`

WSL Clang `gdb` evidence for module system:

```bash
gdb -q -x build/codex-wsl-clang-debug/frame-layout-clang-module-crash.gdb
```

Key backtrace:

- `execution_function_from_constant_value`
- `execution_find_entry_function_in_constants`
- `materialize_entry_function_prototypes_for_lookup`
- crashing test: `test_source_module_preinstalled_callable_preserves_imported_module_captures_after_native_imports`

Both crashes disappeared after moving the new frame layout fields to the end of `SZrFunction`, confirming the root cause was field-offset perturbation rather than frame metadata contents. Valgrind could not be used on the Clang binary because its debuginfo reader aborted on the executable's DWARF form data before the test ran.

Windows MSVC CLI smoke:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

Observed result:

- CLI smoke: `hello world`

Whitespace check:

```powershell
git diff --check -- docs/core-runtime/index.md docs/core-runtime/inline-type-layout-and-byte-stack.md tests/acceptance/2026-05-16-inline-struct-byte-stack.md tests/parser/test_compiler_features.c tests/parser/test_compiler_integration_main.c zr_vm_common/include/zr_vm_common/zr_io_conf.h zr_vm_common/include/zr_vm_common/zr_version_info.h zr_vm_core/include/zr_vm_core/function.h zr_vm_core/src/zr_vm_core/function.c zr_vm_core/include/zr_vm_core/io.h zr_vm_core/src/zr_vm_core/io.c zr_vm_core/src/zr_vm_core/io_runtime.c zr_vm_parser/src/zr_vm_parser/writer.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_meta_function.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
```

Observed result:

- no whitespace errors; Git reported only repository line-ending normalization warnings.

## Results

Passed:

- WSL GCC `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_type_layout_inline_copy_test`: `9 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_tail_reuse_callinfo_reset_test`: `2 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`
- WSL GCC CLI smoke: `hello world`
- WSL Clang `zr_vm_type_layout_inline_copy_test`: `9 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_tail_reuse_callinfo_reset_test`: `2 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`
- WSL Clang CLI smoke: `hello world`
- Windows MSVC CLI smoke: `hello world`
- `git diff --check` on touched files: no whitespace errors

Fixes made in response to evidence:

- Added `SZrFunctionFrameSlotLayout` and frame byte summary fields to `SZrFunction`.
- Added frame layout allocation, freeing, detaching, and lookup support.
- Added binary IO structs and gated read/write/copy support for frame layout metadata.
- Added compiler frame layout construction from typed local bindings and struct prototype layout metadata.
- Fixed non-struct slots being accidentally written as zero-byte layouts.
- Kept the new `SZrFunction` frame layout fields at the end of the struct so existing field offsets remain stable while native fixtures and copied graphs still observe the public function ABI.
- Added a `.zro` roundtrip test for runtime-loaded child function frame metadata.
- Added `SZrStackFramePlace` and stack helpers that resolve frame-relative byte offsets and copy layout-sized inline spans from checked places.
- Added function-level frame slot place/copy helpers backed by `SZrFunctionFrameSlotLayout`.
- Added byte-frame storage slot sizing for VM precall and tail-call frame reuse.
- Added null reset of padding storage slots beyond logical `stackSize`.

## Acceptance Decision

Accepted for this metadata, typed-place, and byte-frame precall reservation increment. WSL GCC and WSL Clang both pass the focused compiler, module, precall, and inline-copy suites, and Windows MSVC passes the CLI smoke. The byte-frame execution payload migration remains future work.

Remaining risk after this increment:

- The 2026-05-16 slice was metadata plumbing only. The 2026-05-18 continuation wires GC/drop/native entry points, span-only native access for inline struct parameters, and already-inline VM pre-call payload copy for layout-proven inline frame payloads, but source-to-argument lowering, return/tail payload movement, and native ABI marshaling remain future work.
