# Inline Frame GC And Native Entry Wiring

## Scope

This acceptance record covers the 2026-05-18 continuation of the inline struct byte-frame migration. The implemented slice connects existing function/prototype metadata to real runtime entry points without claiming the full VM payload or native ABI migration is complete.

In scope:

- runtime prototype-index `typeLayoutId` resolution into `SZrTypeLayout`
- layout-proven GC mark traversal for embedded inline-frame `SZrTypeValue` fields
- minor GC rewrite of forwarded embedded inline-frame values
- frame post-call and tail-call reuse drop of owned embedded inline-frame values
- resolved/prepared VM pre-call copy for already-inline caller frame payloads
- VM single-result post-call copy for already-inline callee return payloads into caller inline destinations
- conservative tail-call reuse fallback for callees with inline struct parameters
- native known-callback access to inline argument spans through the real `ZrLibCallContext`
- native inline argument span refresh after callback-triggered stack relocation in stable and generic native dispatch lanes
- native inline argument span rejection for inline frame slots that are not marked as parameters
- native ordinary argument reads rejected for inline struct parameters so callbacks must use the explicit span API
- known-native direct-binding call-context initialization keeps absent inline metadata explicitly zeroed

Out of scope:

- source-to-argument lowering plus full return/tail-call by-value struct payload movement beyond the already-inline single-result return hook and tail-reuse fallback
- source-level `%extern struct` lowering to a platform native ABI
- a serialized module/function `SZrTypeLayout` table independent of prototype metadata
- removal of boxed fallback paths

## Implementation Notes

`ZrCore_Function_ResolvePrototypeFrameTypeLayout` treats `typeLayoutId` as a checked prototype index for this increment. It validates the prototype data bounds and caches either a POD struct layout or a field-aware layout for embedded `SZrTypeValue` fields that require GC or ownership lifecycle handling. Non-lifecycle fields are no longer treated as POD merely because no lifecycle bit is set: primitive scalar fields must match the expected byte size and bounds, local nested struct fields reuse and flatten the nested prototype layout, builtin reference fields are accepted only when they are stored as a whole `SZrTypeValue`, and unknown/imported non-local field type names fail closed. Bad ids, malformed metadata, recursive resolution, unsafe managed field sizes, pointer-sized reference fields, and unresolved imported layouts also fail closed.

GC uses the VM frame local base, `callInfo->functionBase.valuePointer + 1`, when resolving byte-frame spans. Ordinary slot scanning skips storage units intersecting inline structs, then `ZrCore_Function_VisitInlineFrameGcValues` visits exactly the embedded value fields described by the layout. Minor GC rewrite uses the same layout visitor to update forwarded embedded values.

Frame teardown now calls the inline drop helper from the post-call paths. For already-inline single-result returns, post-call captures the callee metadata, copies the inline return span into the caller inline destination when both layouts resolve compatibly, then drops the captured callee frame. Tail-call reuse drops the old inline frame before copying non-inline calls into the reused base, clears inline storage units rather than interpreting raw inline bytes as ordinary `SZrTypeValue` slots, and refuses inline-parameter callees until a real overlapping layout move exists.

Resolved/prepared VM pre-call now routes callees with inline struct parameters away from the exact-args fast path, initializes the callee frame storage, then tries `ZrCore_Function_CopyInlineFrameParameters` using the previous VM call-info frame base as the caller inline frame. This covers the real call path when the caller argument payload already exists in an inline frame span. If caller metadata, frame layout, or resolver proof is missing, the hook returns without changing the existing boxed call behavior.

Native dispatch initializes `ZrLibCallContext` with the active VM metadata function, callable base, local inline frame base, and inline argument start slot. `ZrLib_CallContext_InlineArgumentSpan` is therefore available from a real known-native callback when the argument slot is already inline in the current VM frame layout and is marked as a parameter. Stack-root callbacks use the stack-layout anchor they already had; stable fast/inline-pinned lanes and the generic dispatcher fallback now adopt a lightweight inline-frame anchor so span lookup after callback-triggered stack relocation returns the relocated payload address without discarding stable argument copies. Inline struct parameters are also hidden from ordinary `ZrLib_CallContext_Argument` and typed `Read*` access, so native callbacks cannot mistake the inline payload for a boxed `SZrTypeValue`. Object known-native direct-binding lanes keep their local call-context copies layout-compatible with the public context and clear them before assigning fields, so no-frame-layout contexts continue to look like explicit absence. If the metadata or layout is missing, the slot is not an inline parameter, or the index is outside the native argument count, the span API reports unavailable and the previous boxed native behavior remains in force.

## Test Inventory

Focused tests:

