---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - tests/parser/test_ssa_sccp_conversion.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - docs/plans/ssa/guides/B-passes-analysis.md
tests:
  - tests/parser/test_ssa_sccp_conversion.c
doc_type: acceptance-record
status: scoped-accepted
---

# SCCP conversion runtime representation gate

Baseline: `76786825`. This slice changes exactly:

- `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c`
- `tests/parser/test_ssa_sccp_conversion.c`
- `docs/instruction-generation/execir-pass-pipeline.md`
- this acceptance record.

## Actual runtime RED and GREEN

Diagnostic files and owned outputs are below
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-representation`.
`witness.c` includes an exact frozen copy of the previous ten-case fixture.
Its before/after calls invoke the actual Oracle and ExecBC projection runner;
its SCCP calls invoke the actual scalar pass pipeline. No runner, pass,
verifier, or allocator is stubbed. `run_witness.py` reuses the formal conversion
target's support objects and three archives read-only, excluding the formal
test object. Each phase records its compile command, exit codes, witness hash,
and before/after hashes for every borrowed object/archive in `receipt.json`.
The GREEN phase replaces only SCCP with the object compiled from this slice
by `compile_pass.py`; `pass-receipt.json` records its source hash and command.

| Evidence | Compile | Run | Observed result |
| --- | --- | --- | --- |
| `immutable-red.log`, old SCCP | 0 | 1 | Existing ten cases pass; 16 runner mismatches |
| `suite-red.log`, new tests with old SCCP | 0 | 1 | Actual Oracle return-kind assertion fails |
| `loop-red.log`, new loop tests with old SCCP | 0 | 1 | Signed backedge passes; late BOOL backedge fails the dependent lattice assertion |
| `compile-pass.log` | 0 | n/a | Actual modified SCCP compiled |
| `immutable-green.log`, modified SCCP | 0 | 0 | Zero conversion runtime mismatches |
| `suite-green.log`, complete fixture | 0 | 0 | 33 always-active cases pass |

For same-token immediate DOUBLE/FLOAT, BOOL, and UINT64, both explicit and
fallback targets reproduce the defect in both actual runners: the original
conversion returns FLOAT7, BOOL1, or UNSIGNED7, while old SCCP replaces it with
a SIGNED7 immediate CONST. All 16 results preserve their kind and payload
after the fix. Canonical signed conversions still fold, including INT8 through
INT64 and a signed COPY chain. Custom tokens conservatively retain CONVERT.
The identical witness source hash is recorded in immutable RED and GREEN
receipts; every borrowed object/archive remained unchanged during each run.

The reproduction command is:

```text
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py <log> D:/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-representation python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-representation/run_witness.py immutable-red --baseline
```

GREEN uses `immutable-green` and supplies
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-representation/exec_ir_sccp.obj`
instead of `--baseline`. The complete production fixture uses `--suite`.

## Representation proof and loop coverage

CONVERT requires matching canonical signed tokens and a private proof of
actual SIGNED representation. Immediate constants establish that proof;
canonical signed pool constants establish it under their typed runtime input
contract. COPY/MOVE preserve it. Equal-bit/type joins intersect the proof and
equality includes the proof, so a late loss triggers dependent reevaluation.
Arithmetic/NEG preserve proof only from proven signed operands; COMPARE
does not. This metadata does not alter other opcodes' numeric transfer,
branch behavior, or the public analysis-cache structure.

The always-active loop tests build a real four-block SSA loop, verify it with
`ZR_EXEC_IR_VERIFY_ALL`, and run both actual runners before/after the pipeline.
A cross-type BOOL condition is runtime false but OVERDEFINED for SCCP, so both
loop edges participate in analysis. Its entry has SIGNED1; the late backedge
has either a SIGNED COPY of 1 or a BOOL comparison result annotated INT64 with
the same bits. The PHI remains CONSTANT1 in both cases. Only the first proves
SIGNED storage and permits its dependent CONVERT to fold; the second must
reevaluate to OVERDEFINED and preserve the conversion. Both return SIGNED1.
FLOAT pool COPY/NEG fixtures independently reject reannotation as signed proof
while preserving real conversion results in both runners.

