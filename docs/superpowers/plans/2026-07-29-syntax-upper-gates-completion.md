---
related_code:
  - zr_vm_parser/
  - zr_vm_core/
  - zr_vm_library/
  - zr_vm_lib_container/
  - zr_vm_lib_ffi/
  - zr_vm_cli/
  - zr_vm_language_server/
  - tests/
implementation_files:
  - zr_vm_parser/
  - zr_vm_core/
  - zr_vm_library/
  - zr_vm_lib_container/
  - zr_vm_lib_ffi/
  - zr_vm_cli/
  - zr_vm_language_server/
plan_sources:
  - user: 2026-07-29 complete Syntax gates 06B, 07B, 08, 09, 10F/10C, 11, and 14
  - docs/plans/syntax/README.md
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
  - docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
  - docs/plans/syntax/2026-07-20-14-test-function-harness-design.md
tests:
  - tests/parser/test_reflection_type_surface.c
  - tests/parser/test_reflection_type_stress.c
  - tests/container/test_generational_pool.c
  - tests/ffi/test_native_extern_contract.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/fixtures/projects/syntax_reference_v1/
  - tests/fixtures/syntax_migration_inventory/
doc_type: milestone-detail
---

# Syntax Upper Gates Completion Plan

> Execution rule: keep development on `main`, use test-first RED/GREEN cycles, validate in WSL GCC and Clang before MSVC, and do not promote a gate from status text or a narrower smoke test.

## Scope And Order

The deliverable is the complete published scope of 06B, 07B, 08, 09, 10F/10C, 11, and 14. The dependency order is:

```text
09 M1-M3 (independent foundation)
  -> 08 M1-M5
  -> 09 M4-M5
  -> 10F evidence closure
  -> 11 M1-M5
  -> 14 M1-M4
  -> 10C provider/consumer convergence
  -> 06B atomic cutover and legacy deletion
  -> 07B current reference promotion
```

10R, 12, and 13 are consumed as existing promoted dependencies, but their current source and tests must still pass after the cutover.

## Task 1: Freeze Requirement-To-Evidence Ledger

**Files:**

- Create: `tests/acceptance/2026-07-29-syntax-upper-gates-audit.md`
- Update: `.codex/sessions/20260729-0158-syntax-upper-gates.md`

1. List every milestone and promotion clause from the seven design documents.
2. Map each clause to implementation files, direct tests, and environment evidence.
3. Mark evidence as proven, contradicted, indirect, or missing.
4. Keep a gate open whenever any clause lacks direct evidence.

## Task 2: Complete Gate 09 M1-M3 Native Contract

**Files:**

- Modify: `tests/container/test_generational_pool.c`
- Modify: `zr_vm_library/include/zr_vm_library/native_binding.h`
- Modify: `zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c`
- Modify: `zr_vm_lib_container/src/zr_vm_lib_container/pooling.c`
- Modify: `zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h`
- Modify: `zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c`
- Update: `docs/library-and-builtins/zr-pooling-and-pinned-ffi-views.md`

1. RED: require descriptor parameter passing modes and the published `bool + out PoolRef<T>` / `bool + out PoolReadRef<T>` signatures.
2. GREEN: add a provider-neutral native parameter passing-mode field, serialize it into native metadata, and import it into canonical callable facts.
3. RED: compile current `import("zr.pooling")` source and verify `out` call binding, definite assignment, and default failed view.
4. GREEN: bind `Pool` callbacks to the existing generational slab runtime without type-name dispatch.
5. RED/GREEN: cover reader/writer conflicts, recycle retirement, early cleanup, view replacement order, ref escape, box/array/closure/suspension rejection, mapped/barriered scanning, and exactly-once drop.
6. Verify the hot successful-borrow path does not repeat generation validation.

## Task 3: Complete Gate 08 M1-M5

**Files:**

