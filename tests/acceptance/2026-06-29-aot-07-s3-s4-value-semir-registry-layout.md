# AOT 07-S3/S4 value SemIR registry layout resolver

## Scope

- Plan focus: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md` M1.5 / 07-S3/S4 and `docs/plans/aot/11-metadata.md` 11-S4G support.
- Production paths:
  - `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c`
  - `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c`
- Contract paths:
  - `tests/parser/test_aot_c_source_contracts.c`
  - `tests/parser/test_aot_c_value_semir_contracts.c`

## Baseline

Before this slice, generated inline `COPY_VALUE` and nested inline-struct field load/store resolved `SZrTypeLayout` through:

```c
ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, typeLayoutId, state)
```

That kept value SemIR copy/field helper code on the function prototype layout cache even after 11-S4G introduced `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()` for attached AOT code-registration layout registries.

## Result

- Inline `COPY_VALUE` generated C now resolves type layout with:

```c
ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, typeLayoutId)
```

- Nested inline-struct field load/store generated C now uses the same metadata-runtime resolver before choosing POD `memmove` or non-POD `ZrCore_TypeLayout_CopyInline`.
- `test_aot_c_source_contracts.c` and `test_aot_c_value_semir_contracts.c` lock the new resolver and forbid the old prototype type-layout resolver in these helper paths.
- Generated artifact inspection confirms value-type generated C uses the metadata-runtime resolver and does not contain `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function` for these lookups.

## RED

WSL gcc source/value SemIR contracts were expanded first. The RED run failed in `zr_vm_aot_c_source_contracts_test` because the generated source did not yet contain:

```c
ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function
```

## GREEN

Validated after the production update:

- WSL gcc: source contracts 22/0, value SemIR contracts 4/0, value-type shared-library smoke 5/0.
- WSL clang: source contracts 22/0, value SemIR contracts 4/0, value-type shared-library smoke 5/0.
- Windows MSVC Debug: source contracts 22/0, value SemIR contracts 4/0, value-type smoke 5/0/1 ignored.

Additional generated artifact check:

- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/*.c` contains `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(...)` for the value-type layout lookups covered by this slice.
- The same generated artifact set has no `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function` matches.

## Non-goals

- Field layout resolver `ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, ...)` is unchanged.
- Typed call/return layout resolver migration is not closed by this slice.
- Runtime generic layout construction, public reflection object materialization, cross-module token publication/rewrite, full byte-frame narrowing, GC roots/exports cleanup, and complete trim analysis remain open.
