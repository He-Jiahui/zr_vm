# AOT 07-S3/S4 runtime inline-struct call/return registry layout resolver

## Scope

- Plan focus: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md` M1.5 / 07-S3/S4 and `docs/plans/aot/11-metadata.md` 11-S4G support.
- Production path:
  - `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c`
- Contract path:
  - `tests/parser/test_aot_c_return_contracts.c`

## Baseline

Before this slice, the generated runtime inline-struct call/return boundary helpers resolved generated-frame layouts through the prototype layout cache:

```c
ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function, typeLayoutId, state)
```

That affected:

- `ZrLibrary_AotRuntime_CallInlineStruct()`
- `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()`
- `ZrLibrary_AotRuntime_ReturnInlineStruct()`

## Result

- All three helpers now validate the inline struct destination/source layout through:

```c
ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, typeLayoutId)
```

- `test_aot_c_return_contracts.c` requires the metadata-runtime resolver and forbids the old prototype resolver in `aot_runtime_return.c`.
- The same contract now reflects the current frame thunk carrier ABI: generated frames receive thunk pointers from `codeRegistration->functionPointers` rather than the older descriptor thunk fields.

## RED

The first return-contract run exposed a stale ABI needle for `frame->functionThunks`. After updating the contract to the current code-registration carrier, the focused RED failed on the new missing resolver needle:

```c
ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function,
```

## GREEN

Validated after the production update:

- WSL gcc: return contracts 1/0, source contracts 22/0, value SemIR contracts 4/0, call shared-library smoke 5/0, value-type shared-library smoke 5/0.
- WSL clang: return contracts 1/0, source contracts 22/0, value SemIR contracts 4/0, call shared-library smoke 5/0, value-type shared-library smoke 5/0.
- Windows MSVC Debug: return contracts 1/0, source contracts 22/0, value SemIR contracts 4/0, call smoke 5/0/5 ignored, value-type smoke 5/0/1 ignored.

Additional source scan:

- `aot_runtime_return.c` contains three `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, ...)` lookups.
- `aot_runtime_return.c` has no `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function` match.

## Non-goals

- Field layout resolver `ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, ...)` is unchanged.
- Runtime generic layout construction, public reflection object materialization, cross-module token publication/rewrite, full byte-frame narrowing, GC roots/exports cleanup, and complete trim analysis remain open.
