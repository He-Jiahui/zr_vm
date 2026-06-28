# 2026-06-28 AOT 11-S6D i64 Typed Direct-Call Metadata Guard/Deopt

Status: completed support sub-slice for 11-S6. Full 11-S6 remains open for bool/u64/f64 typed-boundary drift deopt, inline-struct writeback, cross-module token resolve integration, and broader no-crash ABI drift injection.

Completed items:
- Added `ZrLibrary_AotRuntime_CanUseTypedDirectCall()` to check caller and callee `moduleMetadataBindings` before a generated typed direct call enters an i64 scalar thunk.
- Added `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` as the runtime fallback boundary; it delegates to `CallStackValue()` and lets generated code sync the i64 scalar local from the VM slot.
- Updated i64 no/one/two/three-arg typed direct-call generated C to emit metadata guard markers and preserve the direct state-free thunk path only when caller/callee bindings are compatible.
- Threaded the source function slot through typed direct-call dispatch and writer prototypes so the deopt helper can call the original function slot.
- Added focused runtime guard tests, source-contract coverage, and reused the existing i64 typed direct-call shared-library smoke as the direct-path regression.

RED:
- `tests/module/test_aot_runtime_typed_direct_call_compatibility.c` first referenced the missing typed direct-call guard API. WSL gcc reported implicit declaration warnings and link errors for `ZrLibrary_AotRuntime_CanUseTypedDirectCall`.

GREEN:
- Runtime guard test covers empty caller/callee bindings as compatible, caller binding drift as deopt, and callee binding drift as deopt.
- Source contract verifies the runtime guard/deopt API, `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()`, `CallStackValue()` delegation, i64 generated guard markers, and scalar local sync.
- Existing i64 typed direct-call shared-library smoke continues to validate the compatible direct-call path.

Validation:
- WSL gcc: runtime guard 3/0, call contracts 5/0, typed direct-call smoke 5/0, CTest `aot_runtime_typed_direct_call_compatibility` 1/1.
- WSL clang: runtime guard 3/0, call contracts 5/0, typed direct-call smoke 5/0, CTest `aot_runtime_typed_direct_call_compatibility` 1/1.
- Windows MSVC Debug: runtime guard 3/0, call contracts 5/0, typed direct-call smoke 0 failures / 5 ignored through the existing Unix shared-library branch, CTest `aot_runtime_typed_direct_call_compatibility` 1/1.

Notes:
- This slice intentionally covers i64 scalar typed direct-call guard/deopt only.
- Clang/MSVC still report existing project const qualifier and `aot_runtime.c` unreachable/size_t warnings unrelated to this slice.