## Independent pool analysis limitation

The actual witness also supplies DOUBLE negative zero (encoded bits
`0x8000000000000000`) to a branch whose true/false arms return SIGNED111/222.
Before and after old SCCP, both runners return 222 and the conditional branch
remains unchanged. Nevertheless, the cache claims the true-arm definition is
CONSTANT and the false-arm definition is UNKNOWN: raw bits were used as truth.
The fixed DOUBLE-CONVERT path makes both arms participate, but a direct pooled
CONST branch retains the incorrect cache facts. This slice deliberately does
not repair `mark_successors`, pooled CONST decoding, or binary float semantics.
No runtime divergence is claimed for this separate diagnostic.

## Platform scope

MSVC focused actual-runtime validation above is GREEN. WSL GCC 11.4 is
available (`wsl.exe -e /usr/bin/timeout 10 /usr/bin/gcc --version`, exit 0).
This session's immediately preceding incremental GCC support build already
recorded 60–105 second DrvFS source compiles followed by an owned-process-group
120-second timeout; no matching complete GCC conversion-object manifest was
available. The same multi-source DrvFS build was not retried. Those earlier attempts do
not establish a passing GCC gate. The distinct fresh Clang sanitizer gate and
formal MSVC verification completed below; Valgrind was not run.

## Fresh Linux sanitizer and independent root verification

Root rebuilt the current repository MSVC target successfully (exit 0).
The registered `ssa_sccp_conversion` CTest passed in 0.11 seconds with all
33 cases. The accompanying 27-case MUL materializer suite also passed, and
CTest exited 0. Exact commands and output are retained in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/sccp-representation-multiply-build.log`
and `sccp-representation-multiply-ctest.log` in the same directory.
Independent review verified the frozen four-file source hashes and found no
introduced actionable finding. Root separately inspected proof intersection,
equality and dependent reevaluation, then confirmed the formal runtime output.

A separate worker compiled all 34 focused source units afresh with Clang 14
shared ASan/UBSan instrumentation; every compiler exited 0. It did not reuse
the predecessor's objects whose historical full-header hashes were unavailable.
All 6,435 captured source/header hashes were unchanged before and after the
fresh compilation. The initial audit exited 1 because two `../` dependency
aliases were not normalized and Clang's non-header `asan_ignorelist.txt`
resource lay outside that manifest. The corrected alias audit exited 0;
the original result is retained. The resource's post-compile SHA is recorded
separately, with no claim of a pre/post resource comparison.

Native LLD linked the new Linux objects in the recorded order with unchanged
instrumentation and a private path-only libc linker script (exit 0).
`readelf` confirmed an x86-64 ELF, Linux interpreter, shared ASan dependency
and address/undefined-behavior instrumentation symbols. Actual Linux execution
with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` passed all 33 cases (exit 0).
The final ELF SHA256 is
`2aa1ed398e765dd3104f8024b3f9e16d8495755df57aad1b4ee8482bf610d91b`.
The compile/link/dependency/ELF/run receipts and bounded logs are under
`sccp-representation/linux-clang-sanitizer/` below the owned D: task root.

Root independently executed that same instrumented ELF with leak detection
and UBSan halt-on-error enabled: 33 cases passed, exit 0, 1.434 seconds.
The exact command and output are retained in
`control/sccp-representation-root-clang-sanitizer.log` below the task root.

This representation-gate repair is scoped-accepted on formal MSVC Debug and
fresh Clang Linux ASan/UBSan. GCC and Valgrind remain unverified for this slice;
the pooled-branch analysis risk above and the full 02.01 milestone remain open.
