---
related_code:
  - zr_vm_parser/include/zr_vm_parser/attribute_contract.h
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/include/zr_vm_parser/declaration_transform_contract.h
  - zr_vm_parser/src/zr_vm_parser/attribute_contract.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_interfaces.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_interfaces.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_attributes.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_attributes.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_decorator_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_decorator_identity.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface_contracts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface_contracts.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/attribute_contract.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_interfaces.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_interfaces.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_attributes.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_attributes.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_decorator_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_decorator_identity.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface_contracts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface_contracts.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
plan_sources:
  - user: 2026-07-30 verify Syntax status records and perform the breaking syntax cutover
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
  - docs/superpowers/plans/2026-07-29-syntax-upper-gates-completion.md
tests:
  - tests/compileTime/test_attribute_contract.c
  - tests/compileTime/test_comptime_contract.c
  - tests/compileTime/test_comptime_runtime_contract.c
  - tests/compileTime/test_declaration_transform_contract.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/acceptance/2026-07-31-syntax-11-m4-typed-patch-diagnostics.md
  - tests/acceptance/2026-07-31-syntax-11-m4-interface-adds.md
  - tests/acceptance/2026-07-29-syntax-upper-gates-audit.md
doc_type: module-detail
---

# Compile-Time Typed Generation

## Purpose

Syntax Gate 11 replaces keyword-specific compile-time behavior with ordinary
`comptime fn` declarations, typed metadata roles, compile-only module
descriptors, and immutable declaration data. The compiler must decide behavior
from canonical role and type contracts rather than from removed percent
keywords or source-name dispatch.

This implementation is intentionally fail-closed. A value that does not match
the registered descriptor, role, phase, immutable view, or Patch shape is
rejected before it can mutate compiler state.

## Contract Layers

`compile_tool.h` publishes the `zr.compile` and `zr.compile.declaration`
descriptors. Their callables carry a role, effect class, minimum phase,
parameter contract, return contract, and deterministic public contract hash.
Runtime code cannot bind these compile-only modules.

`comptime_contract.*` validates phase transitions and typed effects.
`comptime_runtime_contract.*` enforces fuel, depth, diagnostics, and cache
limits for the evaluator. `compile_tool_evaluator.*` dispatches the public
feature/assert/error/warning operations by descriptor role rather than by a
second parser surface. Typed constructors validate every published field before
allocating a result object.

`attribute_contract.*` defines AttributeUsage targets, retention,
repeatability, schema fields, and role identity. `compiler_attribute_binding.*`
registers readonly attribute schemas, validates typed constant arguments, and
applies Conditional call elision only to a directly bound void function.
Disabled calls are removed before argument lowering but their arguments remain
type-checked.

`declaration_transform_contract.*` validates immutable declaration views and
append-only Patch data. `compiler_declaration_transform.*` owns transform
registration and the one-round expansion boundary. The compile-time executor
decodes the currently supported typed generated field and re-enters ordinary
member layout and symbol binding. `compile_time_declaration_patch_diagnostics.*`
separately validates and emits typed Patch diagnostics so that this concern is
not added to the already large evaluator orchestration file.
`compile_time_declaration_patch_interfaces.*` decodes canonical TypeIds and
applies interface relations only after Patch validation and diagnostics.
`compiler_interface_contracts.*` keeps struct interface requirement checking
out of the already oversized struct compiler.

## Data Flow

1. Parser output contains ordinary declarations and metadata applications.
2. Signature collection registers compile-only descriptors and metadata roles,
   and prebinds all selected interface signatures before Expansion so a Patch
   can name an interface declared later in source order.
3. Build-fact evaluation prunes `comptime if` branches and records typed
   diagnostics/effects under deterministic budgets.
4. Attribute binding validates the schema and target before exposing typed
   AttributeData to a consumer.
5. A declaration transform receives an immutable view and returns a typed
   Patch for the same target SymbolId.
   Patch and diagnostic SymbolIds are rejected before narrowing unless they are
   nonzero and fit the canonical 32-bit identity domain.
6. The canonical Patch validator rejects collisions, a second expansion round,
   unknown fields, invalid generated entries, duplicate/invalid interface
   TypeIds, empty diagnostic messages, diagnostic targets outside the Patch
   target, and unsupported non-empty collections. The executor reuses this
   validator after typed decoding.
