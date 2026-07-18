# 2026-07-19 AOT 08-S6U / 10-S4Z42 / 11-S2E MethodSpec Method-Token VM-Function Resolution

## Scope

This cross-stage slice removes the caller-resolved-function requirement from local interpreter MethodSpec execution.
It resolves the MethodSpec's underlying MethodDef token through attached zrp metadata to the VM function represented by
the MethodDef `functionIndex`, then reuses the existing MethodSpec context execution boundary. It does not add a zrp
table, change code-registration ABI, synthesize a MethodSpec code slot, or resolve a method from another module.

## Baseline

- 08-S6R / 10-S4Z39 could execute a MethodSpec only when its caller supplied the exact VM function pointer.
- Attached zrp MethodDef rows already carried `functionIndex`, but metadata runtime had no interpreter binding view.
- AOT function indices are assigned by flattening root, constant-referenced functions, then child functions while
  deduplicating equivalent function identities. The interpreter needed the same ordering to consume MethodDef rows.

## Implementation

- `ZrCore_Function_ResolveGraphFunctionByFlatIndex()` performs the AOT-compatible depth-first traversal and identity
  deduplication. Constant and child references to the same function occupy one index.
- Visited storage starts small and grows only with actual distinct graph nodes. An untrusted large metadata index does
  not determine allocation size; allocation failure and exhausted graphs return null.
- `ZrCore_MetadataRuntime_ReadInterpreterMethodBindingView()` requires a unique local `MEMBER_DEF` MethodDef row, an
  existing method record, and an instruction-backed non-native VM function at the row's flat index. It clears output on
  every failure.
- `ZrCore_Reflection_InvokeInterpreterGenericMethodSpec()` reads the existing MethodSpec signature view, resolves its
  underlying method binding, and delegates to the S6R resolved-function path for GenericParam arity validation, context
  materialization, pinning, stack anchoring, argument staging, execution, and result restoration.
- Invalid MethodSpec/member tokens, wrong explicit arity, duplicate or missing MethodDef rows, out-of-range indices,
  `UINT32_MAX - 1`, native functions, and functions without instructions fail closed.

## RED / GREEN

- RED: after adding the public contracts and switching the positive execution test to automatic resolution, MSVC link
  failed on exactly two missing symbols: the interpreter MethodDef binding view and automatic MethodSpec invoke APIs.
- GREEN: implementing the binding and invoke path restored the focused dynamic reflection suite to 24/0.
- Review hardening added constant/child duplicate traversal, index 0/1/2, and near-maximum-index cases. The first dynamic
  growth implementation produced one GCC `-Wtype-limits` diagnostic for a 64-bit-only constant comparison; a
  narrow-address-space preprocessor guard removed it while retaining the 32-bit overflow check.

## Test Inventory

- `tests/module/test_reflection_dynamic_generic_instance.c`
- `tests/module/test_reflection_dynamic_generic_method_context.h`
- Focused CTest: `metadata_runtime_method_binding`, `reflection_token_resolve`,
  `metadata_runtime_binding_compatibility`, `metadata_runtime_typespec_layout`, and
  `reflection_dynamic_generic_instance`.
- Shared regressions: GC 66 cases, instruction execution 31 cases, and instruction table 95 cases.

## Tooling Evidence

- Isolated WSL source: `/tmp/zr_vm-aot-08-s6b-isolated-src`.
- WSL GCC 11.4 build: `/tmp/zr_vm-aot-08-s6b-isolated-gcc`.
- WSL Clang 14.0 build: `/tmp/zr_vm-aot-08-s6b-isolated-clang`.
- Windows MSVC 19.44 Debug build: `%TEMP%/zr_vm-aot-08-s6b-msvc-red` through `VsDevCmd.bat`.
- Shared direct execution was serialized because instruction execution uses a fixed relative import fixture path.
- A three-build parallel relink attempt exceeded its orchestration timeout; each build was resumed and confirmed
  successful before tests. This was a tool scheduling event, not a product failure.

## Results

- WSL GCC: focused CTest 5/5; GC 66/0; instruction execution 31/0; instruction table 95/0.
- WSL Clang: focused CTest 5/5; GC 66/0; instruction execution 31/0; instruction table 95/0.
- Windows MSVC: focused CTest 5/5; GC 66/0; instruction execution 31/0; instruction table 95/0.
- Final changed-source diagnostics: no GCC, Clang, or MSVC source warning/error. MSVC retained only the global command
  line notice that `/W4` overrides `/W3`.

## Acceptance Decision

Accepted as 08-S6U / 10-S4Z42 / 11-S2E. A local MethodSpec can now discover and execute its underlying interpreter VM
function from attached metadata using the same flat function-index semantics as AOT generation. Script-level
`MakeGenericMethod`, cross-module method binding, MethodSpec-specific AOT code slots, and full-AOT reflection closure
remain open, so full 08-S6, 10-S4, and 11-S2 are not declared complete.
