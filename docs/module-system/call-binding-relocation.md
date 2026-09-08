---
related_code:
  - zr_vm_core/include/zr_vm_core/module_call_binding.h
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
plan_sources:
  - user: 2026-09-06 W2 / Call Binding M1 静态调用绑定与可重定位目标表
tests:
  - tests/library/test_call_binding_module.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_call_binding_aot_projection.c
  - tests/acceptance/2026-09-06-call-binding-m1.md
doc_type: module-detail
---

# Module Call Binding Relocation

An imported static call is compiled as a `VM_MODULE` relocation. Type
inference publishes the provider member token, signature token/hash, module
signature hash, and (for a type member) the owner layout contract. The compiler
keeps the existing member and callsite cache representation; the executable
instruction stream does not gain a name-based lookup sequence.

At module load, the signature verifier checks the provider hash before the
relocator scans its token records. A top-level exported function is resolved
through the installed export closure so module captures remain attached. A
constant-backed type or static method is resolved by its exact constant index.
The candidate must match the recorded signature and owner layout before the
runtime witness is installed. The witness is retained as a GC edge and its
generation is checked on each prepared call.

The provider compiler publishes pointer-free `CALLABLE_CONSTANT`, owner, and
child-alias records. The child alias is used when a function constant points at
an inline child; it prevents same-name/same-line functions from being merged by
binary reconstruction. The loader rejects duplicate tokens, wrong signatures,
wrong module hashes, and missing provider definitions. A stale target from a
removed provider is invalidated and rebound against the newly loaded provider;
direct call validation still reports stale generation when no replacement
module is being linked. It does not reconstruct a target by comparing member
names after a contract has failed.

Source and binary providers use the same contract. Binary `.zro` persistence
stores only tokens, hashes, layout data, and relocation coordinates; closure,
callback, and AOT addresses exist only after loading. `.zrm` packaging reuses
the same section through the canonical artifact projection.

## Validation

The module suite exercises source functions, static type methods, captured
module state, binary providers with two same-line `answer` definitions, and
reload invalidation followed by rebind. Artifact and AOT suites verify fixed-width rows and
pointer-free projection. Native registry tests cover the parallel provider
relocation path.
