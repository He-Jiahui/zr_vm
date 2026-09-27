# SSA 08.01 ERI1 reader and raw relocation boundary

## Scope

This independent slice hardens the existing ERI1 envelope reader and its raw
relocation validation API. It leaves ZRAF schema 6, AOT ABI 17, the ERI1
on-disk layout, and the EIS1 scalar graph layout unchanged. It does not add
binding, maps, ExecBC execution, or process-local target publication to
`ZrCore_Module_OpenExecIrArtifact`. Full 08.01 remains open.

## Baseline and RED

The new fixture uses the already registered `ssa_schema_relocation` CTest and
checks an `EXEC_IR` section of four bytes against relocation `codeOffset=5`.
It also checks that a later malformed row invokes no resolver, failed
resolution retains token 7, malformed directory count/width is rejected,
and every reader failure leaves the output view empty. Test calls are inside
an active `CHECK`/`EXPECT` macro instead of `assert`, so defining `NDEBUG`
does not remove them.

On WSL GCC 11.4.0, the unchanged production implementation rebuilt the
target 2/2 in `D:\tmp\zr_vm\ssa-artifact-v6-gcc`. This command exited 1:

```bash
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^ssa_schema_relocation$' --output-on-failure --no-tests=error
```

CTest ran 1/1 and reported 15 failed assertions. The invalid offset was
accepted, resolver callbacks ran and changed the caller's result array, the
failed resolver's diagnostic token was zero, nonzero count with zero width
was accepted, and late directory/hash or null-input failures left stale view
fields. These failures establish the pre-fix behavior and the fixture's RED.

## Test inventory

- Raw writer/reader success with ABI 17 and valid offset 1 still maps token 7
  to a test index 99.
- Offset 5 outside a four-byte `EXEC_IR` section is rejected before resolver
  invocation; the caller's resolved indices remain unchanged.
- A two-row relocation section with the second row malformed causes zero
  resolver calls, retains the failing row index/token, and publishes neither
  result.
- A valid row whose resolver rejects keeps the result array and records
  token 7.
- Directory element count 1 with element width 0 is rejected; overlap,
  hash mismatch, truncated length, and null buffer leave a zero view.
- Existing ExecBC verifier controls remain active in Debug and Release
  builds.
- The `ssa_exec_ir_artifact_v6_write` and
  `ssa_exec_ir_artifact_v6_roundtrip` CTests remain regression gates because
  the ZRAF opener uses this reader; their fixture is a real file in the D
  build tree consumed by a separate process.

## Tooling evidence

RED and GREEN used the repository's WSL GCC 11.4.0 Debug/Ninja cache. After
the production fix, this focused build completed 288/288 with exit 0:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  --target zr_vm_ssa_schema_relocation_test \
           zr_vm_ssa_exec_ir_artifact_v6_test -j 4
```

The following focused CTest then exited 0 with 3/3 passing:

```bash
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^(ssa_schema_relocation|ssa_exec_ir_artifact_v6_(write|roundtrip))$' \
  --output-on-failure --no-tests=error
```

After removing two extra assertions added after the recorded RED, the final
test source was rebuilt alone in the same GCC cache (2/2, exit 0) and this
three-test CTest was rerun (3/3, exit 0). The resulting GCC binary therefore
matches the final fixture source.

Clang 14 Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-clang` completed the
same focused target build 273/273 with exit 0. The same three focused CTest
names passed 3/3 with exit 0. Both builds produced warnings in parser/library
files outside this slice; no warning came from the touched ERI1
source or fixture.

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  --target zr_vm_ssa_schema_relocation_test \
           zr_vm_ssa_exec_ir_artifact_v6_test -j 4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  -R '^(ssa_schema_relocation|ssa_exec_ir_artifact_v6_(write|roundtrip))$' \
  --output-on-failure --no-tests=error
```

Native MSVC Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-msvc` completed
the focused target build 292/292 with exit 0. The same three focused CTests
passed 3/3 with exit 0 under the native Windows runner:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_ssa_schema_relocation_test zr_vm_ssa_exec_ir_artifact_v6_test -j 4
& "D:\Tools\development\cmake\bin\ctest.exe" --test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc -R '^(ssa_schema_relocation|ssa_exec_ir_artifact_v6_(write|roundtrip))$' --output-on-failure --no-tests=error
```

MSVC reported warning C4310 for the pre-existing `SIZE_MAX` cast in the raw
relocation allocation guard, alongside the workspace's `/W3` to `/W4`
override and unrelated parser/library warnings. The warning did not affect
the focused tests.

## Results

The raw reader and relocation fixture is GREEN on GCC, Clang, and MSVC. The
real ZRAF v6 writer/reader pair remains GREEN across separate processes on
all three toolchains. The new failure checks run and preserve the caller's
resolved array, retain token 7, and leave failed reader output empty.

## Acceptance decision

GCC, Clang, and native MSVC focused acceptance are complete. This raw
resolver maps only a test token to an index; it is not executable VM/native/AOT
relocation. Full 08.01 remains open.
