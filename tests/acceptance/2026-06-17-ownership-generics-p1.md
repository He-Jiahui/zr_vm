# Ownership Generics P1 Frontend Slice

## Scope

- Implement the first P1 slice from `docs/plans/using`: ownership kind as intrinsic generic surface.
- Covered this round:
  - `Unique<T>` / `Shared<T>` / `Weak<T>` / `Borrow<T>` / `Loan<T>` intrinsic type recognition.
  - Legacy `%unique T` / `%shared T` / `%weak T` / `%borrow T` / `%loan T` syntax desugaring to intrinsic ownership generic AST wrappers.
  - Type inference unwrap from ownership generic wrapper to inner type plus owner qualifier.
  - Explicit generic constructor calls such as `Unique<Box>(value)` lowering to existing ownership builtin construct expressions.
  - Field prototype metadata for ownership generic fields, preserving inner field type name and owner qualifier.
  - Canonical bare `using` statement parsing, while preserving `%using` compatibility.
  - `using` cleanup semantic metadata recording owner qualifier and selected cleanup builtin kind.
  - `where T: owner` parsing, metadata propagation, and generic call constraint enforcement.

## Evidence

- Timestamp: 2026-06-17 14:41:04 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Focused build:
  - `zr_vm_parser_test`
  - `zr_vm_type_inference_test`
- Focused parser test result:
  - `./bin/zr_vm_parser_test`
  - `70 Tests 0 Failures 0 Ignored OK`
- New P1-focused type inference checks passed inside `zr_vm_type_inference_test`:
  - `Type Inference - Intrinsic Ownership Generic Types`
  - `Semantic - Using Statement Cleanup Plan`
  - `Semantic - Using Cleanup Ownership Generic Kind`
  - `Type Inference - Source Owner Constraint Is Enforced`
- Extended generic parser checks passed inside `zr_vm_parser_test`:
  - `Interface Variance And Where Parsing` now covers `where T: owner`
  - `Ownership Intrinsic Generic Type Surface Parsing`
  - `Using Keyword Statement Parsing`
- New P1-focused compiler integration checks passed before later existing runtime failures:
  - `Using Owner Generic Emits Release Cleanup`
  - `Intrinsic Ownership Generic Field Prototype Metadata`
  - `Intrinsic Ownership Generic Constructors Emit Dedicated Opcodes`
  - `Ownership Weak Runtime Expires To Null After Last Shared Release`
  - `Ownership Upgrade And Release Runtime Follow Lifecycle Contract`

## Remaining Work

- `zr_vm_type_inference_test` still reports seven existing failures in native/source generic import and interface-member flow tests. The new ownership generic tests pass.
- `zr_vm_compiler_integration_test` still has existing frame-layout and ownership runtime failures before/around the legacy ownership runtime tests. The new ownership generic compiler tests pass.
- Scope-exit runtime release lowering for `Unique<T>` / `Shared<T>` using resources is complete enough to emit and execute `OWN_RELEASE` in the focused integration test.
- Borrow/Loan scope-exit behavior still needs the dedicated end-borrow / return-loan design and escape diagnostics.
- `where T: owner` is implemented. More specific future predicates such as `unique` / `shared` / `weak` remain unimplemented.
- Deprecation warnings for `%unique T` / `%shared T` / `%weak T` compatibility syntax remain unimplemented.

## Update: using owner control-flow cleanup

- Timestamp: 2026-06-17 15:18:50 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Added runtime lowering for `Unique<T>` / `Shared<T>` `using (owner)` cleanup across:
  - normal lexical scope exit,
  - direct `return` from inside the using body,
  - `break` that jumps out of the using body.
- Return lowering now disables tail-call emission while active owner cleanups exist, so `OWN_RELEASE` cannot be skipped by tail-call control flow.
- Loop labels now record the target scope-stack depth; break/continue cleanup only releases owner scopes that the jump actually crosses.
- New P1-focused compiler integration checks passed:
  - `Using Owner Generic Emits Release Cleanup`
  - `Using Owner Generic Release Runs Before Return`
  - `Using Owner Generic Release Runs Before Break`
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_compiler_integration_test -j1` succeeded.
  - `./build/codex-using-wsl-gcc-debug/bin/zr_vm_parser_test`: `70 Tests 0 Failures 0 Ignored OK`.
  - Related type-inference checks still pass: intrinsic ownership generic types, using cleanup ownership generic kind, and source owner constraint enforcement.
- Known residual failures unchanged:
  - `zr_vm_type_inference_test`: 7 existing generic/import/interface failures outside the new ownership generic checks.
  - `zr_vm_compiler_integration_test`: existing frame-layout failures and existing ownership runtime failures remain after the new P1-focused tests pass.

## Update: Borrow/Loan scope-end cleanup

- Timestamp: 2026-06-17 16:39:49 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Added deterministic scope cleanup for:
  - `using (Borrow<T>(owner))` on normal scope exit,
  - `using (Borrow<T>(owner))` before direct `return`,
  - `using (Loan<T>(owner))` on normal scope exit,
  - `using (Loan<T>(owner))` before `break` exits the using body.
- Added `OWN_RETURN_LOAN` and `ZrCore_Ownership_ReturnLoanValue` so a loaned value can be moved back into its source owner slot at cleanup time.
- Updated interpreter execution, SemIR/writer naming, generated frame slot scan, C AOT lowering, LLVM AOT helper selection, and AOT runtime helper declarations for `OWN_RETURN_LOAN`.
- New / extended checks passed:
  - `Using Borrow Generic Emits End Borrow Cleanup`
  - `Using Borrow Generic End Borrow Runs Before Return`
  - `Using Loan Generic Returns Loan To Source On Scope Exit`
  - `Using Loan Generic Returns Loan Before Break`
  - `zr_vm_aot_c_ownership_contracts_test`: `1 Tests 0 Failures 0 Ignored OK`
  - `zr_vm_parser_test`: `70 Tests 0 Failures 0 Ignored OK`
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_compiler_integration_test zr_vm_aot_c_ownership_contracts_test -j 1` succeeded.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_parser_test zr_vm_type_inference_test -j 1` succeeded.
  - Ownership-related type-inference checks still pass, including intrinsic ownership generic types, ownership builtin operand checks, using cleanup ownership generic kind, and source owner constraint enforcement.
- Known residual failures:
  - `zr_vm_type_inference_test` still reports `FAIL` because existing generic boxed-new / import / interface-member tests outside this P1 slice fail.
  - `zr_vm_compiler_integration_test` still hits existing frame-layout failures and existing legacy ownership runtime failures after the new owner/borrow/loan using tests pass.
  - Borrow/Loan escape diagnostics across returned values, fields, closures, globals, async/thread boundaries remain unimplemented.

## Update: Borrow/Loan assignment escape diagnostics

- Timestamp: 2026-06-17 17:01:32 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Added conservative Borrow/Loan escape checks in the shared assignment compatibility path:
  - `Borrow<T>` can only flow into a `Borrow<T>` target.
  - `Loan<T>` can only flow into a `Loan<T>` or `Borrow<T>` target.
  - Writing either kind into plain/owning targets now reports the dedicated escape message before the generic type mismatch path.
- New check passed:
  - `Type Inference - Borrowed And Loaned Values Cannot Escape Through Assignment`
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_type_inference_test -j 1` succeeded.
  - `zr_vm_type_inference_test`: new Borrow/Loan assignment escape test passed; full target still reports existing generic/import/interface failures.
  - `zr_vm_parser_test`: `72 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_aot_c_ownership_contracts_test`: `1 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_compiler_integration_test`: Borrow/Loan/ownership-generic focused checks passed before existing frame-layout and ownership runtime failures; the full target still aborts after those known failures.