- `tests/core/test_type_layout_inline_copy.c`
- `tests/gc/gc_tests.c`
- `tests/core/test_object_call_known_native_fast_path.c`
- `tests/core/test_native_inline_span_dispatch.c`
- `tests/core/test_tail_reuse_callinfo_reset.c`

Regression/smoke tests:

- `tests/core/test_precall_frame_slot_reset.c`
- `tests/core/test_tail_reuse_callinfo_reset.c`
- `tests/core/test_vm_closure_precall.c`
- WSL CLI `hello_world`

Covered boundary cases:

- POD prototype metadata resolves to raw-copy/no-drop inline layout.
- Primitive scalar fields keep POD layout only when their recorded size and bounds match the known scalar representation.
- Managed embedded value fields resolve to field-copy/field-drop/GC layout.
- Builtin reference fields stored as whole `SZrTypeValue` slots become GC value fields without an ownership metadata flag.
- GC-only embedded value fields are visited by GC/rewrite but are not released or cleared by inline drop.
- Pointer-sized builtin reference fields fail resolution instead of being treated as POD.
- Local nested struct fields flatten nested managed embedded value fields into parent field-copy/drop/GC layout.
- Unsafe managed field sizes fail resolution instead of scanning raw bytes.
- Unknown non-local/imported field type names fail resolution until a real serialized type-layout table exists.
- Recursive local struct metadata fails resolution instead of being treated as POD.
- Inline embedded object/string values are marked even when `stackTop` is below frame storage top.
- Minor GC rewrites a forwarded embedded inline-frame value.
- Post-call frame teardown drops owned embedded inline-frame values through the real call-info path.
- Tail-call frame reuse drops inline values before storage reuse and skips raw inline bytes during ordinary slot cleanup.
- Prepared/resolved VM pre-call bypasses the exact-args fast path for inline parameters and copies an already-inline caller frame payload into the callee parameter span.
- VM post-call copies an already-inline callee return payload into a compatible caller inline destination before dropping the callee frame.
- Tail-call frame reuse declines inline-parameter callees instead of raw-copying their argument bytes through the old reuse path.
- Real known-native callback code can request an inline argument span from its dispatch context.
- Native callbacks that grow/relocate the stack before requesting a span still receive the relocated inline frame payload address on stable fast-lane and generic dispatcher fallback paths.
- Inline locals that share an argument index-shaped slot are rejected unless the frame slot layout is explicitly marked as a parameter.
- Ordinary native argument reads return unavailable for inline struct parameters even when stable argument copies exist, keeping the explicit span API as the only native access path for inline payload bytes.
- Known-native direct-binding fast paths with no inline frame metadata keep ordinary boxed argument access stable instead of reading uninitialized inline-frame fields.
- Missing inline metadata or plain value frame slots report span unavailable while preserving ordinary boxed `Argument`/`ReadInt` access.

## Tooling Evidence

Resolver nested-layout red run before the production resolver change:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test -j 4
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
```

Observed red result:

- `test_function_prototype_type_layout_resolver_flattens_nested_managed_struct_fields`: `FAIL: Expected 1 Was 0`
- `test_function_prototype_type_layout_resolver_fails_recursive_struct_metadata`: `FAIL: Expected NULL`
- Suite summary: `19 Tests 2 Failures 0 Ignored`

WSL GCC focused build:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_precall_frame_slot_reset_test zr_vm_gc_test zr_vm_object_call_known_native_fast_path_test zr_vm_native_inline_span_dispatch_test zr_vm_vm_closure_precall_test zr_vm_compiler_integration_test zr_vm_module_system_test zr_vm_cli -j 4
```

WSL GCC focused tests:

```bash
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-gcc-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test
./build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-gcc-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-gcc-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_module_system_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed WSL GCC focused results:

- build target set above: exit 0
- `zr_vm_type_layout_inline_copy_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`

During the first fresh GCC validation after adding span-only ordinary argument rejection, `zr_vm_object_call_known_native_fast_path_test` crashed in `ZrCore_Function_FindFrameSlotLayout`. `gdb` showed `ZrLib_CallContext_Argument` reading an invalid `inlineFrameFunction` from an object direct-binding fast-path context. The root cause was a local shadow `ZrLibCallContext` definition and direct-binding initializer path that had not been updated for the newly added inline frame fields. The fix updates that local layout and clears direct-binding contexts before field assignment in both object-call and index-contract fast paths. Re-running `zr_vm_object_call_known_native_fast_path_test` then reported `59 Tests 0 Failures 0 Ignored`.

WSL GCC regression/smoke results:

```bash
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_module_system_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed WSL GCC regression/smoke results:

- `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`
- CLI smoke: `hello world`

WSL Clang focused build:

