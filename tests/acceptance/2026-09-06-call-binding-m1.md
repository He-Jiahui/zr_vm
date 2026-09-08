---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/module_call_binding.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
plan_sources:
  - user: 2026-09-06 W2 / Call Binding M1 静态调用绑定与可重定位目标表
tests:
  - tests/cmake/zr_vm_call_binding_tests.cmake
  - tests/cmake/zr_vm_call_binding_artifact_tests.cmake
  - tests/library/test_call_binding_module.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_call_binding_aot_projection.c
  - tests/parser/test_typed_call_binding.c
doc_type: acceptance-record
---

# Call Binding M1 Acceptance Record

Validation used the WSL GCC build directory
`/home/hejiahui/zr-call-binding-gcc` with the repository checkout mounted at
`/mnt/e/Git/zr_vm`.

## Focused suites

The following CTest selection passed in both WSL GCC and WSL Clang builds:
`call_binding_runtime`,
`call_binding_native_registry`, `call_binding_pipeline`,
`call_binding_relocation`, `call_binding_artifact`,
`call_binding_aot_projection`, `typed_call_binding`, `call_binding_module`,
`metadata_runtime_method_binding`, and `aot_c_metadata_binding_loader`.
The result was 10/10 tests passed. The AOT metadata loader itself now contains
12 passing cases, including an LLVM provider with no internal call-binding
rows. The module fixture covers source imports,
static type methods, captured module state, a binary provider containing two
same-line `answer` definitions, and provider removal followed by generation
validation and rebind to a replacement module; the binary result is 20 (7 +
13), while the reload fixture changes the imported result from 43 to 47.

The linker also validates each callsite against its opcode/cache operation,
descriptor role, and imported owner `TYPE_DEF` before publishing the binding.
Corrupt operation, cache index, virtual/interface slot, and imported-owner
records are rejected as structured link errors rather than entering a name
lookup path. The generated AOT C fixture executes the static call and the
property setter/getter (`access(new Box())`) and returns 42; interface rows stay
deferred until receiver dispatch.

The Windows MSVC Debug CLI target was rebuilt after the same changes; running
the hello-world project printed `hello world` and exited successfully.

The existing regression executables also passed:

- member access fast paths: 108/108;
- known native call fast paths: all cases passed;
- property access lowering: 22/22;
- property consumer contracts: 11/11;
- GC core: 67/67;
- native direct binding: 5/5;
- VM closure precall: 6/6;
- AOT GC roots: 6/6;
- AOT typed direct-call compatibility: 12/12;
- AOT call contracts: 9/9;
- AOT typed call contracts: 4/4;
- known-call pipeline: 5/5;
- meta-call pipeline: 4/4.

The same-build GCC Debug Callgrind comparison is recorded in
`2026-09-06-call-binding-measurement.md`. Bound and cache-only outputs match.
The dynamic call-chain Ir is unchanged; native member and accessor Ir is lower
by 7.40% and 9.79% respectively. This diagnostic comparison does not establish
a release before/after performance claim or close the release three-percent
gate.

## Final Review Fixes

The AOT consumer review added actual generated C/LLVM accessor and meta-call
execution, including zero-argument calls and captured cross-module AOT
functions. LLVM needed method-info registration before its binding rows could
link. Generic C lowering retains the existing `CallStackValue` boundary, which
now prepares bound targets; known-call and source-contract regressions pass.
Accessor scratch windows restore the caller `functionTop` and discard temporary
values after direct calls. Native meta preparation accepts raw native callables
without dereferencing missing VM metadata. LLVM publishes runtime mappings even
when a provider has no internal binding rows.

Memcheck exposed a use-after-free in module removal: the registry's hot string
cache retained a hash node after `ZrCore_HashSet_Remove`. Module removal now
clears the existing hot-pair caches before deletion. The reload test checks an
immediate cache miss as well as the replacement result (43 to 47). The final
generated AOT loader Memcheck run reports zero errors and no leaked blocks.

## Full-build limitation

The all-target build reached the pre-existing `zr_vm_rust_binding_api_test`
link step and failed because `libzr_vm_rust_binding.so` did not provide
`ZrRustBinding_NativeCallContext_GetArgument` (four unresolved references).
This failure is outside the Call Binding targets; the focused targets above
were rebuilt and executed against the updated shared libraries. The broader
compiler-integration executable also contains unrelated baseline project and
matrix benchmark failures, so it is not used as evidence for Call Binding M1.

## Contract boundaries

Artifact rows contain tokens, signature/layout hashes, and relocation data only;
runtime VM/native/AOT addresses are selected after loading. Static module
failures report structured link errors and do not fall back to string lookup.
Opcode prefix expansion and portable SIMD/vector/matrix IR remain follow-up
milestones.