- Remaining work:
  - Region-level escape analysis for call arguments, returns/facts, fields, closures, globals, async/thread boundaries, and plugin types.
  - LSP structured diagnostic golden coverage for the new generic ownership surface.

## Update: owner-to-plain assignment diagnostics

- Timestamp: 2026-06-17 17:16:31 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Tightened the shared assignment compatibility path so `Unique<T>` / `Shared<T>` cannot flow into plain `T` through the generic type-mismatch fallback.
- The diagnostic now reports:
  - `Owned value cannot flow into a plain GC value implicitly`
- Red/green evidence:
  - Updated `Type Inference - Owned Value Requires Detach Before Plain Flow` to expect the owner-to-plain diagnostic.
  - Before implementation, the test failed because the error was still `Type mismatch`.
  - After implementation, the same test passed for both unique and shared owner values.
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_type_inference_test -j 1` succeeded.
  - `zr_vm_type_inference_test`: owner-to-plain and Borrow/Loan assignment escape checks passed; full target still reports the existing seven generic/import/interface failures.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_parser_test zr_vm_aot_c_ownership_contracts_test zr_vm_compiler_integration_test -j 1` succeeded.
  - `zr_vm_parser_test`: `73 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_aot_c_ownership_contracts_test`: `1 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_compiler_integration_test`: owner/borrow/loan using focused checks passed before the existing frame-layout and ownership runtime failures.
- Remaining work:
  - Dedicated owner-to-plain diagnostics for call arguments.
  - Structured diagnostic/LSP golden coverage.
  - Full region-level ownership escape analysis.

## Update: call argument ownership escape diagnostics

- Timestamp: 2026-06-17 17:32:18 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Extended ownership flow diagnostics into function call argument compatibility and overload failure reporting:
  - `Borrow<T>` passed to a plain parameter reports `Borrowed value cannot escape its owner`.
  - `Loan<T>` passed to a plain parameter reports `Loaned value cannot escape its owner`.
  - `Unique<T>` / `Shared<T>` passed to a plain parameter reports `Owned value cannot flow into a plain GC value implicitly`.
- Red/green evidence:
  - Before implementation, the new/updated plain-parameter tests failed with `No matching overload for function 'Observe'`.
  - After implementation, all three focused parameter tests passed with the dedicated ownership diagnostics.
- New / updated checks passed:
  - `Type Inference - Borrowed Value Cannot Flow To Plain Parameter`
  - `Type Inference - Loaned Value Cannot Flow To Plain Parameter`
  - `Type Inference - Owned Value Requires Detach Before Plain Parameter`
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_type_inference_test -j 1` succeeded.
  - `zr_vm_type_inference_test`: new call-argument ownership diagnostics and prior assignment diagnostics passed; full target still reports the existing seven generic/import/interface failures.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_parser_test zr_vm_aot_c_ownership_contracts_test zr_vm_compiler_integration_test -j 1` succeeded.
  - `zr_vm_parser_test`: `73 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_aot_c_ownership_contracts_test`: `1 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_compiler_integration_test`: owner/borrow/loan using focused checks passed before the existing frame-layout and ownership runtime failures.
- Remaining work:
  - Structured diagnostic/LSP golden coverage for ownership escape diagnostics.
  - Field, closure, global, async/thread, and plugin-type region escape analysis.
  - Existing broader ownership runtime failures.

## Update: assignment expression and field assignment escape diagnostics

- Timestamp: 2026-06-17 17:51:25 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Extended ownership flow diagnostics into `ZrParser_AssignmentType_Infer`, so source-level assignments now use the same rules as the shared assignment compatibility helper:
  - `target = value` rejects Borrow/Loan/Unique/Shared flow into a plain target.
  - `box.slot = value` rejects Borrow/Loan/Unique/Shared flow into a plain field.
  - Borrow/Loan report their dedicated escape diagnostics.
  - Unique/Shared report the owner-to-plain diagnostic.
- Red/green evidence:
  - Before implementation, both new tests failed because assignment expression inference returned success.
  - After implementation, both tests passed and reported the dedicated ownership diagnostics.
- New checks passed:
  - `Type Inference - Ownership Escape Diagnostics Apply To Assignment Expressions`
  - `Type Inference - Ownership Escape Diagnostics Apply To Field Assignment Expressions`
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_type_inference_test -j 1` succeeded.
  - `zr_vm_type_inference_test`: new assignment-expression checks, call-argument checks, and prior assignment diagnostics passed; full target still reports the existing seven generic/import/interface failures.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_parser_test zr_vm_aot_c_ownership_contracts_test zr_vm_compiler_integration_test -j 1` succeeded.
  - `zr_vm_parser_test`: `73 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_aot_c_ownership_contracts_test`: `1 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_compiler_integration_test`: owner/borrow/loan using focused checks passed before the existing frame-layout and ownership runtime failures.
- Remaining work:
  - Structured diagnostic/LSP golden coverage for ownership escape diagnostics.
  - Closure, global, async/thread, and plugin-type region escape analysis.
  - Existing broader ownership runtime failures.

## Update: legacy ownership type deprecation warning

- Timestamp: 2026-06-17 18:45:34 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Kept `%unique/%shared/%weak/%borrow/%loan T` as compatibility syntax that still desugars to intrinsic ownership generic wrappers.
- Added structured parser warning `legacy_ownership_type_syntax` with migration suggestions such as `Write Unique<T> instead.`.
- Adjusted structured parser reporting so non-error diagnostics do not set parser hard-error state.
- Adjusted LSP incremental parser fallback logic so parser warnings are published but do not force fallback AST or suppress current semantic analysis.
- New checks:
  - `Legacy Ownership Type Syntax Reports Migration Warning`
  - `LSP Legacy Ownership Type Warning Preserves Current AST`