```bash
cmake --build build/codex-wsl-clang-debug --target zr_vm_type_layout_inline_copy_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_precall_frame_slot_reset_test zr_vm_gc_test zr_vm_object_call_known_native_fast_path_test zr_vm_native_inline_span_dispatch_test zr_vm_vm_closure_precall_test zr_vm_compiler_integration_test zr_vm_module_system_test zr_vm_cli -j 4
```

Observed WSL Clang focused results:

- build target set above: exit 0
- `zr_vm_type_layout_inline_copy_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`
- CLI smoke: `hello world`

After adding nested resolver flattening and recursive-layout failure, the focused resolver/GC/native/runtime targets were rechecked directly under both WSL toolchains:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test zr_vm_gc_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_native_inline_span_dispatch_test zr_vm_object_call_known_native_fast_path_test zr_vm_cli -j 4
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
cmake --build build/codex-wsl-clang-debug --target zr_vm_type_layout_inline_copy_test zr_vm_gc_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_native_inline_span_dispatch_test zr_vm_object_call_known_native_fast_path_test zr_vm_cli -j 4
./build/codex-wsl-clang-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-clang-debug/bin/zr_vm_gc_test
./build/codex-wsl-clang-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-clang-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed nested-layout resolver recheck results:

- WSL GCC and WSL Clang build targets: exit 0
- `zr_vm_type_layout_inline_copy_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- CLI smoke: `hello world`

The compiler/module/closure regression targets were also rechecked after the resolver change:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_vm_closure_precall_test zr_vm_compiler_integration_test zr_vm_module_system_test -j 4
cmake --build build/codex-wsl-clang-debug --target zr_vm_vm_closure_precall_test zr_vm_compiler_integration_test zr_vm_module_system_test -j 4
./build/codex-wsl-gcc-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_module_system_test
./build/codex-wsl-clang-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-clang-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-clang-debug/bin/zr_vm_module_system_test
```

Observed regression recheck results:

- WSL GCC and WSL Clang build targets: exit 0
- `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`

Running GCC and Clang compiler/module tests concurrently produced inconsistent temporary failures in binary metadata/reflection cases. Serial reruns for each toolchain restored `101 Tests 0 Failures` for compiler integration and `87 Tests 0 Failures` for module system, so the accepted evidence uses serial runs for these fixture-writing suites.

After adding the native fallback cases for missing inline metadata and plain value frame slots, the native span target was rechecked directly under both WSL toolchains:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_native_inline_span_dispatch_test -j 4
./build/codex-wsl-gcc-debug/bin/zr_vm_native_inline_span_dispatch_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_native_inline_span_dispatch_test -j 4
./build/codex-wsl-clang-debug/bin/zr_vm_native_inline_span_dispatch_test
```

Observed native fallback recheck results:

- WSL GCC build target: exit 0; `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- WSL Clang build target: exit 0; `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`

After tightening non-lifecycle field classification, the new resolver tests were first run red under WSL GCC:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
```

Observed red result:

- `test_function_prototype_type_layout_resolver_builds_gc_value_field_for_value_sized_reference_type`: `FAIL: Expected 1 Was 0`
- `test_function_prototype_type_layout_resolver_fails_reference_pointer_field_without_value_layout`: `FAIL: Expected NULL`
- `test_function_prototype_type_layout_resolver_fails_unknown_nonlocal_field_metadata`: `FAIL: Expected NULL`
- Suite summary: `23 Tests 3 Failures 0 Ignored`

After implementing the stricter classification, the resolver and related runtime targets were rechecked under both WSL toolchains:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_type_layout_inline_copy_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_type_layout_inline_copy_test
cmake --build build/codex-wsl-gcc-debug --target zr_vm_gc_test zr_vm_native_inline_span_dispatch_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_object_call_known_native_fast_path_test zr_vm_cli_executable -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test
./build/codex-wsl-gcc-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
cmake --build build/codex-wsl-clang-debug --target zr_vm_gc_test zr_vm_native_inline_span_dispatch_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_object_call_known_native_fast_path_test zr_vm_cli_executable -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_gc_test
./build/codex-wsl-clang-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-clang-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed strict-classification recheck results:

- WSL GCC and WSL Clang `zr_vm_type_layout_inline_copy_test`: `23 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang CLI smoke: `hello world`

