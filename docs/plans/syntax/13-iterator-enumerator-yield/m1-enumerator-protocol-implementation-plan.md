# Syntax 13 M1 Enumerator Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `zr.iteration` the sole public owner of `Iterable<T>`, `Enumerator<T>`, `Iterator<T>`, and `AsyncIterator<T>`, then bind ordinary `for` through canonical protocol facts without Array or type-name fallback.

**Architecture:** A small N1 native descriptor module contains the stable public type and member-role metadata. Concrete containers keep their callbacks but publish `zr.iteration.Iterable<T>` metadata. A new parser bridge resolves element type from capability/prototype facts and leaves existing `ITER_*` lowering intact; M1 excludes `yield`, frames, and async lowering.

**Tech Stack:** C17, CMake, Unity, native-binding descriptors, parser type inference, VM iterator bytecodes, GCC/Clang/MSVC.

---

## File Structure

- `zr_vm_lib_iteration/`: N1 descriptor and public registration API.
- `zr_vm_parser/include/zr_vm_parser/iteration_contract.h`: canonical parser-facing contract API.
- `zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c`: protocol-driven element binding.
- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`: concrete container metadata migration only.
- `tests/iterator/test_enumerator_protocol.c`: descriptor, invalid shape, protocol binding, and static lowering coverage.
- `docs/library-and-builtins/zr-iteration-protocol.md`: public-owner and consumer contract.

### Task 1: Add the Descriptor RED Test

**Files:**
- Create: `tests/iterator/test_enumerator_protocol.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [x] Write `test_iteration_descriptor_owns_all_public_types` using this required public API:

  ```c
  const ZrLibModuleDescriptor *descriptor = ZrVmLibIteration_GetModuleDescriptor();
  TEST_ASSERT_EQUAL_STRING("zr.iteration", descriptor->moduleName);
  TEST_ASSERT_EQUAL_UINT32(4u, descriptor->typeCount);
  TEST_ASSERT_TRUE(test_type_has_protocol(descriptor, "Enumerator", ZR_PROTOCOL_ID_ITERATOR));
  TEST_ASSERT_TRUE(test_type_has_role(descriptor, "Iterable", ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT));
  ```

- [x] Register `zr_vm_enumerator_protocol_test` with Unity, core, parser, library, container, and the new iteration target.
- [x] Run `cmake --build .codex/build-s13m1-gcc --target zr_vm_enumerator_protocol_test`; the initial RED was the expected missing `zr_vm_parser/iteration_contract.h` compiler failure before the contract API existed.
- [x] Add only the root and module CMake stubs, then rerun the same target; it remained RED until the descriptor and parser contract APIs existed.

### Task 2: Register Canonical `zr.iteration`

**Files:**
- Create: `zr_vm_lib_iteration/CMakeLists.txt`
- Create: `zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h`
- Create: `zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c`
- Create: `zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c`
- Modify: `zr_vm_common/include/zr_vm_common/zr_contract_conf.h`
- Modify: `zr_vm_core/include/zr_vm_core/object.h`
- Modify: `zr_vm_cli/src/zr_vm_cli/project/project.c`
- Modify: `zr_vm_cli/CMakeLists.txt`
- Test: `tests/iterator/test_enumerator_protocol.c`

- [x] Implement `ZrVmLibIteration_GetModuleDescriptor()` and `ZrVmLibIteration_Register(SZrGlobalState *)`.
- [x] Publish only these descriptor rows:

  ```text
  Iterable<T>.getEnumerator(): zr.iteration.Enumerator<T>
  Enumerator<T>.moveNext(): bool
  Enumerator<T>.current: T
  Iterator<T> implements Enumerator<T> and is not value-constructible
  AsyncIterator<T>.moveNext(): zr.task.Task<bool>
  AsyncIterator<T>.current: T
  AsyncIterator<T>.close(): zr.task.Task<void>
  ```

- [x] Keep `ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT` as the stable iterable-factory role; add only new async role constants and `ZR_PROTOCOL_ID_ASYNC_ITERATOR` required by the descriptor.
- [x] Include the public header, link the module, and call `ZrVmLibIteration_Register(global)` before `ZrVmLibContainer_Register(global)` in CLI standard bootstrap.
- [x] Run `cmake --build .codex/build-s13m1-gcc --target zr_vm_enumerator_protocol_test` and execute it. Descriptor owner/type/role/protocol assertions pass.

### Task 3: Migrate Concrete Container Metadata

**Files:**
- Modify: `zr_vm_library/src/zr_vm_library/builtin_module.c`
- Modify: `zr_vm_lib_container/src/zr_vm_lib_container/module.c`
- Modify: `tests/container/container_test_common.c`
- Modify: `tests/container/test_container_metadata.c`
- Test: `tests/iterator/test_enumerator_protocol.c`