- Re-run evidence:
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_parser_test -j 1` succeeded after the initial long rebuild timed out.
  - `zr_vm_parser_test`: `74 Tests 0 Failures 0 Ignored OK`.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_language_server_shared -j 1` succeeded after an initial timeout/retry.
  - `ninja -C build/codex-using-wsl-gcc-debug tests/CMakeFiles/zr_vm_language_server_lsp_interface_test.dir/language_server/test_lsp_interface.c.o` succeeded.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 1` succeeded on re-run at 2026-06-17 19:27:29 +08:00.
  - `zr_vm_language_server_lsp_interface_test`: `LSP Legacy Ownership Type Warning Preserves Current AST` passed and `All LSP Interface Tests Completed`.
  - `cmake --build build/codex-using-wsl-gcc-debug --target zr_vm_type_inference_test -j 1` succeeded; `zr_vm_type_inference_test` still reports the existing seven generic/import/interface failures, while the P1 ownership checks before and around this slice pass.
- Remaining work:
  - Closure, global, async/thread, and plugin-type region escape analysis.
  - Existing broader ownership runtime failures.

## Update: exported global Borrow/Loan escape rejection

- Timestamp: 2026-06-17 19:13:32 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Added a compiler-side export boundary check for script-level `pub/pro var` declarations:
  - `Borrow<T>` and `Loan<T>` cannot be stored in exported globals.
  - The diagnostic is `Borrowed and loaned owners cannot escape through exported globals`.
  - Private top-level temporaries remain allowed for existing ownership opcode/runtime smoke tests.
- New checks:
  - `borrow-global-escape`
  - `loan-global-escape`
  - Both are included in `Ownership Builtin Compile Rejects Invalid Operands`.
- Re-run evidence:
  - `zr_vm_parser_shared` built successfully.
  - `zr_vm_compiler_integration_test` built successfully.
  - `Ownership Builtin Compile Rejects Invalid Operands` passed and reported the two exported-global escape diagnostics before the existing runtime failures.
  - `zr_vm_aot_c_ownership_contracts_test`: `1 Tests 0 Failures 0 Ignored OK`.
- Blocked / residual verification:
  - Full `zr_vm_compiler_integration_test` execution still hits existing frame-layout failures, existing ownership runtime failures, and a later runtime assertion after the new compile-time case passes.
- Remaining work:
  - Closure, async/thread, plugin-type, and fuller cross-region escape analysis.
  - Existing broader ownership runtime failures.

## Update: closure capture Borrow/Loan escape rejection

- Timestamp: 2026-06-17 19:50:09 +08:00.
- Build directory: `build/codex-using-wsl-gcc-debug`.
- Added a compiler-side closure capture boundary check:
  - Closure and nested-function external variable analysis now carries the parent compiler `typeEnv`.
  - Capturing a `Borrow<T>` or `Loan<T>` variable is rejected before the closure capture is recorded.
  - The diagnostic is `Borrowed and loaned owners cannot escape through closure capture`.
- New checks:
  - `borrow-closure-escape`
  - `loan-closure-escape`
  - Both are included in `Ownership Builtin Compile Rejects Invalid Operands`.
- Red/green evidence:
  - RED: after adding `borrow-closure-escape`, the old compiler still accepted the closure capture and the invalid-operands test failed.
  - GREEN: after implementation, `Ownership Builtin Compile Rejects Invalid Operands` passed and reported both closure-capture diagnostics before the existing runtime failures.
- Re-run evidence:
  - `zr_vm_parser_shared` built successfully.
  - `zr_vm_compiler_integration_test` built successfully.
- Blocked / residual verification:
  - Full `zr_vm_compiler_integration_test` execution still aborts later in existing frame-layout / ownership runtime paths after the new compile-time cases pass.
- Remaining work:
  - Async/thread, plugin-type, and fuller cross-region/global escape analysis.
  - Existing broader ownership runtime failures.

## Update: async/task explicit Loan<T> local await-boundary escape

- Timestamp: 2026-06-17 20:35:14 +08:00.
- Build directory: `build/codex-p1-async-wsl-gcc-debug`.
- Tightened task-effect validation for explicitly typed local variables:
  - `var value: Loan<Box> = null;` is now registered as a loaned binding.
  - Using that binding after `%await` reports `Loaned binding 'value' cannot be used after an await boundary`.
  - Generic `Borrow<T>` / `Loan<T>` async parameters are covered by regression tests and continue to report the existing await-boundary diagnostics.
- Red/green evidence:
  - RED: `test_generic_loan_typed_local_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: after registering explicit `Loan<T>` variable declarations as loaned bindings, the new test passed.
- Re-run evidence:
  - `cmake -S . -B build/codex-p1-async-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++` succeeded.
  - `cmake --build build/codex-p1-async-wsl-gcc-debug --target zr_vm_task_runtime_test -j1` succeeded.
  - `zr_vm_task_runtime_test`: new generic Borrow/Loan parameter regressions and `test_generic_loan_typed_local_cannot_cross_await_boundary` passed.
- Residual verification:
  - Full `zr_vm_task_runtime_test` still reports five existing failures in task runner generic inference and coroutine scheduler member access:
    - `test_borrowed_value_used_before_await_still_compiles`
    - `test_task_runner_start_and_await_execute_on_default_scheduler`
    - `test_task_runner_start_and_await_execute_with_explicit_async_return_type`
    - `test_coroutine_scheduler_manual_pump_executes_started_runner`
    - `test_default_scheduler_property_is_readable_and_writable`
- Remaining work:
  - Thread Send/Sync, plugin-type, and fuller async/cross-region escape analysis.
  - Existing broader ownership and task runtime failures.

## Update: async/task using guard else await-boundary escape

- Timestamp: 2026-06-17 21:11:28 +08:00.
- Build directory: `build/codex-p1-async-wsl-gcc-debug`.
- Tightened task-effect traversal for `using` guard statements:
  - `ZR_AST_USING_STATEMENT.elseBody` is now validated after the resource and main body.
  - A guard `else` branch that crosses `%await` and then reads a `Borrow<T>` / `Loan<T>` binding reports the same await-boundary escape as the main branch.
