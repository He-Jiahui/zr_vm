# SSA 08.01 EIS5 dynamic counted scalar CFG

## Scope

This is a narrow EIS5 leaf: a variable-length little-endian `EXEC_IR` payload
inside the existing ZRAF v6 `EXEC_IR_BUNDLE` / ERI1 v1 envelope at AOT ABI 17.
The current fixture is one no-argument i64 function with three constants,
three values, five blocks, eight instructions, and explicit result, operand,
successor, and predecessor pools. Its 1260-byte size is fixture-specific;
the format length is calculated from encoded counts.

The codec accepts CONSTANT, ADD, BRANCH, CONDITIONAL_BRANCH, and RETURN in one
function. Limits are 256 blocks, 4096 combined constants/values/instructions,
16384 combined pool IDs, and 16 MiB per payload. EIS1–EIS4 payload bytes, ZRAF
v6, ERI1 v1, and AOT ABI 17 remain unchanged. This leaf does not complete
general ExecIR, maps, bindings, relocation resolution, ExecBC, native AOT
calling, package copy, or `ImportByPath` migration.

The writer keeps EIS1–E4 fixed-format priority and routes graphs matching a
fixed-format count tuple, per-block instruction ranges, and opcode sequence to
that fixed validator. Valid graphs with a different instruction layout remain
eligible for EIS5 even when counts collide. EIS5 uses bounded heap staging.
The reader checks counts and computed length before bounded allocation, builds
a temporary graph, verifies it, and publishes only on success. The canonical
opener accepts legacy exact lengths or a bounded EIS5 payload and delegates
magic, version, count, length, and graph validation to the scalar reader.

## Baseline and RED

Before EIS5 dispatch was integrated, the five-block fixture passed
`ZrCore_ExecIr_VerifyModule`; the registered write test failed at
`EIS5 five-block dynamic size query`, the first missing behavior.

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  --target zr_vm_ssa_exec_ir_artifact_v6_test -j4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^ssa_exec_ir_artifact_v6_write$' \
  --output-on-failure --no-tests=error
```

That current-source RED ran 1/1 and failed at the expected size query after
graph verification passed.

A second current-source RED used a verified three-block graph with the EIS3
count tuple but a different block-range/opcode layout. It failed at
`EIS5 graph with EIS3 counts gets dynamic size`; the intended EIS5 payload is
996 bytes, exactly the fixed EIS3 payload length.

## GREEN verification

The WSL GCC 11.4 Debug/Ninja build used only
`D:\tmp\zr_vm\ssa-artifact-v6-gcc`:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  --target zr_vm_artifact_schema_test \
           zr_vm_ssa_exec_ir_artifact_v6_test \
           zr_vm_ssa_schema_relocation_test --parallel 2
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^(artifact_schema|ssa_schema_relocation|ssa_exec_ir_artifact_v6_write|ssa_exec_ir_artifact_v6_roundtrip)$' \
  --output-on-failure --no-tests=error
```

The final build exited 0. CTest passed 4/4: `artifact_schema`,
`ssa_schema_relocation`, `ssa_exec_ir_artifact_v6_write`, and
`ssa_exec_ir_artifact_v6_roundtrip`. The write/roundtrip pair covers the EIS1
byte-for-byte golden and the EIS2–EIS5 codecs. The separate reader process
opened four EIS5 artifacts: five-block and same-count collision graphs for both
condition values. The collision payload is 996 bytes, matching the legacy EIS3
length, and the canonical opener dispatches it by its EIS5 magic. Both graph
shapes verify source IDs and return true=42 / false=7 through the Oracle.

Root independently built and tested the MSVC cache
`D:\tmp\zr_vm\ssa-artifact-v6-msvc`: the focused v6/schema CTests passed
3/3, and the current-source `artifact_schema` target passed 1/1.

## Negative coverage

The direct EIS5 reader rejects rehashed invalid successor IDs, nonreciprocal
edges, nonzero reserved fields, counts above the block, combined node, and
combined pool caps, declared lengths above 16 MiB, exact length mismatch,
truncation, and unsupported version 6. The canonical opener rejects rehashed
invalid CFG structure, reserved fields, over-cap counts and lengths, exact
length mismatch, and unsupported version 6. Failures report the bundle section
and payload-relative offset where stable. Direct failures leave the caller's
empty module unpublished. The fixed-format regression gates also
reject malformed EIS1–E4 inputs; in particular, an EIS4 graph with a changed
literal is not reinterpreted as EIS5.

## Acceptance boundary

This accepts only the bounded EIS5 counted scalar-CFG leaf. It does not mark
general CFG, binding, relocation, map, ExecBC, package-copy, AOT projection,
native callable ABI, or the full 08.01 milestone complete.
