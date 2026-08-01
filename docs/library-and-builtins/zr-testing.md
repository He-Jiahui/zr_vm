---
related_code:
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
implementation_files:
  - zr_vm_lib_testing/src/zr_vm_lib_testing/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
plan_sources:
  - docs/plans/syntax/2026-07-20-14-test-function-harness-design.md
tests:
  - tests/testing/test_test_role_binding.c
  - tests/testing/test_assertions.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/fixtures/projects/testing_reference
doc_type: module-detail
---

# zr.testing Test-Phase Provider

## Public Contract

`zr.testing` is the official N3 Test-phase provider. Runtime hosts cannot
consume it. A test host may consume Runtime and Test providers, but never
CompileTool providers. Its descriptor owns three retained metadata roles:

- `zr.testing.test` marks an ordinary top-level `fn` as a test root.
- repeatable `zr.testing.case` supplies compile-time constant arguments.
- `zr.testing.skip` retains one non-empty skip reason.

The callable surface is `assert`, generic `equal<T>`, and synchronous
`throws<E>`. Failures are bounded structured `AssertionFailure` values rather
than process-global text matching. The provider descriptor, phase, role ids,
and public contract hash participate in the official-provider inventory.

## TestManifest

Test compilation type-checks ordinary functions and emits a versioned binary
`TestManifest`. Each entry carries canonical function SymbolId/TypeId,
callable child index, module/qualified name, source range, async bit, skip
reason, and constant case arguments. Decode rejects unknown schema versions,
oversized counts, invalid booleans, truncation, trailing bytes, and overlong
strings. Cleanup is valid for partially decoded input, including allocation
failure and corrupt-count paths.

Production compilation still type-checks test declarations, then trims their
roots and emits no manifest. A production declaration cannot call a trimmed
test function. There is no hidden `main`, source-text test discovery, or old
`%test` AST/lowering path.

## Async And Isolation Boundary

Synchronous tests return `void`. Async tests explicitly return
`zr.task.Task<void>`; the host awaits the result through the canonical task
contract. Test execution happens in a fresh global state, and the CLI process
runner can place each selected case in a child process with timeout and output
capture. Debug projection of the manifest remains a Gate 14 M4 follow-up.
