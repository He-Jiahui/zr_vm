# Call Binding AOT Runtime Registration

## Scope

This bounded Call Binding M1 slice covers runtime registration of generated AOT
rows. The loader decodes the pointer-free ABI 16 row through the artifact
schema, rebuilds the interpreter binding, compares its contract and relocation
coordinates, and promotes only the matching process-local thunk. The original
VM callable object remains attached for interpreter execution and GC
tracing.

## Implementation

- `zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c`
- `zr_vm_library/src/zr_vm_library/aot_runtime.c`
- `tests/parser/test_call_binding_aot_projection.c`
- `tests/parser/test_aot_c_metadata_binding_loader.c`
- `docs/parser-and-semantics/call-binding-artifact-and-aot.md`

## Evidence

Validation used the separate WSL GCC build directory
`/home/hejiahui/zr-call-binding-gcc`, configured from the repository checkout.

```text
zr_vm_call_binding_aot_projection_test: 5 Tests 0 Failures 0 Ignored
zr_vm_aot_c_metadata_binding_loader_test: 12 Tests 0 Failures 0 Ignored
zr_vm_metadata_runtime_method_binding_test: 2 Tests 0 Failures 0 Ignored
```

The projection runtime case compiled a virtual member call, built its AOT
function table, encoded and registered 96-byte rows, linked them, checked AOT
thunk and method-info selection, and checked that the interpreter callable
object survived. It also reclassified a row as a MODULE relocation and checked
the invalid target sentinel, zero owner-depth/flags, and deferred `TARGET_NONE`
state. Flipping signature, module, or layout hashes, selecting a wrong target
index, or supplying an invalid row size caused structured rejection and
invalidated the link pass.

The generated loader cases compiled static, property-accessor, and interface
calls into shared AOT libraries, loaded them through
`ZrLibrary_AotRuntime_ExecuteEntry`, and inspected the attached metadata
runtime. Static and accessor rows resolved to registration thunks and method
info while preserving callable objects; interface MODULE rows retained
`TARGET_NONE` for receiver dispatch. The static and interface entries executed
and returned 42. The property fixture also executes the generated accessor
function (`access(new Box())`) in both C and LLVM, exercising the setter/getter
path and returning 42. Meta-call cases execute C with zero/one argument and
LLVM with one argument, and assert a binding cache hit. Separate C provider
fixtures execute imported functions, static methods, and captured module state,
checking an AOT target and a nonzero hit count on the consumer. An LLVM
provider with no internal call-binding rows also links and executes through its
published runtime method mapping.

The existing `zr_vm_aot_c_call_shared_library_smoke_test` was also checked.
Its static numeric local-only case retains a pre-existing source-contract
failure: the generated file contains the `u64` scalar stack copy, but not the
expected `f64` copy marker. Temporarily restoring the prior function-equivalence
heuristic produced the same failure, so it is outside the call-binding changes.
The failure remains tracked as baseline for the broader AOT smoke suite.

The final generated loader run under Valgrind Memcheck completed with zero
errors and no leaked blocks (742,869 allocations and frees).

## Open Coverage

The focused tests use local constant relocation and synthetic process-local
thunks alongside generated C/LLVM shared libraries. Bound cached accessor opcodes
are emitted through the AOT runtime cached helpers; legacy dynamic
`META_GET`/`META_SET` opcodes retain the explicit unsupported-meta-value
boundary. LLVM method-info rows advertise runtime mapping, without adding
reflective invocation support.
MODULE interface/virtual rows are validated as deferred records with
`TARGET_NONE`; receiver dispatch resolves them later. Cross-module provider
relocation is covered by the generated C provider fixtures. Native target
promotion is covered by the native registry suite.