- Modify: `tests/parser/test_reflection_type_surface.c`
- Modify: `tests/parser/test_reflection_type_stress.c`
- Add or modify: `tests/artifact/test_reflection_metadata_roundtrip.c`
- Add or modify: `tests/language_server/test_lsp_reflection_facts.c`
- Modify as proven necessary: `zr_vm_core/src/zr_vm_core/reflection_*.c`
- Modify as proven necessary: `zr_vm_parser/src/zr_vm_parser/compiler/`
- Modify as proven necessary: `zr_vm_language_server/`
- Update: reflection module documentation under `docs/core-runtime/` and `docs/parser-and-semantics/`

1. M1: prove exact/static/erased identities and every declaration category without name dispatch.
2. M2: prove field mutability, property accessor/ref-return contracts, overload/generic queries, access control, ambiguity, and by-ref invocation rejection.
3. M3: prove source/native/binary schema and contract-hash equality plus corrupt/stripped artifact negatives.
4. M4: prove direct/spread construction, no-match/ambiguity/throw, category rejection, generation invalidation, GC compact, and VM/AOT equivalence.
5. M5: prove canonical LSP hover/completion/navigation, migration classifications, 100k member/deep inheritance/cache/GC/throw stress.

## Task 4: Complete Gate 09 M4-M5

**Files:**

- Modify: `tests/container/test_generational_pool.c`
- Add or modify: `tests/artifact/test_pool_contract_roundtrip.c`
- Add or modify: `tests/language_server/test_lsp_pooling_facts.c`
- Modify: `zr_vm_lib_container/src/zr_vm_lib_container/pooling.c`
- Modify as proven necessary: `zr_vm_core/`, `zr_vm_parser/`, `zr_vm_language_server/`

1. Roundtrip StableSlotSource, layout, guard, isolation, and concurrency contracts through source/native/binary artifacts.
2. Reject corrupt or unknown layout/capability records.
3. Expose only immutable pool metadata to reflection; prohibit boxing or persistent direct refs.
4. Prove LSP behavior uses role/capability facts.
5. Run million-handle, churn, scan-byte, concurrent/thread-local, compact, and drop stress independently.

## Task 5: Close Gate 10F Evidence

**Files:**

- Modify as needed: `tests/ffi/test_native_extern_contract.c`
- Modify as needed: `zr_vm_parser/include/zr_vm_parser/ffi_contract.h`
- Modify as needed: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c`
- Modify as needed: `zr_vm_lib_ffi/`, `zr_vm_aot/`, `zr_vm_library/`

1. Cross-check every 10F clause against scalar, aggregate/union, in/ref/out, callback, owner/resource/ref-struct/Span, marshalling, target ABI, throw/cleanup, and corrupt artifact tests.
2. Add RED cases for any missing vector before implementation.
3. Run the same canonical signature vectors through VM/libffi and AOT C; validate LLVM lowering boundaries.

## Task 6: Complete Gate 11 M1-M5

**Files:**

- Modify: `tests/compileTime/test_compile_time_execution.c`
- Add: `tests/compileTime/test_typed_declaration_transform.c`
- Modify or add: compile-time artifact and LSP tests
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_time_*.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c`
- Modify as needed: `zr_vm_library/`, `zr_vm_core/`, `zr_vm_language_server/`

1. Preserve M1 build-fact/comptime-if coverage and remove legacy assumptions.
2. Implement typed evaluator effects, deterministic budgets/cache, and the four diagnostic APIs.
3. Implement AttributeUsage and Conditional roles with canonical IDs and argument-SemIR elision.
4. Implement immutable declaration views, append-only typed Patch, one expansion round, generated rebind/source map, collision/cycle/budget negatives.
5. Roundtrip artifacts, compile-tool build dependencies, reflection projection, formatter, LSP virtual documents, and migration.
6. Delete runtime decorator and concrete type-name dispatch paths.

## Task 7: Complete Gate 14 M1-M4

**Files:**

