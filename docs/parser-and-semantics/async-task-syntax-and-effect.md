---
related_code:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
plan_sources:
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
  - docs/plans/syntax/12-async-task-job-scheduler/m1-explicit-task-syntax-effect-implementation-plan.md
tests:
  - tests/task/test_task_runtime.c
  - tests/parser/test_place_cfg_graph.c
  - tests/acceptance/2026-07-25-syntax-12-m1-explicit-task-syntax-effect.md
doc_type: module-contract
---

# Async Task Syntax And Effect

Syntax 12 M1 establishes the source-level contract for explicit task-returning
callables. It deliberately does not select a frame layout, lowering strategy,
or scheduler runtime; those are later Syntax 12 milestones.

## Source Contract

An `async` callable retains its source return TypeRef. Named functions,
class/struct members, and `async fn` lambdas must declare one closed
`zr.task.Task<T>` carrier and share the canonical async callable effect:

```zr
async fn fetch(pending: zr.task.Task<int>): zr.task.Task<int> {
    return await pending;
}
```

The Task carrier is established by the official native `zr.task` descriptor's
`ZR_PROTOCOL_ID_TASK_HANDLE` role. A source alias such as
`var task = %import("zr.task")` resolves to the same owner and can use
`task.Task<T>`. A same-named source type or another module's `Task` does not
gain the role from spelling alone.

Async parameters are value parameters only. `in`, `ref`, `out`, and ref-like
parameter or payload forms are rejected before execution lowering. An async
callable whose return is not a closed Task carrier is rejected at the same
canonical validation point.

## Await Semantics

`await expression` has its own `ZR_AST_AWAIT_EXPRESSION` node. Type inference
first infers the operand, requires the Task-handle protocol and exactly one
payload argument, then returns that payload TypeId. It never extracts a type
from a name, member text, or diagnostic message.

The effect pass permits direct await only in an async callable or the existing
scheduler-managed top-level context. The reference-escape pass treats the
node as a suspension boundary. The CFG builder represents direct await in an
expression statement, return, or variable initializer as a normal predecessor
into a `SUSPENSION` block, a typed `SUSPEND` edge to a re-entry join, and a
typed `RESUME` edge to the original statement. Borrowed values cannot be
consumed after that boundary.

The declaration-to-callable projection derives `ASYNC` from the AST rather
than from a function name. Named function `TypeId` registration and member
callable refinement therefore preserve `ZR_CANONICAL_CALLABLE_EFFECT_ASYNC`;
the lambda effect pass uses the same derivation before lowering.

M1 records this topology but does not compile or execute direct await. Task
frame creation, resume dispatch, and payload materialization are M2 work.

## Legacy Boundary

Syntax 12 M6.2 removes the compatibility parser route. `%async`, `%await`,
and `%async T` report migration diagnostics; `TaskRunner` and the hidden
`__createTaskRunner` / `__awaitTask` helpers are not public descriptor facts.
Consumers derive the async effect only from explicit `async` AST state and the
canonical Task protocol. There is no legacy lowering fallback.
