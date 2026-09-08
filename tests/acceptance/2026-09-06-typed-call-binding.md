# Typed Call Binding Validation

Environment: Windows workspace `E:/Git/zr_vm`, WSL `Ubuntu-22.04`, GCC 11.4,
Debug Ninja cache `/home/hejiahui/zr-typed-binding-gcc`. The shared parent cache
`/home/hejiahui/zr-call-binding-gcc` was not built by this task.

## Focused Result

The following build and executable completed successfully after the generic
dispatch correction for typed native values:

```sh
cmake --build /home/hejiahui/zr-typed-binding-gcc -j 12 --target zr_vm_typed_call_binding_test
/home/hejiahui/zr-typed-binding-gcc/bin/zr_vm_typed_call_binding_test
```

Result: **12 tests, 0 failures**. Coverage includes typed function parameters,
native callback execution through the embedding API, a registered native member
passed directly from source, zero-argument calls, AOT thunk/metadata validation,
two distinct closure capture contexts, a captured callback, runtime signature
mismatch, stale caller generation, signature-row tampering, and source-to-binary
roundtrips with no persisted target pointer.

The native test exposed a real dispatch regression during development: inferred
closure types had been quickened to VM-only opcodes. Finalization now restores
generic call opcodes for typed bindings, and binary linking rejects incompatible
typed opcode rows.

## Shared-Path Checks

The final rebuild of all five targets succeeded. Executables then completed with
these results:

| Executable | Result |
| --- | --- |
| `zr_vm_typed_call_binding_test` | 12 passed |
| `zr_vm_call_binding_pipeline_test` | 14 passed |
| `zr_vm_call_binding_artifact_test` | 6 passed |
| `zr_vm_call_binding_aot_projection_test` | 5 passed |
| `zr_vm_aot_c_call_shared_library_smoke_test` | 4 passed, 1 failed |

The AOT shared-library typed-callback case passed. The failure was the static
numeric generated-C text assertion at
`tests/parser/test_aot_c_call_shared_library_smoke.c:458`, expecting the string
`zr_aot_scalar_stack_copy_f64 dstSlot=5 srcSlot=6`. This suite is not reported as
fully passing.

The successful combined build log is
`/home/hejiahui/zr-typed-binding-gcc/typed-final-build.log`.

## Limits

Native typed validation currently supports fixed-arity, nongeneric registered
functions using primitive type descriptors. Imported members with complete
descriptors are projected to their canonical source function type; descriptors
without structural type information remain broad and are rejected at a typed
boundary.

Full GCC/Clang, ASan/UBSan, and integrated AOT acceptance remain with the parent
task after the shared checkout settles.