- Create: `zr_vm_parser/include/zr_vm_parser/test_contract.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/test_binding.c`
- Create: `zr_vm_lib_testing/`
- Create: `zr_vm_cli/src/commands/test_command.c`
- Create: `zr_vm_cli/src/testing/test_runner.c`
- Add: `tests/testing/`
- Modify: `zr_vm_core/`, `zr_vm_language_server/`, debug integration, and CMake files

1. RED/GREEN: bind test/case/skip roles only on ordinary `fn`/`async fn`, validate signatures/constants, serialize TestManifest, and trim production roots after full checking.
2. RED/GREEN: implement the N3 Test descriptor and only assert/equal/throws plus structured bounded failures.
3. RED/GREEN: implement deterministic manifest discovery, filtering/listing, sync/async invocation, module isolates, jobs, timeout/crash, output capture, and exit codes.
4. RED/GREEN: make LSP/debug consume facts/manifest and migrate `%test` idempotently.
5. Prove no keyword, special function AST, source-text discovery, duplicate module, or production executable remains.

## Task 8: Complete Gate 10C

**Files:**

- Modify: native provider inventory/registry and canonical identity consumers
- Add: provider convergence tests under `tests/library/`, `tests/module/`, and `tests/language_server/`
- Update: `docs/module-system/`

1. Freeze the N0-N3 official provider inventory and canonical type roles from 08, 09, and 11-14.
2. Migrate bare `debug` to `zr.debug` once, with migration diagnostics but no permanent duplicate registration.
3. Prove descriptor/artifact/LSP/reflection/debug projections share identity/schema and provider phase.
4. Prove all domain, duplicate, capability, phase, alias, package-version, file-URI, and reserved-root cases.
5. Reject placeholders for any provider whose owner gate is not directly proven.

## Task 9: Execute 06B Atomic Cutover

**Files:**

- Modify: migration frontend and inventory tests
- Modify: parser/compiler/writer/LSP/formatter/CLI sources
- Modify: all current `.zr`, project fixtures, examples, snippets, golden files, and artifacts
- Modify: `docs/zr_language_specification.md`

1. Re-run 06A dry-run and require zero blocked/unknown/targetNotPromoted items.
2. Apply structured edits for every current source and fixture; never use blind `%` text replacement.
3. Switch the production parser from legacy lowering to stable migration diagnostics and fix hints.
4. Remove legacy AST fields, helpers, lowering/runtime emission, completion/token metadata, and duplicate semantics.
5. Rebuild versioned `.zrs/.zri/.zro` and reject old schema with a recompile diagnostic.
6. Enforce a structured allowlist: modulo, migration inputs, negatives, strings/comments, and superseded history only.

## Task 10: Promote 07B Reference Fixture

**Files:**

- Modify: `tests/fixtures/projects/syntax_reference_v1/`
- Modify: syntax reference manifest/runner/golden tests
- Modify: `docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md`

1. Replace every pending/legacy/current ambiguity with executable current pass/fail cases.
2. Cover parser, semantic facts, VM, AOT, artifact, CLI, LSP, debugger, migration negatives, and platform capability variants.
3. Require current examples to compile/typecheck and negative cases to assert stable diagnostic codes.
4. Promote 07B only after 06B and all provider gates are independently proven.

## Task 11: Final Review, Validation, Cleanup, And Commit

1. Run focused suites after every RED/GREEN cycle.
2. Run WSL GCC Debug/ASan and Clang/UBSan matrices, then Windows MSVC Debug/ASan where supported.
3. Run full CTest tiers, migration inventory, reference manifest, artifact roundtrip, VM/AOT equivalence, LSP, CLI, and stress suites with zero ignored failures.
4. Perform code review against every promotion clause and record exact commands/results in acceptance documents.
5. Remove repository build products, generated logs, crash dumps, temp mirrors, and test output while preserving tracked golden artifacts.
6. Stage only goal-owned files through an isolated index, inspect the staged diff, and create detailed commits on `main`.
7. Confirm the final worktree has no uncommitted goal-owned change and no goal-owned build/log residue.
