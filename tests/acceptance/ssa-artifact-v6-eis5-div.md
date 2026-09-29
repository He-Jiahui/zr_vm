# SSA 08.01 EIS5 scalar DIV

## Scope

This bounded leaf adds one signed i64 `DIV` opcode shape to the EIS5 v5
counted CFG. It uses two i64 operands, one i64 result, and `RETURN`; the
44-byte header, v5 payload version, field order, and 84-byte instruction record
remain unchanged. The one-block, four-instruction fixture is 716 bytes, which
also matches the fixed EIS4 ADD length. EIS5 magic continues to select the
counted reader when lengths collide.

The fixture passes `ZrCore_ExecIr_VerifyModule` and the ExecIR Oracle before
serialization. It divides 84 by -7 and returns -12. The Oracle's DIV
implementation checks a zero divisor and `INT64_MIN / -1` before evaluating C
signed division; both cases are asserted to fail without reaching an undefined
division operation.

DIV has the schema `MAY_THROW` flag and is effect-observable to the verifier.
This slice accepts exactly one DIV carrying effect token IDs 1→2. Other
instructions still require zero flags and effect tokens; this does not add a
general effect chain or multiple effectful DIV instructions.

## RED and implementation

An initial fixture without the required effect tokens failed verification
with `EFFECT_TOKEN`. After adding the canonical 1→2 pair, VerifyModule and the
Oracle passed while the old writer failed the existing write CTest at the
first unsupported-opcode boundary:

```text
FAIL: EIS5 DIV dynamic size query shares the EIS4 716-byte tuple
```

The writer and reader now accept only the DIV shape, i64 operands/results,
`MAY_THROW`, and effect IDs 1→2. They report field-specific offsets for DIV:
flags at 526, effectIn at 588, and effectOut at 592. The direct reader and
canonical opener reject rehashed mutations for missing `MAY_THROW`, a
non-increasing effect pair, non-i64 input/result types, and the still
unsupported `ARITHMETIC` opcode. Rejected writes preserve the destination and
file bytes; failed reads leave the caller's module empty. The separate read
phase reopens the canonical artifact and verifies the returned -12 result.

Because DIV is now supported, the earlier SUB and MUL unsupported-opcode
mutations use `ARITHMETIC`, which remains outside the EIS5 allowlist. The tests
continue to cover the EIS1 golden, EIS2–EIS4 payloads, prior EIS5 scalar and
CFG cases, and the EIS4 length collision.

## Verification

The existing Clang build cache at `D:\tmp\zr_vm\ssa-artifact-v6-clang` was
reused. The current-source build and focused four CTests passed:

```powershell
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang `
  --target zr_vm_ssa_exec_ir_artifact_v6_test `
           zr_vm_ssa_schema_relocation_test `
           zr_vm_artifact_schema_test -- -j2
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang `
  --output-on-failure `
  -R '^(artifact_schema|ssa_schema_relocation|ssa_exec_ir_artifact_v6_write|ssa_exec_ir_artifact_v6_roundtrip)$'
```

Results: `artifact_schema`, `ssa_schema_relocation`,
`ssa_exec_ir_artifact_v6_write`, and `ssa_exec_ir_artifact_v6_roundtrip` all
passed (4/4). The Clang target build exited 0. Root independently built the
frozen C/test sources with MSVC (343/343 steps, exit 0); the same four CTests
passed (4/4).

## Compatibility and acceptance boundary

EIS1–EIS4 payload bytes and rejection paths are unchanged. EIS5 v5 readers that
predate DIV reject it at the instruction record because the payload version
and instruction width did not change. The fixed effect pair is a narrow
canonical DIV allowance; general effect-token serialization and exceptional
CFG handling remain outside this leaf. This does not complete the full 08.01
artifact or relocation plan; maps, bindings, relocation resolution, ExecBC,
package copying, and AOT projection remain open.
