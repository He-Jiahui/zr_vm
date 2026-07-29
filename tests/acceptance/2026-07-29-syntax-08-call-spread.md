---
related_code:
  - zr_vm_core/src/zr_vm_core/function_call_spread.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_spread.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
tests:
  - tests/parser/test_call_spread.c
  - tests/parser/test_semir_dynamic_call_deopt.c
  - tests/parser/test_aot_c_shared_library_smoke.c
doc_type: acceptance-record
---

# Syntax 08 Acceptance: Call Spread

## Scope

This record accepts spread arguments at dynamic call sites across parser/compiler binding, VM
execution, SemIR, AOT C, LLVM lowering boundaries, reflection construction, and rollback/GC
handling. Fixed-prefix arguments use the normal binder; the spread tail must be an array whose
elements match remaining value parameters without an implicit conversion.

The VM pins the spread source, roots expanded slots before copies can allocate, copies in an
alias-safe order, and destroys partial slots before restoring the original stack on failure.
Inline-struct arguments use the resolved callee frame layout. SemIR retains a distinct dynamic
spread instruction, and AOT liveness accounts for the prefix plus spread container slot.

## Independent Review And Validation

| Environment | Suite | Result |
|---|---|---:|
| WSL GCC 11.4 Debug | call spread | 16/16 |
| WSL GCC 11.4 Debug | SemIR dynamic call/deopt | 2/2 |
| WSL GCC 11.4 Debug | reflection surface | 18/18 |
| WSL GCC 11.4 Debug | reflection stress | 3/3 |
| WSL GCC 11.4 Debug | true AOT C compile/load/execute | 14/14 |
| WSL GCC 11.4 ASan | true AOT C compile/load/execute | 14/14, no sanitizer finding |

Independent code review found no remaining blocker after checking GC roots, rollback, inline
struct layout, overload binding, SemIR preservation, scalar-local liveness, and real AOT C
execution. Total fresh evidence is 53/53 with no ignored case.

The sanitizer run also caught and closed an AOT module-root lifetime defect: a loaded module is
now pinned while its runtime record is live, and every record-construction failure path releases
the pin. The smoke fixture roots its compile-time function graph until the generated module has
finished executing, so test-owned metadata cannot be collected through a stale reference.

Residual non-blocking coverage gaps are explicit fault injection for a mid-copy exception and
true LLVM execution of the spread case; LLVM lowering boundaries are covered, while AOT C is the
executed backend proof.