7. Each `CompileDiagnostic` must contain exactly `isError`, `message`, and
   `target`; the target must equal the Patch target, the message must be
   non-empty, and the complete array consumes the comptime diagnostic budget
   before native allocation or element decode. The default boundary accepts
   1024 entries and rejects 1025.
8. Attribute additions are decoded against registered canonical schemas before
   mutation. Target applicability, exact field kinds, retention, repeatability,
   TypeId identity, source range, and the 10,000-entry budget are validated as
   typed data; successful additions retain artifact/reflection metadata.
9. Diagnostics are decoded before any generated member is appended. Warnings
   are nonfatal and retain warning log severity; an error marks the Patch
   failed and exits before field-symbol registration or rebind.
10. Accepted generated fields pass through the normal semantic-symbol and type
   layout pipeline; they do not bypass rebind or layout.
11. Accepted interface additions append the canonical interface relation to
    the target's `inherits` and `implements` metadata. Class and struct targets
    then run ordinary required-member, receiver-effect, const-field, recursive
    parent-interface, and contract-slot validation before publication/layout.

## Failure and Compatibility Boundary

The current implementation accepts typed non-empty `diagnostics`,
`interfaceAdds`, and schema-checked `attributeAdds`. It supports only
`GeneratedField` additions, with transform-source provenance and
artifact/reflection retention. Generated methods, properties, types, generated
source maps, complete rollback after allocation failure during a multi-add,
formatter/build-dependency projection, and the remaining consumers are still
Gate 11 M4/M5 work. These gaps keep the root Syntax plan open.

The old runtime decorator executor and serialized helper callbacks are removed.
An ordinary `#name#` application must resolve to a registered static metadata
role or a typed declaration transform; an arbitrary runtime function cannot be
reintroduced as hidden module-initialization mutation.

Legacy percent spellings are not a compatibility mechanism for those missing
features. The production parser rejects `%compileTime` and related removed
forms. Migration tooling may describe an edit, but a missing typed contract
must remain an explicit compile-time error.

## Test Coverage

The focused contract executables cover descriptor stability, role/schema
validation, phase/effect/budget behavior, Patch validation, collisions, and the
single expansion round. `test_compile_time_execution.c` covers the integrated
compiler path, Conditional argument elision, module-scope restrictions,
generated-field layout/rebind/provenance, typed warning/error Patch diagnostics,
schema-checked attribute additions, static decorator shape retention, and
canonical interface additions for class and struct targets, later-declared
interfaces, required members, receiver/const rules, contract slots, alias-aware
identity collisions, negative typed constructor shapes, uint32 SymbolId alias
rejection, and direct error-before-field-registration ordering. The runtime contract also checks
diagnostic message/severity/location preservation, warning log projection, and
the 1024/1025 budget boundary.
`test_attribute_contract.c` locks the diagnostic constructor schema into the
hashed canonical provider contract. The upper-gate
ledger records which clauses are proven and which remain partial; passing the
focused tests does not promote Gate 11 M4/M5 as a whole.

The 2026-07-30 WSL GCC isolated build and full 123-test CTest matrix include the
pre-diagnostic baseline. The 2026-07-31 WSL GCC and Clang focused replays pass
71/71 after adding direct diagnostic and interface-addition evidence; later
2026-08-01 focused replays include attribute additions, artifact/reflection
retention, and runtime-decorator absence. The same
toolchains also pass the 127-case compiler integration suite. See the linked
acceptance records for exact commands and RED/GREEN history. This focused
evidence does not replace the final multi-toolchain gate matrix.

## File Structure

The compile-time executor remains the orchestration boundary for the existing
interpreter. New independent responsibilities are kept in semantic modules:
descriptor evaluation, runtime budgets, attribute binding, and declaration
transform registration each have named source files and narrow internal APIs.
Patch diagnostic decoding is also isolated behind one internal function rather
than adding more field/schema helpers to `compile_time_executor.c`.
Patch interface decoding and value-type interface contract validation are
isolated in their own modules for the same reason.
`compiler_diagnostics.c` is the single mapping point from compile-time
diagnostic severity to the core logging level.
`compiler_attribute_binding.c` stays as one cohesive schema-to-application
pipeline even though it is slightly above the repository size warning; splitting
its mutually dependent schema parser and application validator would add a
second private contract without reducing current coupling.