- Red/green evidence:
  - RED: `test_using_else_branch_borrow_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: after validating `elseBody`, the new test passed and reported `Borrowed binding 'value' cannot be used after an await boundary`.
- Re-run evidence:
  - `cmake --build build/codex-p1-async-wsl-gcc-debug --target zr_vm_task_runtime_test -j1` succeeded.
  - `zr_vm_task_runtime_test`: new `test_using_else_branch_borrow_cannot_cross_await_boundary` passed.
- Residual verification:
  - Full `zr_vm_task_runtime_test`: `20 Tests 5 Failures 0 Ignored`; failures are the existing task runner generic inference and `coroutineScheduler` member access cases.
- Remaining work:
  - Thread Send/Sync, plugin-type, and fuller async/global/cross-region escape analysis.
  - Existing broader ownership and task runtime failures.

## Update: thread Send/Sync nested owner generic escape

- Timestamp: 2026-06-17 21:43:31 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened thread marker constraint checks:
  - `Send` / `Sync` checks now recursively scan generic argument `elementTypes` for `Borrow<T>` / `Loan<T>` / `Shared<T>` / `Weak<T>` ownership qualifiers.
  - Native `Send` / `Sync` short-name implementations are treated as equivalent to `zr.thread.Send` / `zr.thread.Sync` constraints only for imported native prototypes.
  - `Transfer<Borrow<T>>` and `Shared<Loan<T>>` no longer satisfy thread marker constraints by hiding the owner inside a wrapper generic.
- Red/green evidence:
  - RED: `test_thread_markers_reject_nested_isolate_alias_generic_arguments` initially failed because a nested borrowed generic argument still satisfied `Send`.
  - GREEN: after recursive ownership scanning, the new test passed.
- Re-run evidence:
  - `cmake --build build/codex-p1-thread-wsl-gcc-debug --target zr_vm_thread_runtime_test -j1` succeeded.
  - `zr_vm_thread_runtime_test`: `test_thread_markers_reject_nested_isolate_alias_generic_arguments` passed.
- Residual verification:
  - Full `zr_vm_thread_runtime_test`: `21 Tests 10 Failures 0 Ignored`; failures are existing task runner generic inference, generic construct target, and cached native member callable cases.
- Remaining work:
  - Plugin-type and fuller async/global/cross-region escape analysis.
  - Existing broader ownership, task, and thread runtime failures.

## Update: nested ownership generic argument escape rejection

- Timestamp: 2026-06-17 22:05:16 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened ordinary assignment/type-compatibility paths:
  - Ownership flow diagnostics now recursively inspect generic argument `elementTypes`.
  - Same-name generic shells now compare target argument compatibility instead of returning success before checking arguments.
  - `Box<Borrow<T>>` and `Box<Loan<T>>` report borrow/loan escape when assigned to plain `Box<T>`.
  - `Box<Unique<T>>` and `Box<Shared<T>>` report owner-to-plain escape when assigned to plain `Box<T>`.
- Red/green evidence:
  - RED: `test_nested_ownership_generic_arguments_cannot_escape_through_assignment` initially failed because nested `Borrow<T>` inside `Box<T>` was accepted.
  - GREEN: after recursive ownership diagnostics and generic argument compatibility checks, the new test passed.
- Re-run evidence:
  - `cmake --build build/codex-p1-thread-wsl-gcc-debug --target zr_vm_type_inference_test -j1` succeeded.
  - `zr_vm_type_inference_test`: `test_nested_ownership_generic_arguments_cannot_escape_through_assignment` passed.
- Residual verification:
  - Full `zr_vm_type_inference_test`: `115 Tests 7 Failures 0 Ignored`; failures are existing native/source generic import and interface-member flow cases.
- Remaining work:
  - Plugin-type and fuller async/global/cross-region escape analysis.
  - Existing broader ownership, task, and thread runtime failures.

## Update: nested exported-global Borrow/Loan generic escape

- Timestamp: 2026-06-17 22:59:59 +08:00.
- Build directories:
  - `build/codex-wsl-clang-asan`
  - `build/codex-p1-thread-wsl-gcc-debug`
- Tightened compiler escape boundaries:
  - `pub/pro var escaped: Holder<Borrow<T>>` is rejected.
  - `pub/pro var escaped: Holder<Loan<T>>` is rejected.
  - The exported-global and return checks now share a recursive `elementTypes` borrow/loan scan.
- Red/green evidence:
  - RED: `nested-borrow-global-escape` initially failed because the old compiler accepted the exported global declaration.
  - GREEN: after recursive boundary scanning, the focused ownership invalid-operands test passed and reported nested borrow/loan exported-global diagnostics.
- Re-run evidence:
  - `cmake --build build/codex-wsl-clang-asan --target zr_vm_parser_shared -j1` succeeded.
  - `cmake --build build/codex-wsl-clang-asan --target zr_vm_compiler_integration_test -j1` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK`.
  - `cmake --build build/codex-p1-thread-wsl-gcc-debug --target zr_vm_parser_shared -j1` succeeded.
- Residual verification:
  - Full ASAN `zr_vm_compiler_integration_test` still aborts at existing `test_function_parameter_handling` stack-use-after-scope before reaching the ownership block.
  - Full `zr_vm_type_inference_test`: `115 Tests 7 Failures 0 Ignored`; the nested assignment ownership test still passes.
  - Full `zr_vm_thread_runtime_test`: `21 Tests 10 Failures 0 Ignored`; the nested thread marker ownership test still passes.
- Remaining work:
  - Plugin-type and fuller async/global/cross-region escape analysis.
  - Existing broader ownership, task, and thread runtime failures.

## Update: nested closure-capture Borrow/Loan generic escape

- Timestamp: 2026-06-17 23:33:55 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened compiler closure capture boundary:
  - `var nested: Holder<Borrow<T>>` cannot be captured by a closure or nested function.
  - `var nested: Holder<Loan<T>>` cannot be captured by a closure or nested function.
  - `compiler_closure.c` now recursively scans captured variable `SZrInferredType.elementTypes` for borrow/loan ownership.
- Red/green evidence:
  - RED: `nested-borrow-closure-escape` initially failed because the old compiler accepted the closure capture.
  - GREEN: after recursive capture scanning, the focused ownership invalid-operands test passed and reported nested closure capture diagnostics.
