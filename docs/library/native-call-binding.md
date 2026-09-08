---
doc_type: module
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding_call_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
plan_sources:
  - docs/parser-and-semantics/call-binding-artifact-and-aot.md
tests:
  - tests/library/test_call_binding_native_registry.c
---

# Native call binding

Native providers publish stable member and signature tokens through the
descriptor registry. The compiler stores the provider module hash and those
tokens in a module relocation. At link time the registry resolves the closure
by module hash and token; process addresses are never written to artifacts.

The provider hash includes callable signatures, passing modes, generic
constraints, type fields, methods, meta methods, enum declarations, and module
contract fields. Closure identity remains runtime-only and is protected by the
normal GC barrier when copied into a call-site cache.
