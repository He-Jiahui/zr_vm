# SSA 08.01 first persistent canonical ExecIR slice

## Scope

ZRAF v6 ZRO section 22 carries a nested ERI1 envelope at AOT ABI 17. Its
EIS1 payload encodes one no argument i64 function with CONSTANT and RETURN.
The dedicated opener decodes a temporary `SZrExecIrModule`, verifies it, and
publishes it only after matching the expected public identity and outer
metadata contract. The parser writes a real file using a temporary file in
the destination directory before same directory publication. Legacy `01ZR`
ImportByPath remains separate.
The zero argument count comes from the explicit contract row, while i64 is
checked in the graph. Signature tokens and hashes are only identities here;
the test does not claim a native callable ABI.

This is the first 08.01 vertical slice, not completion of 08.01. General
maps, binding, relocation resolution, ExecBC, package copy, AOT projection,
and ImportByPath migration remain unimplemented for canonical ExecIR.

## Baseline and RED

Before implementation, WSL GCC syntax checking the new fixture failed on
the absent `ZrCore_Module_OpenExecIrArtifact`,
`ZrParser_ExecIr_WriteCanonicalZroFile`, and
`ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE`. The old nested ERI1 raw codec accepted
arbitrary `EXEC_IR` bytes and supplied no canonical graph to Oracle.

## Test inventory

- Separate CTest `--write` and `--read` processes create and consume a real
  ZRAF file in the D drive build tree. The reader checks graph fields and
  Oracle return 42.
- EIS1 direct codec test checks exact graph roundtrip, source IDs, Oracle
  return, unsupported binding rejection, unchanged output buffer, and failed
  decode without publication.
- File writer rejects unsupported binding, source map, and relocation
  section. It preserves an existing artifact after an injected write failure
  and after invalid input.
- Loader rejects legacy `01ZR`, schema 5, truncated and trailing bytes,
  missing bundle, an outer contract member mismatch after graph decode,
  ABI 16, a nonzero ERI1 reserved field, module hash mismatch, unsupported
  nested sections, and a recomputed hash over an invalid opcode. Every
  failure leaves the output graph empty.
- Existing raw ERI1 relocation and artifact schema tests protect their old
  APIs. The raw relocation test's ABI field follows the global ABI constant;
  the generic codec itself does not enforce executable ABI. The separate
  capability-validation fixture uses a controlled host/manifest ABI 16 value
  and tests equality, not current artifact executability.

## Tooling evidence

The fixture was checked first with:

```bash
cd /mnt/e/Git/zr_vm
gcc -std=c11 -Wall -Wextra -Werror=implicit-function-declaration -fsyntax-only \
  -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include \
  tests/library/test_ssa_exec_ir_artifact_v6.c
```

Observed RED: three missing interface/section diagnostics and exit 1.

## Results

GCC Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-gcc`: target build exited
0. The focused CTest command below passed 5/5, including the real file
write/read pair in separate processes, on 2026-09-27:

```bash
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^(ssa_exec_ir_artifact_v6_(write|roundtrip)|ssa_contract_freeze|ssa_schema_relocation|artifact_schema)$' \
  --output-on-failure --no-tests=error
```

All three CMake caches used `-G Ninja -DCMAKE_BUILD_TYPE=Debug
-DBUILD_TESTS=ON -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF
-DBUILD_LANGUAGE_SERVER=OFF -DBUILD_RUST_BINDING=OFF -DBUILD_CLI=OFF`.
WSL GCC and Clang used `-DCMAKE_C_COMPILER=gcc` or `clang` and their own
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-{gcc,clang}` build directories. The
focused build targets were
`zr_vm_ssa_exec_ir_artifact_v6_test zr_vm_artifact_schema_test
zr_vm_ssa_contract_freeze_test zr_vm_ssa_schema_relocation_test`.
For MSVC, the native `ctest.exe` at
`D:\Tools\development\cmake\bin\ctest.exe` ran the same regex with
`--test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc`; MSVC tools were imported
through `using-vsdevcmd`'s `Invoke-VsDevCommand.ps1` wrapper.

Clang 14 Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-clang`: target build
exited 0 and the same five focused CTest names passed 5/5 on 2026-09-27.
MSVC 19.44 Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-msvc` built the
same targets and passed the five focused CTest names 5/5 using native
Windows `ctest.exe`, including the exclusive temporary file writer and its
injected write failure check.

Adjacent version-migration checks in the warm Clang cache: the registered
`ssa_capability_validation` CTest passed 1/1; direct `zrm_container` Unity
passed 9/9; direct `artifact_schema` Unity passed 25/25, including the real
source compile/binary signature import and artifact writer roundtrips from
`test_artifact_schema_source_roundtrip.c`. The direct
`aot_c_frame_setup_contracts` Unity test failed 1/1 at line 912 because it
searches for `frame.codeRegistration` after `if (includeExportContext)`;
the emitter assigns it before that `if`. Both exact lines have the same
ordering in HEAD, before this slice's ABI text update, so this pre-existing
source-contract test mismatch is separate from the artifact validation.