- Re-run evidence:
  - `cmake --build build/codex-p1-thread-wsl-gcc-debug --target zr_vm_compiler_integration_test -j1` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK`.
- Residual verification:
  - ASAN parser rebuild is currently blocked by a parallel-session compile error in `type_inference_member_resolution.c`.
- Remaining work:
  - Plugin-type and fuller async/global/cross-region escape analysis.
  - Existing broader ownership, task, and thread runtime failures.

## Update: nested async/task Borrow/Loan generic await escape

- Timestamp: 2026-06-17 23:56:29 +08:00.
- Build directory: `build/codex-p1-async-wsl-gcc-debug`.
- Tightened task effect await-boundary checks:
  - `Holder<Borrow<T>>` typed locals cannot be read after `%await`.
  - `Holder<Loan<T>>` typed locals cannot be read after `%await`.
  - `compiler_task_effects.c` now recursively scans explicit AST type `subType`, generic arguments, and tuple elements for Borrow/Loan ownership.
- Red/green evidence:
  - RED: `test_nested_generic_borrow_typed_local_cannot_cross_await_boundary` and `test_nested_generic_loan_typed_local_cannot_cross_await_boundary` initially failed because the old validator accepted both locals after `%await`.
  - GREEN: after recursive AST type scanning, both nested typed local tests passed and reported the expected await-boundary diagnostics.
- Re-run evidence:
  - `cmake --build build/codex-p1-async-wsl-gcc-debug --target zr_vm_task_runtime_test -j1` succeeded.
  - `zr_vm_task_runtime_test`: nested Borrow/Loan typed local await-boundary tests passed.
- Residual verification:
  - Full `zr_vm_task_runtime_test`: `22 Tests 5 Failures 0 Ignored`; failures are existing task runner generic inference and `coroutineScheduler` member access cases.
- Remaining work:
  - Plugin-type and fuller async/global/cross-region escape analysis.
  - Existing broader ownership, task, and thread runtime failures.

## Update: plugin guard basic escape rejection

- Timestamp: 2026-06-18 00:28:12 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened `using (var p = %import(...))` guard scope boundaries:
  - Guard binders and guarded-block aliases cannot be returned directly.
  - Guarded member values such as `p.member` cannot be returned directly.
  - Guarded values cannot be captured by closures or nested functions without an explicit future `share()` path.
- Red/green evidence:
  - RED: `plugin-guard-return-escape` initially failed because the old compiler accepted `return math` from inside the guard block.
  - GREEN: after the conservative guard-body scan in `compile_statement.c`, `plugin-guard-return-escape`, `plugin-guard-member-return-escape`, and `plugin-guard-closure-escape` report `plugin_type_escape`.
- Re-run evidence:
  - `cmake --build /mnt/e/Git/zr_vm/build/codex-p1-thread-wsl-gcc-debug --target zr_vm_compiler_integration_test -j1` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK`.
  - `git diff --check -- tests/parser/test_compiler_features.c zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c` exited 0 with LF/CRLF normalization warnings only.
- Residual verification:
  - The full compiler integration binary continues into an existing runtime assertion after the focused ownership-invalid window.
- Remaining work:
  - `share()` promotion, plugin field/argument/cross-region escape analysis, scoped plugin release, and `PluginLoad.Available` lowering.
  - Existing broader ownership, task, thread, and integration runtime failures.

## Update: plugin guard call-argument escape rejection

- Timestamp: 2026-06-18 01:02:16 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened `using (var p = %import(...))` guard argument boundaries:
  - The plugin guard escape scanner is now isolated in `compiler_using_plugin_guard_escape.c/.h`.
  - Guard binders, guarded-block aliases, and callable member references cannot be passed as function call arguments without a future explicit `share()` path.
  - `sink(math)` inside a guard block reports `plugin_type_escape ... through call argument`.
  - Expression statements, variable initializers, and assignment right-hand call arguments reuse the same scan.
  - Control-flow conditions and iteration expressions reuse the same scan, so `if (sink(math))` also reports call-argument escape.
  - Ordinary guarded plugin calls such as `math.abs(...)` still compile as result-typed calls, not plugin handle escapes.
- Red/green evidence:
  - RED: `plugin-guard-call-argument-escape` initially failed because the old compiler accepted `sink(math)`.
  - GREEN: after the call-argument scan, the focused ownership invalid-operands test passed and reported the expected call-argument diagnostic.
  - Follow-up RED/GREEN: `plugin-guard-if-call-argument-escape` initially failed because the old statement scan skipped `if` conditions; after adding the control-flow condition scan it reports `plugin_type_escape ... through call argument`.
- Re-run evidence:
  - `cmake --build /mnt/e/Git/zr_vm/build/codex-p1-thread-wsl-gcc-debug --target zr_vm_compiler_integration_test -j1` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK`.
  - `ninja zr_vm_compiler_integration_test` in `build/codex-p1-thread-wsl-gcc-debug` succeeded after the control-flow condition follow-up.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK` with both call-argument cases.
  - `cmake --build /mnt/e/Git/zr_vm/build/codex-p1-thread-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test -j1` succeeded.
  - `zr_vm_project_import_canonicalization_test`: `12 Tests 0 Failures 0 Ignored OK`.
- Residual verification:
  - The full compiler integration binary still continues into an existing runtime assertion after the focused ownership-invalid window.
- Remaining work:
  - `share()` promotion, plugin field/cross-region escape analysis, scoped plugin release, and `PluginLoad.Available` lowering.
  - Existing broader ownership, task, thread, and integration runtime failures.

## Update: plugin guard field/container escape rejection

- Timestamp: 2026-06-18 01:44:35 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened guard-scoped plugin persistence checks:
  - Direct block-local aliases such as `var alias = math` remain allowed and continue to be tracked by the guard scanner.
  - Object-field persistence such as `var box = { handle: math }` is rejected.
  - Array-element persistence such as `var handles = [math]` is rejected.
  - Object, array, key-value, and unpack literal traversal also carries the existing call-argument and closure-capture scans.
- Red/green evidence:
  - RED: `plugin-guard-object-field-escape` initially failed because the old scanner accepted `var box = { handle: math }`.
  - GREEN: after adding aggregate literal scanning, `plugin-guard-object-field-escape` reports `plugin_type_escape ... through field/container`.
  - GREEN: the same focused run also covers `plugin-guard-array-element-escape`, which reports the same diagnostic for `[math]`.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test` in `build/codex-p1-thread-wsl-gcc-debug` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK` with object-field and array-element cases.
  - Final re-run at 2026-06-18 01:53:52 +08:00 confirmed both object-field and array-element diagnostics still report `plugin_type_escape ... through field/container`.
  - `zr_vm_project_import_canonicalization_test`: `12 Tests 0 Failures 0 Ignored OK`.
  - `git diff --check` exited 0 with LF/CRLF normalization warnings only; trailing-whitespace scan on touched docs and scanner files produced no matches.
- Residual verification:
  - The full compiler integration binary still continues past the focused ownership-invalid window into existing frame-layout failures, existing ownership runtime failures, and a later runtime assertion.
