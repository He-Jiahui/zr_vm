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
  - tests/language_server/test_lsp_project_native_receiver_callable_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# LSP Native Descriptor Function Callable Contract

## Purpose

Native module functions already expose structured `ZrLibFunctionDescriptor` metadata through the native registry and LSP metadata provider. Before this change, semantic query could resolve the exact module and function descriptor, but signature help discarded parameter names and documentation by formatting only inferred argument types. Hover used another projection and could display only a broad resolved type.

The external callable contract adapter makes the resolved descriptor the single display source for native module functions. Hover and signature help now consume the same name, generic parameter list, ordered parameters, return type and documentation without searching by member text or reconstructing a callable from the AST.

Native receiver call sites use a split structured contract. The current metadata descriptor supplies exact method identity, parameter names and documentation, while the parser `CallAt` fact supplies the closed canonical function `TypeId`. The LSP formats parameter and return types plus receiver effect from that `TypeId`, so a `LinkedList<int>` method cannot regress to raw descriptor types containing `T`.

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

For an instance `METHOD`, the same exact callee range must also resolve through `ZrParser_SemanticQuery_CallAt`. `ZrLanguageServer_LspExternalCallableContract_FromResolvedMethod` accepts only a canonical function `TypeId` with a receiver effect and a complete current-generation method descriptor. It never substitutes the descriptor owner name or parses raw generic text.

## Contract Formatting

`ZrLanguageServer_LspExternalCallableContract_FromResolvedMember` borrows the immutable descriptor fields from the resolved metadata member. The formatter emits:

```text
name<genericParameters>(parameterName: parameterType, ...): returnType
```

Parameter information uses the same ordered descriptor array. Descriptor parameter documentation is preserved and may be followed by argument-specific semantic fact documentation, such as numeric ranges or ownership facts. Active parameter selection remains based on the parsed function-call argument range.

Hover calls the same formatter and adds provider source and callable documentation around that exact label. A descriptor-plugin reload therefore changes both features together when the provider generation publishes a new descriptor.

Receiver methods emit `const fn` or `fn` from the canonical receiver effect. Ordered descriptor parameter names are paired by index with canonical parameter contracts; passing, escape, closed parameter type and return type all come from the canonical function. For example, the builtin contract is `fn addLast(value: int): LinkedNode<int>`.

## Availability And Fallback Rules

The signature resolver returns three states:

- `NOT_EXTERNAL`: the target is not an in-scope native callable, or is a static method outside this adapter, so existing consumers may continue.
- `RESOLVED`: a complete structured descriptor contract produced signature help.
- `UNAVAILABLE`: semantic query resolved an in-scope native function, but required descriptor fields or formatting were unavailable. The caller returns no signature instead of falling back to member-name or AST-text inference.

Unknown return or parameter types are not rewritten as `any`. Generic module-function descriptors are formatted only when their structured names are available. Constraint-bearing generic module functions and generic receiver methods currently return unavailable because this adapter does not yet have a canonical generic-constraint/method-clause formatter.

## Receiver Method Boundary

Only instance method call sites with both exact metadata identity and a canonical parser call fact enter the receiver adapter. Bare method references keep the established metadata hover. Static methods return `NOT_EXTERNAL`. A recognized instance method whose descriptor is incomplete, whose canonical function is effectful, or whose method generic clause is non-empty returns `UNAVAILABLE`; it does not fall through to member-name, AST-text or raw-owner substitution.

This boundary still excludes constructors, meta-members and properties. They require their own query kinds and effect/access contracts rather than being treated as functions or instance methods.

## Snapshot And Generation Behavior

This implementation does not add a new snapshot or descriptor schema. The external consumer obtains the descriptor only from the current `SZrLspSemanticQuery`, whose module resolution selects the current provider generation. Module-function tests replace an integer plugin with a floating-point plugin and require both labels to change from `int` to `float`. Receiver tests replace `total(): int` with raw descriptor `total(): float`; the reanalyzed canonical call fact normalizes the visible result to `fn total(): double`.

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

Receiver coverage adds:

- builtin `LinkedList<int>.addLast(1)` query identity, exact `fn addLast(value: int): LinkedNode<int>` hover/signature label and active parameter 0;
- descriptor-plugin `ProbePoint.total()` before and after provider reload, including method documentation and canonical `float` to `double` normalization;
- `incomplete_total(): unknown` returning no signature help, proving no canonical-wide or member-name fallback;
- the same builtin receiver label through real stdio `textDocument/signatureHelp` and `textDocument/hover` requests.

## Open Work

- Publish structured generic method clauses and effect display before accepting generic/effectful native receiver methods.
- Decide whether bare and static native method references should converge on the call-site adapter or retain distinct contracts.
- Add canonical generic constraint formatting for descriptor functions.
- Converge binary module functions on the same external callable query shape.
- Extend the query model to constructor, property and meta-member callable/effect contracts.
- Add performance percentiles, peak-memory, cancellation and snapshot-race evidence before promoting full L6 robustness.
