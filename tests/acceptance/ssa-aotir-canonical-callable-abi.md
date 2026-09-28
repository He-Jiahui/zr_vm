# SSA AOTIR canonical callable ABI bridge

**Date:** 2026-09-28
**Scope:** narrow producer qualification for `NOARGS_I64` in the AOTIR projection.

## Contract

`ZrParser_ExecIr_LowerAotWithCanonicalCallable` is an opt-in bridge from a
verified ExecIR function to a canonical function type in one supplied semantic
context. It requires both `function.signatureHash` and
`function.contract.signatureHash` to equal that canonical node's
`structuralHash`, and requires the contract's declared effects to match the
canonical function effects. The accepted shape is currently limited to no
parameters, no receiver, no effects, canonical primitive `int64` return, a
zero-parameter frame, and typed ExecIR RETURN values that match that type.

The implementation reuses the existing ExecIR-to-AOT projection checks and
builds a private candidate. It publishes the ABI declaration only after all
checks succeed; every rejection keeps the caller's existing output intact.
The legacy AOT lowering APIs still publish `UNKNOWN`. A successful projection
remains `runnable == false` and does not claim native code generation.

The canonical type node's `structuralHash` currently mixes child local
`TZrTypeId` values. This bridge uses that hash only for same-context identity;
it makes no cross-context or persisted-hash stability claim.

## RED and GREEN

The test was prepared before the producer API. The current-source target build
then reached its final link and failed only because the test's new call had no
definition:

```text
undefined reference to `ZrParser_ExecIr_LowerAotWithCanonicalCallable'
```

After the public API and implementation were added, the focused target built
with WSL GCC 11.4.0 and the direct Unity fixture exited 0. The fixture covers
the valid canonical noargs-i64 case, confirms legacy lowering remains
`UNKNOWN`, and checks transactional rejections for missing/invalid canonical
types, non-function and mismatched callable hashes, signature/effect mismatch,
receiver/parameter/return mismatch, malformed ExecIR, and invalid constants.

During implementation, the first compile attempt exposed a missing direct
include for `ZR_SEMANTIC_ID_INVALID`; adding `semantic.h` to the implementation
resolved it. The first linked fixture run then isolated the null-constants,
count-one rejection: the reused lowerer returned false without assigning a
diagnostic. The facade now supplies `INVALID_PROJECTION` only when that failure
path leaves the diagnostic at `NONE`.

## Verification

Build output and executables were kept in the D-drive cache
`/mnt/d/tmp/zr_vm/close-proxy-core-red`; no build artifacts were moved across
drives. The focused executable was run directly:

```text
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_ssa_aot_canonical_abi_test -- -j4
wsl.exe --exec /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_aot_canonical_abi_test
```

Both commands completed successfully. The adjacent targets built successfully,
and each registered CTest passed individually:

```text
ssa_aot_canonical_abi                  1/1 passed
ssa_aot_callable_abi                   1/1 passed
ssa_aot_projection_branch_descriptor  1/1 passed
```

The exact CTest invocations were:

```text
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R ssa_aot_canonical_abi --output-on-failure --no-tests=error
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R ssa_aot_callable_abi --output-on-failure --no-tests=error
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R ssa_aot_projection_branch_descriptor --output-on-failure --no-tests=error
```

A first grouped-regex attempt matched no tests due to command-line quoting and
is not counted as verification.

## Limits

This slice does not add parameterized callable ABIs, receiver/effect lowering,
cross-context canonical hash comparison, native artifact emission, or
`runnable=true`. Those remain explicit unsupported paths.