- Remaining work:
  - `share()` promotion, cross-region plugin escape analysis, scoped plugin release, and `PluginLoad.Available` lowering.
  - Existing broader ownership, task, thread, and integration runtime failures.

## Update: typed and no-annotation import guard payload escape reuse

- Timestamp: 2026-06-18 02:12:41 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Tightened `DynamicModule<T>` import guards:
  - `compile_using_import_variant_guard_statement()` now registers typed and no-annotation `%import` payload bindings with `compiler_using_plugin_guard_escape.c/.h` before compiling the guarded body.
  - `using (var [m]: DynamicModule<Plugins> = %import("zr.plugins")) { return m; }` is rejected as `plugin_type_escape ... through return`.
  - `using (var [m] = %import("zr.plugins")) { return m; }` follows the same default `@Available` lowering and is rejected by the same scanner.
  - This reuses the same scanner as plain plugin guard handles, aliases, callable member references, call arguments, aggregate persistence, and closure capture.
- Red/green evidence:
  - RED: `typed-plugin-guard-return-escape` initially failed because the old compiler accepted `return m`.
  - GREEN: after adding the payload binding scanner entry, the focused ownership invalid-operands test passed and reported the expected return-escape diagnostic.
  - Follow-up coverage: no-annotation payload return/closure/call/object/array escape cases are included in the same invalid-operands matrix.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test` in `build/codex-p1-thread-wsl-gcc-debug` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: `1 Tests 0 Failures 0 Ignored OK` with `typed-plugin-guard-return-escape`.
  - `zr_vm_union_test`: `35 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_project_import_canonicalization_test`: `12 Tests 0 Failures 0 Ignored OK`.
- Residual verification:
  - The full compiler integration binary still continues past the focused ownership-invalid window into existing frame-layout failures, existing ownership runtime failures, and a later runtime assertion.
- Remaining work:
  - `share()` promotion, cross-region plugin escape analysis, scoped plugin release, and complete load/verify.
  - Existing broader ownership, task, thread, and integration runtime failures.

## Update: typed and no-annotation import guard payload escape matrix closeout

- Timestamp: 2026-06-18 02:30:06 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Scope:
  - Typed and no-annotation `DynamicModule<T>.@Available` `%import` guard payload bindings both reuse `ZrParser_Compiler_ValidateUsingPluginGuardEscapeBindings()`.
  - The invalid-operands matrix covers payload escape through return, closure capture, call argument, object field, and array element.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: passed, including typed and no-annotation payload escape cases.
  - `zr_vm_union_test`: `35 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_project_import_canonicalization_test`: `13 Tests 0 Failures 0 Ignored OK`.
- Residual verification:
  - The full compiler integration binary still continues past the focused ownership-invalid window into existing frame-layout failures, existing ownership runtime failures, and a later runtime assertion.
- Remaining work:
  - `share()` promotion, complete cross-region plugin escape analysis, scoped plugin release, complete load/verify, and owner payload move/borrow semantics.

## Update: plugin guard nested-region assignment escape

- Timestamp: 2026-06-18 02:46:25 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Scope:
  - `compiler_using_plugin_guard_escape.c` now preserves plugin alias taint assigned to an outer local from inside a nested block, `if`, or loop scan.
  - Inner-region locals are still discarded when leaving the nested region; only aliases that refer to locals already visible in the outer region are merged back.
  - `using (var [m] = %import("zr.plugins")) { var alias; if (flag) { alias = m; } return alias; }` is rejected as `plugin_type_escape ... through return`.
- Red/green evidence:
  - RED: `untyped-plugin-guard-if-assignment-return-escape` initially failed because `alias = m` inside `if` did not keep `alias` plugin-tainted after the nested block.
  - GREEN: after preserving outer-local aliases on nested-region exit, the focused invalid-operands window reports the expected return escape.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test` in `build/codex-p1-thread-wsl-gcc-debug` succeeded.
  - Narrow `Ownership Builtin Compile Rejects Invalid Operands`: passed, including `untyped-plugin-guard-if-assignment-return-escape`.
  - `zr_vm_union_test`: `35 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_project_import_canonicalization_test`: `15 Tests 0 Failures 0 Ignored OK`.
  - `git diff --check` on touched implementation/test/docs exited 0 with LF/CRLF normalization warnings only.
- Residual verification:
  - The full compiler integration binary still continues past the focused ownership-invalid window into existing frame-layout failures, existing ownership runtime failures, and a later runtime assertion.
- Remaining work:
  - `share()` promotion, more complete cross-region/global/async plugin escape analysis, scoped plugin release, complete load/verify, and owner payload move/borrow semantics.
  - Existing broader ownership, task, thread, and integration runtime failures.

## Update: plugin guard share promotion

- Timestamp: 2026-06-18 03:25:06 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Scope:
  - `compile_expression_types.c` now lowers no-arg `.share()` on a guarded module prototype to `ZrCore_Ownership_NativeSharePlain`.
  - The helper/result slot uses the expression target slot and the argument slot is kept immediately after it, matching the native call frame layout.
  - Identifier receivers use the original local slot as the share source, so `math.share()` does not clear the guard-scoped plain module binding.
  - `var handle = math.share();` now yields a non-null shared owner while `math` remains non-null inside the guard; `%release(handle)` clears the shared handle.
