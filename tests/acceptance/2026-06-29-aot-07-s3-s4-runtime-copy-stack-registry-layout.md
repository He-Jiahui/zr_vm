# AOT 07-S3/S4 runtime CopyStack registry layout resolver

## Scope

- Plan focus: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md` M1.5 / 07-S3/S4 and `docs/plans/aot/11-metadata.md` 11-S4G support.
- Production path:
  - `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`
- Contract path:
  - `tests/parser/test_aot_c_source_contracts.c`

## Baseline

Before this slice, the `ZrLibrary_AotRuntime_CopyStack()` inline-struct stack-copy fallback resolved generated-frame layouts through the prototype layout cache:

```c
ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function, destinationLayout->typeLayoutId, state)
```

That kept the runtime helper on prototype metadata even after 11-S4G introduced the function-level code-registration layout registry resolver.

## Result

- `ZrLibrary_AotRuntime_CopyStack()` now validates the inline struct source/destination fallback layout through:

```c
ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, destinationLayout->typeLayoutId)
```

- `aot_runtime_values.c` includes `zr_vm_core/metadata_runtime.h`.
- `test_aot_c_source_contracts.c` requires that runtime include and resolver, and forbids the old prototype resolver in `aot_runtime_values.c`.

## RED

WSL gcc source contracts were expanded first. The RED run failed because `aot_runtime_values.c` did not yet contain:

```c
#include "zr_vm_core/metadata_runtime.h"
```

or the expected resolver:

```c
ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function,
```

## GREEN

Validated after the production update:

- WSL gcc: source contracts 22/0, call shared-library smoke 5/0, value-type shared-library smoke 5/0.
- WSL clang: source contracts 22/0, call shared-library smoke 5/0, value-type shared-library smoke 5/0.
- Windows MSVC Debug: source contracts 22/0, call smoke 5/0/5 ignored, value-type smoke 5/0/1 ignored.

## Non-goals

- Field layout resolver `ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, ...)` is unchanged.
- Object-to-inline copy, value-slot copy, and materialized stack-value assignment semantics remain owned by `ZrLibrary_AotRuntime_CopyStack()`.
- Runtime generic layout construction, public reflection object materialization, cross-module token publication/rewrite, full byte-frame narrowing, GC roots/exports cleanup, and complete trim analysis remain open.
