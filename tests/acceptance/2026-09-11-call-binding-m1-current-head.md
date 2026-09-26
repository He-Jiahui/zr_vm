---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
implementation_files:
  - tests/parser/test_call_binding_pipeline.c
  - tests/library/test_zrm_container.c
plan_sources:
  - user: 2026-09-11 W2 / Call Binding M1 static binding and relocatable target table
tests:
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_call_binding_aot_projection.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/library/test_call_binding_relocation.c
  - tests/library/test_zrm_container.c
  - tests/core/test_call_binding_runtime.c
doc_type: acceptance-record
---

# Call Binding M1 current-head verification

The production contract is present in `b54a6f27` (included by current `HEAD`):
compiler facts publish token/signature/layout/relocation records, call-site
cache entries carry the same contract plus a tagged process-local witness, and
the linker rejects stale, ambiguous, signature, owner, and layout mismatches.
The writer and canonical artifact schema serialize only the persistent fields;
VM/native/AOT addresses remain runtime-only. The link path has no name-lookup
fallback for a statically bound site. Virtual/interface rows retain their slot
contract and resolve against the receiver at dispatch time; accessors and
meta-functions use the same operation field.

This audit adds two regression checks without changing production behavior:

* `test_call_binding_pipeline.c` now drives the compiler's normal build-fact
  and reference/effect prechecks before compiling deliberately ambiguous and
  signature-incompatible member calls. It requires the precise diagnostics
  `Ambiguous overload for member 'pick'` and `Expected 'int' but found 'float'`.
* `test_zrm_container.c` encodes a validated 96-byte call-binding row, keeps a
  sentinel VM function pointer only in a runtime witness, verifies that the
  pointer bytes are absent from the encoded payload, packs the payload as a
  `.zro` module into `.zrm`, reads it back, and decodes the row byte-for-byte.

## Focused evidence

The WSL GCC and Clang Debug builds were run against the current checkout.

* `call_binding|typed_call_binding|metadata_runtime_method_binding`: 9/9
  CTest tests passed in each compiler build.
* `aot_c_metadata_binding_loader`: 1/1 passed in each compiler build. The
  generated loader suite covers static, accessor, interface, native/provider,
  C/LLVM, and runtime registration paths.
* Direct `zr_vm_call_binding_pipeline_test`: 16 tests, 0 failures in both GCC
  and Clang; the new ambiguity and signature cases pass.
* Direct `zr_vm_zrm_container_test`: 9 tests, 0 failures in both GCC and
  Clang; the new pointer-free `.zrm` roundtrip passes.
* Windows MSVC Debug `zr_vm_zrm_container_test`: 9 tests, 0 failures. The
  MSVC pipeline reaches and passes the 15 binding/parser cases (including both
  new diagnostics), then the existing full-collection graph case terminates
  with `0xC0000005`. This reproduces the dirty-worktree GC failure seen by
  unrelated regression binaries; it is not treated as a green MSVC pipeline
  result. The MSVC CLI hello-world smoke test still exits 0 and prints
  `hello world`.

The current dirty checkout also makes the standalone Callgrind measurement
crash before instrumentation (`Ir=0`) in the same runtime area. The previous
same-build measurement remains the only performance evidence: bound versus
cache-only was equal for the dynamic control chain and reduced Ir by 7.40% for
the native-member case and 9.79% for accessors. It explicitly does not claim a
historical before/after gain or close the release 3% gate. No new performance
claim is made here.

The canonical artifact roundtrip and AOT projection tests additionally verify
token/signature/layout/relocation equality, and the binary relocation suite
checks that loaded VM targets are rebound rather than serialized as addresses.
Opcode-prefix/variable-length instruction work and portable SIMD IR remain
follow-up milestones.