- Red/green evidence:
  - RED: `Plugin Guard Share Promotes Module Handle To Shared Owner` initially executed with an unexpected mask (`math`/`handle` lifecycle mismatch).
  - GREEN: after using the target/result slot and contiguous helper argument layout, the test passed.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test` in `build/codex-p1-thread-wsl-gcc-debug` succeeded.
  - Focused integration run reached and passed both `Ownership Builtin Compile Rejects Invalid Operands` and `Plugin Guard Share Promotes Module Handle To Shared Owner`.
  - Full `zr_vm_compiler_integration_test` execution still later aborts at the existing `execution_dispatch.c:5711` assertion.
  - `zr_vm_union_test`: `36 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_project_import_canonicalization_test`: `15 Tests 0 Failures 0 Ignored OK`.
  - `git diff --check` exited 0 with LF/CRLF normalization warnings only.
- Remaining work:
  - Scoped plugin release, complete load/verify, complete ref-to-def binding, and more complete cross-region/global/async plugin escape analysis.
  - Existing broader ownership, task, thread, and integration runtime failures.

## Update: ownership runtime frame-layout stabilization

- Timestamp: 2026-06-18 08:00:22 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Scope:
  - Weak reference expiry now clears only stack slots that are still weak and still point at the expiring weak ref.
  - Direct and tail VM frame reuse copies byte-backed VALUE parameters from logical frame slots and mirrors them into dense slots.
  - VALUE parameter destination reuse releases any old owner before overwriting, including inline-frame drop paths.
  - Union carrier owner payload materialization consumes the carrier/source ownership metadata after moving the active payload into inline storage.
  - Meta/dynamic/native call staging uses frame-layout-aware call windows, and ownership/typed equality/compare/fused branch paths read logical frame value slots instead of stale physical stack slots.
- Red/green evidence:
  - RED: the new lifecycle regression first failed in direct recursion/tail reuse when a VALUE parameter frame slot was not copied into the reused frame.
  - RED: the same regression then exposed meta-call staging, typed equality/branch, and union owner payload ownership metadata holes as the execution advanced.
  - GREEN: after the frame-layout/runtime fixes, `Ownership Release Preserves Unrelated Stack Values After Weak Expiry` passes through direct recursion, `@call` meta recursion, try/catch/finally, weak upgrade, release, and alias release.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test` in `build/codex-p1-thread-wsl-gcc-debug` succeeded.
  - Focused compiler integration output shows these ownership runtime tests passing: unique share move, borrow/loan/detach lifecycle, weak expiry, upgrade/release lifecycle, weak-expiry unrelated-stack preservation, detach multi-owner rejection, and direct/loop child known VM call quickening.
  - Full `zr_vm_compiler_integration_test` now runs to completion with `113 Tests 23 Failures 0 Ignored FAIL`; the remaining failures are existing frame-layout/network/project/quickening/typed member-call baseline failures, not a new ownership runtime assertion.
  - `zr_vm_union_test`: `57 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_gc_test`: `66 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_project_import_canonicalization_test`: `23 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - Scoped plugin release/share lifecycle, `PluginLoad.Available`, registry/DLL load+verify, and complete cross-region/global/async plugin escape analysis.
  - Full compiler integration baseline failures still need separate convergence.

## Update: plugin guard scoped release/share lifecycle

- Timestamp: 2026-06-18 08:44:43 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Scope:
  - Plain `%import` guard success now creates a hidden shared owner for the guard-scoped plain module handle and registers scope cleanup.
  - Default `DynamicModule<T>.@Available` `%import` payload success follows the same hidden-owner cleanup path.
  - The visible guard binding remains a plain module value; explicit `.share()` still creates an additional releasable shared owner.
- Red/green evidence:
  - RED: `Plugin Guard Scoped Module Handle Releases On Scope Exit` initially failed because the compiled function had no ownership share helper constant for the implicit scoped owner.
  - GREEN: after reusing the module plain share helper from `compile_statement.c`, both plain guard and default payload paths contain `ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN`, exactly one `OWN_RELEASE`, and execute the body result.
- Re-run evidence:
  - `ninja zr_vm_compiler_integration_test zr_vm_union_test zr_vm_gc_test zr_vm_project_import_canonicalization_test zr_vm_task_runtime_test` succeeded.
  - `zr_vm_compiler_integration_test`: new scoped release test and existing `.share()` promotion test PASS; full target remains `114 Tests 23 Failures 0 Ignored FAIL` with existing baseline failures.
  - `zr_vm_union_test`: `57 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_gc_test`: `66 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_project_import_canonicalization_test`: `26 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_task_runtime_test`: plugin guard await-boundary cases PASS; full target remains `25 Tests 5 Failures 0 Ignored FAIL`.
- Remaining work:
  - `PluginLoad.Available`, registry-level unload/refcount API, complete cross-region/global/async plugin escape analysis, and full compiler integration baseline convergence.

## Update: plugin guard conditional alias await-boundary escape

- Timestamp: 2026-06-18 14:04:44 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now treats `var alias = flag ? plugin : null` as a plugin guard alias when either conditional branch carries a plugin guard binding.
  - `%await` followed by reading that alias reports `Plugin guard binding 'alias' cannot be used after an await boundary`.
- Red/green evidence:
  - RED: `test_plugin_guard_conditional_alias_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: `compiler_task_effects.c` now merges propagated binding kinds from `ZR_AST_CONDITIONAL_EXPRESSION` branches before registering variable initializer effects.
- Re-run evidence:
  - `cmake --build build/codex-metadata-token-wsl-gcc-debug --target zr_vm_task_runtime_test -- -j2` succeeded.
  - `zr_vm_task_runtime_test`: new conditional alias await-boundary test PASSed; full target remains `26 Tests 5 Failures 0 Ignored FAIL` with existing task runner/scheduler failures.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_metadata_token_model_test`: `15 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Existing broader task/runtime baseline failures.

## Update: plugin guard logical alias await-boundary escape

- Timestamp: 2026-06-18 14:27:45 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now treats `var alias = plugin || null` as a plugin guard alias when either logical-expression operand carries a plugin guard binding.
  - `%await` followed by reading that alias reports `Plugin guard binding 'alias' cannot be used after an await boundary`.
- Red/green evidence:
  - RED: `test_plugin_guard_logical_alias_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: `compiler_task_effects.c` now merges propagated binding kinds from `ZR_AST_LOGICAL_EXPRESSION` operands before registering variable initializer effects.
- Re-run evidence:
  - `cmake --build build/codex-metadata-token-wsl-gcc-debug --target zr_vm_task_runtime_test zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -- -j2` succeeded.
  - `zr_vm_task_runtime_test`: new logical alias await-boundary test PASSed; full target remains `27 Tests 5 Failures 0 Ignored` with existing task runner/scheduler failures.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_metadata_token_model_test`: `16 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Existing broader task/runtime baseline failures.

## Update: plugin guard cast alias await-boundary escape

- Timestamp: 2026-06-18 14:50:42 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now treats `var alias = <object> plugin` as a plugin guard alias when the source expression carries a plugin guard binding.
  - `%await` followed by reading that alias reports `Plugin guard binding 'alias' cannot be used after an await boundary`.
- Red/green evidence:
  - RED: `test_plugin_guard_cast_alias_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: `compiler_task_effects.c` now forwards propagated binding kinds through `ZR_AST_TYPE_CAST_EXPRESSION`.
- Re-run evidence:
  - `cmake --build build/codex-metadata-token-wsl-gcc-debug --target zr_vm_task_runtime_test -- -j2` succeeded.
  - `zr_vm_task_runtime_test`: new cast alias await-boundary test PASSed; full target remains `28 Tests 5 Failures 0 Ignored` with existing task runner/scheduler failures.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_metadata_token_model_test`: `16 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Existing broader task/runtime baseline failures.

