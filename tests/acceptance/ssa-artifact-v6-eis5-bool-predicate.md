# SSA 08.01 EIS5 BOOL predicate extension

## Scope

This bounded leaf extends the existing EIS5 v5 counted scalar-CFG payload. It
uses the existing `typeToken` and 64-bit `bits` fields; the header, record
sizes, length formula, payload version, ZRAF v6 envelope, ERI1 v1 bundle, and
AOT ABI 17 are unchanged. EIS1-EIS4 routing, bytes, and rejection behavior are
preserved.

EIS5 accepts i64 and BOOL constants and values. BOOL constant bits must be
exactly zero or one, and a CONSTANT result must use the constant's type token.
CONDITIONAL_BRANCH accepts BOOL predicates and the existing i64 predicates for
backward compatibility. ADD operands/results and RETURN operands remain i64.
The single no-argument function and bounded CFG/side-table limits from the
[counted CFG leaf](ssa-artifact-v6-eis5-counted-cfg.md) still apply.

Older EIS5 v5 readers validate these typed fields as i64-only and therefore
reject a BOOL token as `INVALID_SECTION`. This extension intentionally keeps
the version and layout, so new BOOL artifacts have this forward-read
limitation until readers are upgraded.

## RED

The BOOL fixture passed `ZrCore_ExecIr_VerifyModule`. Before BOOL token support
was added to the EIS5 codec, both Clang and MSVC write tests failed at the
first expected missing behavior:

```text
FAIL: EIS5 BOOL-predicate dynamic size query
```

## GREEN verification

The current-source Clang 14 Debug/Ninja build used
`D:\tmp\zr_vm\ssa-artifact-v6-clang`:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  --target zr_vm_ssa_exec_ir_artifact_v6_test -j4
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  --target zr_vm_ssa_schema_relocation_test zr_vm_artifact_schema_test -j4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  -R '^(artifact_schema|ssa_schema_relocation|ssa_exec_ir_artifact_v6_write|ssa_exec_ir_artifact_v6_roundtrip)$' \
  --output-on-failure --no-tests=error
```

Both builds exited 0. CTest passed 4/4: `artifact_schema`,
`ssa_schema_relocation`, `ssa_exec_ir_artifact_v6_write`, and
`ssa_exec_ir_artifact_v6_roundtrip`.

Root independently built the current source with MSVC in
`D:\tmp\zr_vm\ssa-artifact-v6-msvc`; that build exited 0 and the same four
CTest gates passed 4/4. GCC was not run for this bounded leaf.

The write/read pair covers both conditions for the new BOOL-predicate graph
and the existing i64-predicate graph. Direct codec read and canonical artifact
open preserve Oracle results true=42 and false=7. The writer rejects BOOL bits
outside zero/one and a mismatched constant/result type. Direct reads reject
the rehashed mutations at payload offsets 220 and 260 respectively. The
canonical opener rejects them in `EXEC_IR_BUNDLE` at the absolute artifact
offset `payloadOffset + 220` or `payloadOffset + 260`.
The same tests retain the EIS1 byte-for-byte golden and EIS2-EIS4 regressions;
they also keep the EIS5 996-byte count-collision graph on the EIS5 magic route.

## Acceptance boundary

This accepts only BOOL predicates in the existing bounded EIS5 scalar-CFG
schema. It does not complete general CFG, other scalar types or opcodes, maps,
bindings, relocation resolution, ExecBC, package copy, AOT projection, native
callable ABI, or the full 08.01 milestone.
