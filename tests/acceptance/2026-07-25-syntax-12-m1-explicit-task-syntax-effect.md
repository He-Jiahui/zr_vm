# Syntax 12 M1 Acceptance: Explicit Task Syntax And Effect

## Scope

This acceptance covers only Syntax 12 M1 from
`docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md`.
It establishes explicit `zr.task.Task<T>` syntax and static async effects. It
does not claim Task frame lowering, Task execution, Job/Scheduler support, or
legacy `TaskRunner` migration.

## Accepted Contract

- The OfficialNative `zr.task.Task<T>` descriptor role is the only Task
  carrier proof. Imports and source aliases resolve to that role; spelling
  does not create an equivalent carrier.
- Named functions, class/struct members, and `async fn` lambdas retain their
  explicit source Task return type. Their effect is derived from the
  declaration, and named callable `TypeId` registration carries
  `ZR_CANONICAL_CALLABLE_EFFECT_ASYNC`.
- `await` is `ZR_AST_AWAIT_EXPRESSION`; only a closed Task carrier supplies
  its payload type. Non-async use, ref-like parameters/payloads, and invalid
  result forms fail before runtime lowering.
- The CFG exposes `SUSPEND -> JOIN -> RESUME` around direct await in an
  expression statement, return, and variable initializer. The same node is a
  reference-escape suspension boundary.

## Validation Evidence

Each toolchain used its own build directory and ran the same focused targets
after the final public-header rebuild.

| Toolchain | Build targets | CFG | Type inference | Task target |
|---|---|---:|---:|---|
| GCC 11.4 | `zr_vm_place_cfg_graph_test`, `zr_vm_type_inference_test`, `zr_vm_task_runtime_test` | 6/6, exit 0 | 119/119, exit 0 | 54 tests; M1 regressions pass; exit 1 because of 4 documented legacy failures |
| Clang 14 | same | 6/6, exit 0 | 119/119, exit 0 | 54 tests; M1 regressions pass; exit 1 because of 4 documented legacy failures |
| MSVC 19.44 | same | 6/6, exit 0 | 119/119, exit 0 | 54 tests; M1 regressions pass; exit 4 because Unity returns the four failure count |

The focused M1 regressions cover explicit named Task signatures, import alias
identity, direct await AST/payload inference, non-Task rejection, async scope,
borrow crossing, async lambda, async class/struct members, canonical async
effect projection, and variable-initializer suspension CFG.

## Known Baseline Outside M1

All three task logs contain the same four pre-existing failures:

- `test_borrowed_value_used_before_await_still_compiles`
- `test_task_runner_start_and_await_execute_on_default_scheduler`
- `test_task_runner_start_and_await_execute_with_explicit_async_return_type`
- `test_coroutine_scheduler_manual_pump_executes_started_runner`

They fail while resolving the legacy `__createTaskRunner` generic path. This
is a Syntax 12 M6 migration and legacy-runtime concern, not a relaxed M1
assertion. The M1 commit preserves that path and does not count these failures
as passing evidence.