## Update: plugin guard assignment-expression alias await-boundary escape

- Timestamp: 2026-06-18 15:20:15 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now treats `var alias = (temp = plugin)` as a plugin guard alias when the inner assignment is plain `=`.
  - The right-hand plugin guard binding kind is preserved for the outer initializer; compound assignments remain non-propagating.
  - `%await` followed by reading that alias reports `Plugin guard binding 'alias' cannot be used after an await boundary`.
- Red/green evidence:
  - RED: `test_plugin_guard_assignment_expression_alias_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: `compiler_task_effects.c` now forwards propagated binding kinds from `ZR_AST_ASSIGNMENT_EXPRESSION.right` for plain `=` assignment expressions.
- Re-run evidence:
  - `ninja -C build/codex-metadata-token-wsl-gcc-debug zr_vm_task_runtime_test` succeeded.
  - `zr_vm_task_runtime_test`: new assignment-expression alias await-boundary test PASSed; full target remains `29 Tests 5 Failures 0 Ignored` with existing task runner/scheduler failures.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_metadata_token_model_test`: `17 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Existing broader task/runtime baseline failures.

## Update: plugin guard generator body await-boundary escape

- Timestamp: 2026-06-18 15:39:35 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now visits `ZR_AST_GENERATOR_EXPRESSION.block`.
  - A guarded plugin binding used inside `{{ ... %await task; out plugin; }}` is checked against the same await boundary as ordinary block statements.
  - The diagnostic is `Plugin guard binding 'plugin' cannot be used after an await boundary`.
- Red/green evidence:
  - RED: `test_plugin_guard_generator_body_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: `compiler_task_effects.c` now validates the generator expression block.
- Re-run evidence:
  - `ninja -C build/codex-metadata-token-wsl-gcc-debug zr_vm_task_runtime_test` succeeded.
  - `zr_vm_task_runtime_test`: new generator body await-boundary test PASSed; full target remains `30 Tests 5 Failures 0 Ignored` with existing task runner/scheduler failures.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_metadata_token_model_test`: `17 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Existing broader task/runtime baseline failures.

## Update: plugin guard template interpolation await-boundary escape

- Timestamp: 2026-06-18 15:57:10 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now visits `ZR_AST_TEMPLATE_STRING_LITERAL.segments`.
  - `ZR_AST_INTERPOLATED_SEGMENT.expression` is validated in the same async context.
  - A guard binding used in a template interpolation after `%await` now reports the plugin guard await-boundary diagnostic.
- TDD evidence:
  - RED: `test_plugin_guard_template_interpolation_cannot_cross_await_boundary` initially failed because `compiler_validate_task_effects` returned success.
  - GREEN: adding template string and interpolation traversal in `compiler_task_effects.c` made the new task runtime case PASS.
- Rerun evidence:
  - `zr_vm_task_runtime_test`: new template interpolation await-boundary test PASSed; full target remains `31 Tests 5 Failures 0 Ignored` with existing task runner/scheduler failures.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_metadata_token_model_test`: currently `19 Tests 1 Failures 0 Ignored`; the failing `test_union_type_spec_binding_reports_layout_mismatch_without_partial_binding` belongs to the active metadata TypeSpec/layout binding follow-up, not this task-effect slice.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Complete load+verify closure.
  - Owner payload move semantics.

## Update: plugin guard template interpolation compile-time escape

- Timestamp: 2026-06-18 16:18:50 +08:00.
- Build directory: `build/codex-p1-thread-wsl-gcc-debug`.
- Scope:
  - The plugin guard compile-time scanner now treats template strings as expression containers.
  - Template literal segments and interpolation expressions participate in scoped-value, field/container, call-argument, closure-capture, and side-effect scans.
  - ``var text = `module ${math}``` inside a `%import` guard now reports `plugin_type_escape ... through field/container`.
- Red/green evidence:
  - RED: `plugin-guard-template-interpolation-escape` was initially accepted, causing the invalid-operands test to fail.
  - GREEN: `compiler_using_plugin_guard_escape_expression.c` now handles `ZR_AST_TEMPLATE_STRING_LITERAL` and `ZR_AST_INTERPOLATED_SEGMENT`.
- Re-run evidence:
  - `ninja -C build/codex-p1-thread-wsl-gcc-debug zr_vm_compiler_integration_test` succeeded.
  - `zr_vm_compiler_integration_test`: invalid-operands new case PASSed; full target returned from RED `116 Tests 24 Failures 0 Ignored` to the existing `116 Tests 23 Failures 0 Ignored` baseline.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis.
  - Complete load+verify closure.
  - Owner payload move semantics.

## Update: task-effect declaration/member body await-boundary traversal

- Timestamp: 2026-06-18 16:50:02 +08:00.
- Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
- Scope:
  - Task-effect validation now traverses class and struct declaration members instead of skipping declaration nodes.
  - Class/struct field initializers, ordinary methods, meta functions, property getter/setter bodies, and enum member values now reuse the same node/function-like validation as top-level functions and lambdas.
  - Ordinary class/struct methods are still non-`%async`; `%await` inside those method bodies reports `%await is only allowed inside %async bodies or scheduler-managed top-level coroutines`.
- TDD evidence:
  - RED: `test_task_effects_reject_await_inside_class_method` initially failed because `compiler_validate_task_effects` returned success, raising task runtime from the existing `31 Tests 5 Failures` to `32 Tests 6 Failures`.
  - GREEN: `compiler_task_effects_declarations.c` and `compiler_task_effects_internal.h` split declaration/member traversal out of `compiler_task_effects.c`, and both class/struct method `%await` cases PASS.
- Rerun evidence:
  - `ninja -C build/codex-metadata-token-wsl-gcc-debug zr_vm_task_runtime_test`: succeeded.
  - `zr_vm_task_runtime_test`: `33 Tests 5 Failures 0 Ignored`; the five failures are the existing task runner/scheduler baseline, while the two new declaration/member traversal cases PASS.
  - `zr_vm_project_import_canonicalization_test`: `31 Tests 0 Failures 0 Ignored OK`.
  - `zr_vm_compiler_integration_test`: `116 Tests 23 Failures 0 Ignored`, matching the existing compiler integration baseline.
  - `zr_vm_metadata_token_model_test`: currently `21 Tests 1 Failures 0 Ignored`; failing `test_module_signature_hash_changes_with_union_type_def_contract` belongs to the active P0 metadata/union TypeDef module hash follow-up, not this task-effect slice.
- Remaining work:
  - More complete cross-function/global async plugin escape analysis beyond declaration/member traversal.
  - Complete load+verify closure.
  - Owner payload move semantics.
