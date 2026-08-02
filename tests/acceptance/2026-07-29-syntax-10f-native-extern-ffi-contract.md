---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_parser/include/zr_vm_parser/ffi_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_callable_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_contract.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_callable_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_contract.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
plan_sources:
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/ffi/test_native_extern_contract.c
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/parser/test_aot_c_shared_library_smoke.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
doc_type: acceptance-record
---

# Syntax 10F Acceptance: Native Extern And Canonical FFI Contract

## Scope

This record accepts the Syntax 10F M3 native contract gate. The accepted source surface is
`native extern`. `%extern` is not a compatibility spelling: production parser recognition is
diagnostic-only, returns `legacy_syntax_removed`, and cannot create the old AST/lowering.

Each static function owns a persistent `SZrNativeImportContract` containing stable symbol and
module identities, library/entry identity, availability and capabilities, source mapping,
target ABI, policy fields, a canonical FFI signature, and an independent canonical language
callable contract. The two hashes cover different facts. The signature covers scalar,
pointer, enum, aggregate/union field layout, callback, parameter direction, marshalling,
ownership, and nullability. Common validation and hashes are shared by compiler, `.zro`, VM,
AOT, loader, and libffi.

## Contract Boundaries

- Static calls resolve by contract index through `getContractSymbol`; they do not reconstruct
  or parse a runtime signature object.
- `.zro` serializes explicit fields and rejects truncation, excessive counts, invalid enums,
  hash drift, and target-contract corruption.
- AOT ABI 14 publishes an immutable native-import table through both module and code
  registration descriptors; runtime admission requires pointer/count agreement.
- libffi lowers the canonical vector only after availability, capability, target pointer size,
  endianness, ABI hash, aggregate range, and signature hash validation.
- Callback types require explicit lifetime, thread, and exception policy. Ref-like,
  owner/resource, GC-reference, unsupported ABI, and other non-blittable implicit shapes fail
  closed by capability/layout facts, not by a concrete type-name blacklist.
- Union values are admitted only as by-value inputs. Marshalling requires exactly one
  non-default field to identify the active member; zero or multiple candidates fail closed.
  Union returns and union `ref/out` parameters are rejected by both compiler construction and
  persisted-contract admission until the object model carries stable active-member metadata.
- Call-lifetime callbacks are activated only after every allocation succeeds. Nested use of
  the same callback handle restores an explicit policy/error snapshot in reverse order; the
  outermost cleanup retains the `call` policy and marks the handle inactive.

## Fresh Validation

| Environment | Target | Result |
|---|---|---:|
| WSL GCC 11.4 Debug | `zr_vm_native_extern_contract_test` | 27/27 |
| WSL GCC 11.4 Debug | `zr_vm_ffi_test` | 29/29 |
| WSL GCC 11.4 Debug | `zr_vm_ffi_native_call_pin_contract_test` | 2/2 |
| WSL GCC 11.4 Debug | `zr_vm_aot_c_shared_library_smoke_test` | 14/14 |
| WSL GCC 11.4 Debug | `zr_vm_aot_c_call_shared_library_smoke_test` | 5/5 |
| WSL GCC 11.4 Debug | `zr_vm_aot_c_frame_setup_contracts_test` | 1/1 |
| Windows MSVC 19.44 Debug | `zr_vm_native_extern_contract_test` | 27 tests, 0 failures, 1 Unix-only LLVM case ignored |
| Windows MSVC 19.44 Debug | `zr_vm_ffi_test` | 29/29 |
| Windows MSVC 19.44 ASan | `zr_vm_native_extern_contract_test` | 27 tests, 0 failures, 2 ignored, no sanitizer finding |
| Windows MSVC 19.44 Debug | `zr_vm_ffi_native_call_pin_contract_test` | 2/2 |
| Windows MSVC 19.44 Debug | `zr_vm_aot_c_shared_library_smoke_test` | 14 platform-specific cases ignored |
| Windows MSVC 19.44 Debug | `zr_vm_aot_c_call_shared_library_smoke_test` | 5 platform-specific cases ignored |
| Windows MSVC 19.44 Debug | `zr_vm_aot_c_frame_setup_contracts_test` | 1/1 |