The generated `.zro` and all three CMake caches are under the named
`D:\tmp\zr_vm\ssa-artifact-v6-*` directories. Clang and MSVC caches were
handed to the optional support agent for subsequent focused reuse; they
must not be deleted while that work is active. No build output was placed
in the shared source checkout by this slice.

## Acceptance decision

GCC, Clang, and native MSVC focused artifact proofs are green. The adjacent
AOT source-contract mismatch remains open outside this slice.
Full 08.01 remains open after this slice.

## EIS2 two-block follow-up

This follow-up keeps ZRAF v6, AOT ABI 17, and ERI1 v1. It adds a distinct
EIS2 v2 payload for one no argument i64 function with an unconditional entry
BRANCH to a CONSTANT then RETURN block. The test fixes the committed EIS1
payload as an exact 412 byte golden and writes the EIS2 artifact to a sibling
`-branch.zro` path under the same D build tree. The separate `--read` process
must reconstruct the actual two-block graph, verify its edge and run Oracle
to return 42. A malformed successor with its nested hash recomputed must
report the exact payload byte offset and leave the output graph empty; a
wrong EIS2 version must return `UNSUPPORTED_VERSION` with no graph.
The same file mutation without a hash update must be rejected by ERI1 hash
validation before graph decoding.
Direct codec checks also require exact EIS1 and EIS2 sizes, a byte-for-byte
EIS1 v1 golden, and unchanged size/output destinations on rejected EIS2
graphs.

Test-first GCC RED was captured in the retained
`D:\tmp\zr_vm\ssa-artifact-v6-gcc` cache: the focused test target built
2/2, then `ssa_exec_ir_artifact_v6_write` failed 1/1 with
`branch writer status: invalid-section (10)` and
`FAIL: write verified two-block branch artifact`. The old EIS1 golden and
source graph verifier passed before this assertion. This is the expected
failure of the existing one-block writer.

## EIS2 tooling and results

GCC 11.4 Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-gcc` rebuilt the
focused artifact, schema, and relocation targets (695/695 Ninja edges, exit
0). The final EIS2 test source and file writer were compiled in that build.
The following registered CTests passed 4/4, exit 0:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  --target zr_vm_ssa_exec_ir_artifact_v6_test \
           zr_vm_ssa_schema_relocation_test zr_vm_artifact_schema_test -j 4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^(ssa_exec_ir_artifact_v6_(write|roundtrip)|ssa_schema_relocation|artifact_schema)$' \
  --output-on-failure --no-tests=error
```

Clang 14 Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-clang` rebuilt the
artifact test target (623/623, exit 0), including the final test source and
file writer, then passed the registered write/roundtrip CTests 2/2:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  --target zr_vm_ssa_exec_ir_artifact_v6_test -j 4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  -R '^ssa_exec_ir_artifact_v6_(write|roundtrip)$' \
  --output-on-failure --no-tests=error
```

MSVC 19.44 Debug/Ninja in `D:\tmp\zr_vm\ssa-artifact-v6-msvc` rebuilt the
artifact test target, including its final test and writer objects, and linked
the executable with exit 0. Native Windows `ctest.exe` passed the registered
write/roundtrip CTests 2/2, exit 0:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  'C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1' `
  'D:\Tools\development\cmake\bin\cmake.exe' --build `
  'D:\tmp\zr_vm\ssa-artifact-v6-msvc' `
  --target zr_vm_ssa_exec_ir_artifact_v6_test --parallel 4
& 'D:\Tools\development\cmake\bin\ctest.exe' `
  --test-dir 'D:\tmp\zr_vm\ssa-artifact-v6-msvc' `
  -R '^ssa_exec_ir_artifact_v6_(write|roundtrip)$' `
  --output-on-failure --no-tests=error
```

The MSVC build used `Invoke-VsDevCommand.ps1` through Windows PowerShell with
`-ExecutionPolicy Bypass` to import the Visual Studio environment. The
current default PowerShell blocked direct script execution before compilation;
this was a launcher issue, and the verified native build and tests followed.
The wiki source validator passed: 116 Markdown files, 115 manifest pages,
646 local links. The nine-file scoped diff passed `git diff --check`.

All three platform runs exercised the exact EIS1 v1 golden, EIS2 direct
encode/decode and Oracle=42, distinct EIS2 payload selection, two-process
file roundtrip, unchanged outputs on rejected writes and reads, wrong
payload version, payload mutation rejected by its stored hash, and the same
mutation with recomputed hash rejected at successor byte offset 556.

## EIS2 acceptance decision

This fixed two-block artifact slice is accepted by the focused matrix above.
The EIS2 implementation does not add general CFG, maps, binding, relocation
resolution, ExecBC, native AOT calling, package copy, or ImportByPath
migration. Full 08.01 remains open.
