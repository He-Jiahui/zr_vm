---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
plan_sources:
  - user: 2026-07-20 严格执行 LSP semantic inference 计划并逐子里程碑记录产出
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/language_server/descriptor_plugin_fixture_float.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_callable_signature_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# LSP Native Descriptor Function Callable Contract

## Purpose

Native module functions already expose structured `ZrLibFunctionDescriptor` metadata through the native registry and LSP metadata provider. Before this change, semantic query could resolve the exact module and function descriptor, but signature help discarded parameter names and documentation by formatting only inferred argument types. Hover used another projection and could display only a broad resolved type.

The external callable contract adapter makes the resolved descriptor the single display source for native module functions. Hover and signature help now consume the same name, generic parameter list, ordered parameters, return type and documentation without searching by member text or reconstructing a callable from the AST.

## Data Flow

```text
callee identifier range
  -> LspSemanticQuery_ResolveAtPosition
  -> ModuleIdentity + provider generation + resolved metadata member
  -> ZrLibFunctionDescriptor
  -> LspExternalCallableContract
     -> canonical function label
     -> ordered parameter labels and descriptor documentation
     -> hover and signatureHelp protocol projections
```

`lsp_signature_help.c` derives the exact member identifier range from the parsed call context. `lsp_external_callable_signature_help.c` resolves that position through `SZrLspSemanticQuery`; it does not accept a caller-supplied member name. Only `NATIVE_BUILTIN` and `NATIVE_DESCRIPTOR_PLUGIN` module functions with `memberKind == FUNCTION` enter this path.

## Contract Formatting

`ZrLanguageServer_LspExternalCallableContract_FromResolvedMember` borrows the immutable descriptor fields from the resolved metadata member. The formatter emits:

```text
name<genericParameters>(parameterName: parameterType, ...): returnType
```

Parameter information uses the same ordered descriptor array. Descriptor parameter documentation is preserved and may be followed by argument-specific semantic fact documentation, such as numeric ranges or ownership facts. Active parameter selection remains based on the parsed function-call argument range.

Hover calls the same formatter and adds provider source and callable documentation around that exact label. A descriptor-plugin reload therefore changes both features together when the provider generation publishes a new descriptor.

## Availability And Fallback Rules

The signature resolver returns three states:

- `NOT_EXTERNAL`: the target is not an in-scope native module function, so existing source, binary and receiver callable consumers may continue.
- `RESOLVED`: a complete structured descriptor contract produced signature help.
- `UNAVAILABLE`: semantic query resolved an in-scope native function, but required descriptor fields or formatting were unavailable. The caller returns no signature instead of falling back to member-name or AST-text inference.

Unknown return or parameter types are not rewritten as `any`. Generic descriptors are formatted only when their structured names are available. Constraint-bearing generic descriptors currently return unavailable because this adapter does not yet have a canonical constraint formatter.

## Receiver Method Boundary

Native receiver methods are intentionally excluded. Existing method projection performs closed generic substitution, for example replacing a descriptor owner's `T` with `int`. Formatting a raw `ZrLibMethodDescriptor` here would regress that contract to an open generic label. Methods remain on the established receiver consumer until semantic query publishes the closed structured callable descriptor needed by this adapter.

This boundary also excludes constructors, meta-members and properties. They require their own query kinds and effect/access contracts rather than being treated as module functions.

## Snapshot And Generation Behavior

This implementation does not add a new snapshot or descriptor schema. The external consumer obtains the descriptor only from the current `SZrLspSemanticQuery`, whose module resolution selects the current provider generation. The descriptor-plugin project test replaces an integer plugin with a floating-point plugin, reloads the owning project and re-queries hover and signature help; both labels must change from `int` to `float`.

No descriptor pointer is cached by the new adapter. The contract is stack-local and is consumed before the semantic query is freed.

## Test Coverage

The project fixture defines `combine(left, right)` in integer and floating-point descriptor plugins. Its test fixes:

- exact ModuleIdentity, provider source kind and function descriptor identity;
- exact `combine(left: int, right: int): int` label;
- parameter labels, descriptor documentation and active parameter 1;
- the same label in hover;
- provider reload to `combine(left: float, right: float): float` without stale descriptor reuse.
- an `incomplete_callable(): unknown` descriptor returns no signature help and cannot fall through to AST or member-name reconstruction.

The stdio smoke opens a real document importing `zr.system.gc` and checks `set_budget(microseconds: int): null` through both `textDocument/signatureHelp` and `textDocument/hover`, including parameter documentation and didClose cleanup. This exercises the builtin provider through protocol serialization without plugin-specific CMake arguments.

## Open Work

- Publish closed structured contracts for native receiver methods before moving them to this adapter.
- Add canonical generic constraint formatting for descriptor functions.
- Converge binary module functions on the same external callable query shape.
- Extend the query model to constructor, property and meta-member callable/effect contracts.
- Add performance percentiles, peak-memory, cancellation and snapshot-race evidence before promoting full L6 robustness.
