---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_object.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_object.c
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/parser/test_resource_unique_drop.c
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_resource_owner_borrow_receiver.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_closure_capture_runtime.c
  - tests/exceptions/test_exceptions.c
  - tests/parser/test_aot_c_ownership_contracts.c
  - tests/parser/test_aot_c_scope_contracts.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
doc_type: module-detail
---

# Resource / Unique / Drop

## Source contract

Syntax 04 M1 introduces the type-directed ownership surface:

```zr
resource class FileHandle {
    pub @constructor(path: string) { ... }
    pub @destructor() { ... }
}

var file: Unique<FileHandle> = own FileHandle("data.bin");
drop(file);
```

The declaration decides the lifetime world:

- `resource class` is an ownership-world type.
- `own T(...)` is valid only when `T` resolves to a resource class.
- A resource class cannot be constructed with ordinary `new` or value construction.
- `drop(owner)` consumes an owner through the canonical release builtin.
- `Unique<T>` is non-nullable; an optional owner must use an explicit option surface.

`resource`, `own`, and `drop` are contextual identifiers in M1. `resource` is claimed only before
`class`, and `own` only before a type-shaped identifier target, so ordinary `resource(value)` and
`own(value)` calls remain valid. Parser AST preserves the resource declaration modifier and marks
the new surface on `SZrConstructExpression`; later stages do not infer the world from spelling or
from a runtime allocation mode.

## Canonical ownership facts

The existing ownership dataflow remains the source of truth for moves. Assignment, parameter
passing, and return consume `Unique<T>` and publish a canonical move fact. A later read, borrow,
or drop of the moved source publishes an ownership violation. The compiler bridge rejects only
an ownership fact with `kind=ERROR`, `qualifier=UNIQUE`, and `isViolation=true`; it does not branch
on a diagnostic message, variable name, or source text.

Assignment compatibility also rejects `null` for non-nullable `Unique`, `Shared`, and `Weak`
qualifiers before the historical reference-like null conversion is considered. This preserves
ordinary GC reference compatibility while enforcing the owner invariant structurally.

## Construction and cleanup lowering

Resource construction is destination-first:

1. The object seed is created in `CONSTRUCTING` state.
2. `MARK_TO_BE_CLOSED` arms the seed before the constructor call.
3. Field stores update the normal managed-field presence state.
4. Successful construction moves the direct owner to the expression result with `OWN_UNIQUE`.
5. The now-empty construction marker is closed.

If construction throws, unwind sees the armed seed. The resource custom destructor is skipped,
only initialized managed fields are dropped, and fields are visited in reverse declaration order.
For a fully constructed resource, normal scope exit, early return, throw, break, continue, or
explicit `drop` all converge on `OWN_RELEASE` exactly once.

Resource custom Drop bodies must be non-throwing. The compiler uses CFG exception edges to reject
a resource destructor that may enter a catch/throw path; ordinary GC class destructor behavior is
unchanged.

## Runtime representation

A direct resource `Unique` stores the resource object pointer and `ownershipKind=UNIQUE`; its
`ownershipControl` is null. Move transfers that pointer and resets the source. No strong/weak
control block or refcount is created on the M1 direct Unique path.

`ownership_resource.c` owns the direct-resource lifecycle state machine:

- `CONSTRUCTING`: partial cleanup only.
- `ALIVE`: custom Drop then managed fields.
- `DROPPING`: reentrant Drop is ignored.
- `DROPPED`: repeated release is idempotent.

Managed owner fields drop from derived prototype to base prototype, and within each prototype in
reverse declaration order. A null/uninitialized field is skipped. Runtime marker mirroring keeps
the canonical base slot synchronized when frame-layout physical slots are moved or released, so
unwind cannot release a stale owner copy.

## VM and AOT agreement

Resource syntax lowers through the existing stable ownership instruction family:

- `OWN_UNIQUE`
- `OWN_RELEASE`
- `MARK_TO_BE_CLOSED`
- `CLOSE_SCOPE`

Semantic IR retains the ownership transitions. AOT C and LLVM emit the corresponding ownership
and scope helper calls rather than an unsupported fallback. The focused pipeline executes the VM
fixture and inspects the generated C/LLVM sources; the observed explicit/scope Drop log is `21`
on all three supported toolchains.

## M1 And M3 Boundary

M1 establishes resource construction, direct Unique move, deterministic Drop, field cleanup,
partial construction, and exception cleanup. Syntax 04 M2 now supplies the Shared/Weak
control-block contract, and M3 supplies compile-time owner reborrow and direct receiver checking;
see `resource-shared-weak.md` and `resource-owner-borrow-receiver.md`.

The remaining boundary in this section is the M4 GC/domain bridge; the direct-Unique
representation described above remains unchanged. M3 does not add a runtime borrow table:
owner move/drop/share conflicts are rejected from canonical Place and LoanId facts.

The current direct resource object is temporarily kept outside ordinary GC collection through the
existing ignore registry while owned, then returned to GC after Drop. This is not the final bridge
model. Syntax 04 M4 must replace it with explicit domain/bridge identity and satisfy the plan's
"no hidden ignore registry" promotion gate. M1 claims only the narrower gate: the ordinary direct
Unique path has no hidden refcount/control block, and VM/AOT Drop order agrees.