The native-contract AOT case emits six canonical vectors (scalar, aggregate, union,
`ref/out`, callback, and throw/cleanup), compiles the generated C into a shared library,
loads it, checks exact metadata/hashes, and passes every published vector through the same
libffi validator. The scalar source case also resolves and executes the fixture symbol through
the static contract path and returns 42.

## Review Corrections

The first aggregate representation embedded a full field array in every type node, making a
single native import contract 87,864 bytes. Review replaced it with one signature-level field
pool and type ranges, reducing the contract to 6,728 bytes while preserving validation and
hash coverage. Runtime contract lowering was separated from the descriptor parser so both
implementation files remain below the project's large-file threshold. `.zro` short reads and
oversized count paths now fail before iteration or allocation.

MSVC validation also exposed an exception-reset defect: `ZrCore_State_ResetThread` retained
the failed frame as the active GC stack and only rewrote the base slot's type tag. A missing
symbol could therefore leave an unwritten temporary with stale GC flags, causing an access
violation during global teardown. The reset now fully normalizes the base slot and collapses
both success and error paths to the base frame boundary. The SymbolError test asserts this
invariant directly, and the original failure path passes under MSVC ASan.

Independent review then exposed two additional contract defects. Union object marshalling
previously wrote every alias field at offset zero, so declaration order selected the native
bytes. Input marshalling now chooses one explicit non-default member and rejects ambiguous or
empty representations; return and write-back directions are rejected because decoding aliases
cannot preserve that identity. Callback activation previously used one Boolean and could let an
inner call close an outer call. Each marshalled callback argument now owns a scope frame, code
pointer storage is embedded rather than allocated after activation, and cleanup restores nested
frames transactionally. Regression fixtures make the two callback calls sequential in C rather
than relying on operand evaluation order.

The final review also found that record-array growth failure released the library, function
table, frame table, and GC pins but leaked four owned path strings. The append-failure cleanup now
matches the other record-construction exits. On Windows ASan, the fixture and runtime use separate
static CRT instances, so `errno` is not shared across the DLL boundary; that one ABI-inapplicable
case is explicitly ignored there. WSL and dynamic-CRT MSVC Debug retain direct errno-policy
coverage, while Windows static-CRT callers use the `lastError` policy.

An isolated-index rebuild then exposed a missing LLVM lowering boundary in the commit set. Native
extern code generation could emit `RESET_STACK_NULL`, `RESET_STACK_NULL2`, and
`PROPERTY_REF_CREATE_LOCAL`, but the LLVM dispatcher rejected those instructions as unsupported.
The dedicated runtime calls are now emitted, and the LLVM code-registration case moves from the
reproduced 26/27 failure back to 27/27.

## 2026-08-03 schema v4 revalidation

Final review reopened the earlier M3 implementation even though the behavioral suite was green.
The persisted native-import row stored `callableContractHash` as a copy of `signatureHash`, so it
did not retain language passing, escape, initialization, temporary, call-site, receiver, or effect
facts. The builder also rejected concrete names such as `Span`, `Task`, and `Unique`, and aggregate
layout hashing was not anchored to canonical `SZrTypeLayout`.

The repair bumps the native FFI schema to v4 and replaces the copied scalar with a structured
`SZrFfiCallableContract`. `ZrParser_FfiContract_Build` now requires the compiler semantic context,
normalizes every parameter through the canonical syntax contract, interns the callable, and
persists an independently hashed vector. Common admission validates the vector and its
`in/ref/out` cross-contract mapping. Aggregate and union hashes are rebuilt from validated
canonical `SZrTypeLayout`; admission is capability/layout driven, so a local blittable struct named
`Span` succeeds while ownership and unsupported generic shapes fail closed.

The complete callable vector round-trips through `.zro` and is emitted by both C and LLVM AOT.
Focused revalidation is identical under WSL GCC 11.4 and Clang 14: native extern 29/29, AOT C
stripping 37/37, and strict percent cutover 6/6. MSVC 19.44 reports the same 29/29, 37/37, and 6/6;
the one LLVM runtime-loading case in the native suite is explicitly Unix-only and therefore ignored
on Windows. The migration inventory remains on its existing 649-item review baseline after the new
fixture functions were named to avoid false constructor-call classification.

## Outcome

Syntax 10F is accepted for its M3 native extern/FFI ABI scope. The strict 06B production parser
cutover is also complete, but this does not complete 10C provider convergence or the root Syntax
redesign.
