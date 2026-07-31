---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b4 structured runtime-root reference origin
status: completed
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_branch_assignment_join.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_branch_refinement.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_loop_assignment_scope.c
tests:
  - tests/parser/test_expression_fragment_parser.c
---

# E2b4 Structured Runtime-Root Reference Origin

## Contract

- `ZrParser_TypeEnvironment_RegisterRuntimeRoot` accepts only a structured root
  kind and nonzero opaque token, and rejects an already-bound source name.
- The normal semantic context allocates a query-local `SymbolId` and `TypeId`.
  Runtime roots have no source declaration range and no function-local Place.
- Identifier read facts copy `originKind`, `runtimeRootKind`, and `originToken`
  from the binding. They never recover root identity from `zr` text or an AST.
- Ordinary and canonical source registration clear every runtime-root field.
- Branch-assignment, branch-refinement, and loop-assignment environment clones
  preserve the complete Place/origin identity.
- The new public reference-fact fields are tail-appended so all existing fact
  field offsets remain unchanged.

This slice publishes parser origin facts only. Core token resolution is already
available from E2b3; Debug formal execution and closure-capture origin remain
later E2b/E3 consumers.

##状态与产出记录

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-01 04:18 +08:00 | `completed` | Published structured runtime-root binding/reference origin, query-local registration, stale-field reset, complete branch/loop identity cloning, and focused parser coverage. |

## Validation

- RED: MSVC focused compilation failed on the absent registration API, origin
  enums, and binding/reference fields.
- GREEN: after a clean static rebuild, MSVC passes expression fragment 6/6,
  semantic facts 12/12, semantic query 27/27, and Debug expression diagnostics
  37/37; every process exits 0.
- GREEN: GCC and Clang compile the five exact changed implementation objects and
  focused test object with process exit 0. Both toolchains also recompile the
  final `type_system.c` and test delta with process exit 0.
- The full E3 consumer/multi-toolchain acceptance matrix remains pending and is
  not implied by this parser support record.
