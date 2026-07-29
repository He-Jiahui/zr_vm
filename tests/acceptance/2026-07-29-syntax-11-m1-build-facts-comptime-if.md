---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_late_check.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_binding.c
tests:
  - tests/compileTime/test_compile_time_execution.c
  - tests/parser/test_ref_struct_restrictions.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/task/test_task_runtime.c
doc_type: acceptance-record
---

# Syntax 11 M1 Acceptance: BuildFacts And `comptime if`

## Scope

This record accepts only Gate 11 M1: the BuildFacts phase, compile-tool binding boundary,
and current-syntax `comptime if` branch selection. It does not close the root Syntax 11 gate
or later macro/derivation milestones.

BuildFacts now distinguishes module declaration lists from runtime lexical blocks. Module-level
conditions are evaluated before runtime bodies, selected module functions participate in whole-
module hoisting regardless of source order, and selected runtime functions remain scoped to their
branch. Compile-tool providers lose to runtime function shadows, while runtime uses of an actual
provider report `compiletool.phase_mismatch`.

Traversal covers interface method/meta signatures, native extern function/delegate signatures,
regular and variadic parameters, declaration and parameter decorators, and default expressions.
Late validation rejects `comptime {}` in runtime functions and runtime top-level containers.

## Fresh Validation

| Environment | Target | Result |
|---|---|---:|
| WSL GCC 11.4 Debug | `zr_vm_compile_time_test` | 61/61 |
| WSL GCC 11.4 Debug | `zr_vm_ref_struct_restrictions_test` | 11/11 |
| WSL GCC 11.4 Debug | `zr_vm_reference_escape_closure_suspension_test` | 13/13 |
| WSL GCC 11.4 Debug | `zr_vm_task_runtime_test` | 17/17 |
| Windows MSVC 19.44 Debug | `zr_vm_compile_time_test` | 61/61 |

The hoisting matrix includes direct module functions, selected module functions before and after
their first runtime use, local functions before and after use, repeated BuildFacts preparation,
and a selected runtime shadow followed by a compile-tool feature query. AST assertions verify the
selected branch in every signature/decorator variant rather than relying only on compilation.

Final review found that direct module-level runtime statements were traversed with the declaration
phase flag. Top-level `return`, `if`, and `while` could therefore use a CompileTool provider without
the required phase error. Module traversal now carries runtime phase independently from module-
declaration permission; three focused RED-to-GREEN cases close that gap without changing import
prebinding or selected-function hoisting.

The final isolated-index pass also found that the current `import("...")` entry point and specific
compile-time error propagation were outside the staged boundary. The parser now accepts the current
form while retaining `%import` compatibility and rejecting `zr.import`; compile-time failures keep
their concrete diagnostic text so phase and module-scope assertions remain machine-checkable. The
reproduced WSL result moved from 49/61 to 61/61.

## Outcome

Syntax 11 M1 is accepted. Root Syntax 11 remains open, and therefore this milestone does not
authorize the final 06B repository-wide legacy syntax deletion.