- [x] Add RED assertions that builtins do not expose `IEnumerable` or `IEnumerator`; `Array<T>`, `Map<K,V>`, `Set<T>`, and `LinkedList<T>` implement `zr.iteration.Iterable<...>`; and their iterator factory returns `zr.iteration.Enumerator<...>`.
- [x] Run `cmake --build .codex/build-s13m1-gcc --target zr_vm_enumerator_protocol_test zr_vm_container_metadata_test`; RED assertions exposed the legacy owner before migration.
- [x] Replace only descriptor `implements` and `getIterator` return strings. Retain concrete callbacks and capability bits. Register the iteration descriptor before containers in the shared container test bootstrap.
- [x] Run the direct Unity binaries for the enumerator and container metadata surfaces. The migration assertions pass in the dedicated enumerator target; container metadata retains a separate closed `Map.containsKey` prototype-materialization baseline marker. This project registers no CTest entries in the focused build directory, so CTest discovery is not treated as passing coverage.

### Task 4: Resolve `for` Binding from the Capability

**Files:**
- Create: `zr_vm_parser/include/zr_vm_parser/iteration_contract.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/type_inference.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c`
- Test: `tests/iterator/test_enumerator_protocol.c`
- Test: `tests/parser/test_numeric_foreach_cardinality_dataflow.c`

- [x] Add RED tests: direct `Enumerator<int>` binds `int`; `Iterable<int>` factory binds `int`; a no-capability shape fails; a constrained ref-like capability resolves only through its protocol fact; typed Array/Span loops retain static `ITER_*` bytecodes and do not request dynamic boxing.
- [x] Run the iterator test; the RED exposed the old `ZR_VALUE_TYPE_ARRAY`, `ZR_PROTOCOL_ID_ARRAY_LIKE`, and legacy protocol binding path.
- [x] Define and implement:

  ```c
  TZrBool ZrParser_EnumeratorBinding_ResolveElementType(
      SZrCompilerState *compiler,
      const SZrInferredType *source,
      SZrInferredType *outElementType);
  ```

  It may inspect resolved generic prototype protocol facts for `ZR_PROTOCOL_ID_ITERATOR` and `ZR_PROTOCOL_ID_ITERABLE`; it must not branch on Array base type, `ARRAY_LIKE`, a concrete type name, member spelling, or raw source text.
- [x] Make the legacy public `bind_foreach_element_type_from_inferred_iterable` delegate to the bridge, and call the bridge from `compile_foreach_statement` without changing `ITER_INIT`, `ITER_MOVE_NEXT`, or `ITER_CURRENT` lowering.
- [x] Run the direct Unity binaries for enumerator, numeric foreach cardinality, and loop-assignment dataflow. The equivalent CTest selection has no registered entries; the unrelated container-runtime baseline marker is recorded in the acceptance evidence.

### Task 5: Documentation, Evidence, Record, Commit

**Files:**
- Create: `docs/library-and-builtins/zr-iteration-protocol.md`
- Modify: `docs/library-and-builtins/index.md`
- Modify: `docs/plans/syntax/13-iterator-enumerator-yield/m1-enumerator-protocol-implementation-plan.md`
- Create: `docs/plans/syntax/13-iterator-enumerator-yield/m1-enumerator-protocol.md`
- Create: `tests/acceptance/2026-07-24-syntax-13-m1-enumerator-protocol.md`

- [x] Create the module document with YAML front matter whose `related_code`, `implementation_files`, `plan_sources`, and `tests` list the exact M1 surface. State that only `zr.iteration` owns the four TypeIds; containers implement capabilities; M1 has no `yield` or async lowering.
- [x] Update the category index with the new document.
- [x] Run the focused target matrix in independent GCC, Clang, and MSVC `s13m1` directories. Record command, real exit code, Unity pass/fail, and the container-runtime baseline marker in the acceptance document.
- [x] Create `m1-enumerator-protocol.md` under `## 状态与产出记录`, with `completed` status, start/finish time, exact completed work, public contract, and validation evidence. Mark all completed implementation-plan checkboxes `[x]`.
- [x] Use `GIT_INDEX_FILE=.git/index-syntax13-m1-stage` and commit only M1 paths:

  ```text
  feat(syntax): establish iteration enumerator protocol
  ```

## Self-Review

- [x] The four public names occur only under `zr.iteration`, never as builtin/container-owned type descriptors.
- [x] No `yield`, generator syntax, `iterator fn`, AsyncIterable, boxing adapter, or new function AST is introduced.
- [x] The compiler binding contains no Array base-type, `ARRAY_LIKE`, concrete type-name, member-name, or source-text fallback.
- [x] Tests cover direct enumerator, iterable factory, invalid shape, ref-like capability, concrete container migration, static lowering, and loop cleanup.
