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
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/attribute_contract.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
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
second parser surface.

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
member layout and symbol binding.

## Data Flow

1. Parser output contains ordinary declarations and metadata applications.
2. Signature collection registers compile-only descriptors and metadata roles.
3. Build-fact evaluation prunes `comptime if` branches and records typed
   diagnostics/effects under deterministic budgets.
4. Attribute binding validates the schema and target before exposing typed
   AttributeData to a consumer.
5. A declaration transform receives an immutable view and returns a typed
   Patch for the same target SymbolId.
6. Patch validation rejects collisions, a second expansion round, unknown
   fields, invalid generated entries, and unsupported non-empty collections.
7. Accepted generated fields pass through the normal semantic-symbol and type
   layout pipeline; they do not bypass rebind or layout.

## Failure and Compatibility Boundary

The current implementation rejects non-empty `interfaceAdds`, `attributeAdds`,
and `diagnostics`. It also supports only `GeneratedField` additions. Generated
methods, properties, types, source maps, transactional multi-add rollback, and
all artifact/reflection/LSP/formatter consumers remain Gate 11 M4/M5 work.
These gaps keep the root Syntax plan open.

Legacy percent spellings are not a compatibility mechanism for those missing
features. The production parser rejects `%compileTime` and related removed
forms. Migration tooling may describe an edit, but a missing typed contract
must remain an explicit compile-time error.

## Test Coverage

The four focused contract executables cover descriptor stability, role/schema
validation, phase/effect/budget behavior, Patch validation, collisions, and the
single expansion round. `test_compile_time_execution.c` covers the integrated
compiler path, Conditional argument elision, module-scope restrictions,
generated-field layout/rebind, and negative typed shapes. The upper-gate ledger
records which clauses are proven and which remain partial; passing the focused
tests does not promote Gate 11 M4/M5 as a whole.

The 2026-07-30 WSL GCC isolated build and full 123-test CTest matrix include all
of these targets. Direct focused evidence is 52/52 across compile-time,
attribute, comptime contract/runtime, and declaration-transform suites.

## File Structure

The compile-time executor remains the orchestration boundary for the existing
interpreter. New independent responsibilities are kept in semantic modules:
descriptor evaluation, runtime budgets, attribute binding, and declaration
transform registration each have named source files and narrow internal APIs.
`compiler_attribute_binding.c` stays as one cohesive schema-to-application
pipeline even though it is slightly above the repository size warning; splitting
its mutually dependent schema parser and application validator would add a
second private contract without reducing current coupling.