After adding value-sized builtin reference fields, the inline drop semantics were tightened so GC-only value fields are not treated as owned fields. The new test was first run red under WSL GCC:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
```

Observed red result:

- `test_gc_only_value_field_drop_keeps_non_owned_reference_value`: `FAIL: Expected 12 Was 0`
- Suite summary: `24 Tests 1 Failures 0 Ignored`

After making `ZrCore_TypeLayout_DropInline` release only fields marked `ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE`, the resolver/drop and related runtime targets were rechecked under both WSL toolchains:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_type_layout_inline_copy_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_type_layout_inline_copy_test
cmake --build build/codex-wsl-gcc-debug --target zr_vm_gc_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_precall_frame_slot_reset_test zr_vm_native_inline_span_dispatch_test zr_vm_object_call_known_native_fast_path_test zr_vm_cli_executable -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test
./build/codex-wsl-gcc-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
cmake --build build/codex-wsl-clang-debug --target zr_vm_gc_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_precall_frame_slot_reset_test zr_vm_native_inline_span_dispatch_test zr_vm_object_call_known_native_fast_path_test zr_vm_cli_executable -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_gc_test
./build/codex-wsl-clang-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-clang-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed GC-only drop recheck results:

- WSL GCC and WSL Clang `zr_vm_type_layout_inline_copy_test`: `24 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- WSL GCC and WSL Clang CLI smoke: `hello world`

2026-05-21 continuation revalidation after the actual payload migration slice was inspected end to end:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_type_layout_inline_copy_test zr_vm_native_inline_span_dispatch_test -j 2
./build/codex-wsl-gcc-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-gcc-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-gcc-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-gcc-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_module_system_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_type_layout_inline_copy_test zr_vm_native_inline_span_dispatch_test zr_vm_precall_frame_slot_reset_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_vm_closure_precall_test zr_vm_gc_test zr_vm_compiler_integration_test zr_vm_module_system_test -j 2
./build/codex-wsl-clang-debug/bin/zr_vm_type_layout_inline_copy_test
./build/codex-wsl-clang-debug/bin/zr_vm_native_inline_span_dispatch_test
./build/codex-wsl-clang-debug/bin/zr_vm_precall_frame_slot_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_tail_reuse_callinfo_reset_test
./build/codex-wsl-clang-debug/bin/zr_vm_vm_closure_precall_test
./build/codex-wsl-clang-debug/bin/zr_vm_gc_test
./build/codex-wsl-clang-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-clang-debug/bin/zr_vm_module_system_test
cmake --build build/codex-wsl-gcc-debug --target zr_vm_object_call_known_native_fast_path_test zr_vm_cli_executable -j 2
./build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
cmake --build build/codex-wsl-clang-debug --target zr_vm_object_call_known_native_fast_path_test zr_vm_cli_executable -j 2
./build/codex-wsl-clang-debug/bin/zr_vm_object_call_known_native_fast_path_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Observed 2026-05-21 revalidation results:

- WSL GCC `zr_vm_type_layout_inline_copy_test`: `24 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`
- WSL GCC `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- WSL GCC CLI smoke: `hello world`
- WSL Clang `zr_vm_type_layout_inline_copy_test`: `24 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_native_inline_span_dispatch_test`: `6 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_precall_frame_slot_reset_test`: `12 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_tail_reuse_callinfo_reset_test`: `4 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_vm_closure_precall_test`: `6 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_gc_test`: `65 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_compiler_integration_test`: `101 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_module_system_test`: `87 Tests 0 Failures 0 Ignored`
- WSL Clang `zr_vm_object_call_known_native_fast_path_test`: `59 Tests 0 Failures 0 Ignored`
- WSL Clang CLI smoke: `hello world`

Windows MSVC CLI smoke:

```powershell
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

Observed Windows MSVC result:

- build target: exit 0
- CLI smoke: `hello world`

The WSL launcher printed its existing localhost/NAT warning noise, and the compiler integration suite printed expected negative-case compiler diagnostics. The Clang build also emitted unused-function warnings for existing object-call scratch staging helpers. Commands returned exit 0 with passing Unity summaries.

## Acceptance Decision

Accepted for this GC/drop/native entry wiring slice plus the first real VM pre-call inline payload movement hook, the already-inline single-result post-call return hook, the conservative tail-reuse fallback for inline-parameter callees, native inline span refresh across callback-triggered stack relocation, native span rejection for non-parameter inline slots, ordinary native argument-read rejection for inline struct parameters, boxed argument preservation when inline metadata is missing or the frame slot is a plain value, and prototype resolver classification for primitive POD fields, value-sized builtin reference fields, local nested struct managed-field flattening, recursive layout failure, pointer-sized reference failure, unknown non-local/imported field failure, and drop behavior that releases only ownership-marked embedded value fields.

The acceptance boundary remains explicit: GC/drop/native entry points, already-inline VM pre-call payload copy, already-inline single-result return copy, tail-reuse refusal for inline parameters, native inline-span relocation refresh, non-parameter rejection, span-only native access for inline parameters, boxed fallback for no-layout/plain-value arguments, primitive/value-sized-reference/local-nested prototype resolver proof, ownership-only inline drop release, and fail-closed handling for pointer-sized references or unknown non-local fields are genuinely wired for layout-proven inline frame payloads, but source-to-argument lowering, the full return/tail by-value payload model, imported/serialized standalone type-layout tables, and native platform ABI marshaling are still future work.
