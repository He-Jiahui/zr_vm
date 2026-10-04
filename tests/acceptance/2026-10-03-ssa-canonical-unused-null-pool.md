---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_execbc_vm_canonical_types.inc
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/test_exec_ir_scalar_scratch_eligibility.c
doc_type: testing-guide
status: scoped-accepted-msvc-native
---

# Canonical unused NULL pool adaptation

## Scope and contracts

The canonical type adapter accepts a primitive NULL constant descriptor only
when its flags and bits are zero and no CONSTANT instruction references its
pool index. It translates the descriptor in its private constant-array copy
to `ZR_VALUE_TYPE_NULL`; it preserves every original pool entry and index.
NULL is distinct from the Oracle's undefined storage kind, and no NULL is
substituted with an integer or bool.

NULL value annotations, instruction type targets, match tokens and nullable
canonical wrappers remain unsupported. Referenced NULL constants are rejected
with instruction/source diagnostics. Unknown canonical types still reject even
when unused. Malformed NULL flags/bits produce `INVALID_VALUE`; a high 64-bit
payload is saturated to `UINT32_MAX` in the diagnostic's 32-bit actual field.

The normal full VM materializer remains the final consumer. Its scalar,
memory/effect, ownership, CFG/PHI, GC/deopt, CONVERT and STORE guards are retained.
The existing planner emits referenced constants into the Core function; a
two-entry projection with INT64 42 and unused NULL stays intact while the
emitted function has one constant. There is no public structure, schema, ABI,
wire-format or language change. This is a lower adapter correction, not full
source-loop VM support or completion of 01.02/01.05.

## Test-first RED and scratch GREEN

Evidence was collected on 2026-10-02 UTC, 2026-10-03 in Asia/Shanghai, under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/source-loop-phi-audit/`.

The unchanged root MSVC libraries ran `unused_null_probe.c`: compile/link exits
0/0, execution exit 2, three Unity tests with two failures. Legal unused NULL
failed with `UNSUPPORTED` 28 at pool resolution; an INT64-only baseline
materialized and the actual Core VM returned 42. A malformed NULL descriptor
also failed the expected more precise `INVALID_VALUE` assertion because the
old adapter rejected its type first. This RED history remains valid evidence
of the original adapter boundary.

The D-only candidate `unused_null_adapter.c` was compiled under a distinct
scratch symbol without changing the root libraries. `unused_null_patch_probe`
completed compile/link/run with exits 0/0/0: three Unity tests, zero failures,
all thirteen mutations checked. The legal unused NULL case and baseline each
executed the actual Core VM and returned 42. All eleven negative mutations
left output empty and projection arrays unchanged. The final logs compile
without the earlier missing execution-header prototype warning.

Both final hash receipts record 26 unchanged borrowed libraries, support
objects and repository inputs. Evidence files are `unused_null_probe.log`,
`unused_null_patch_probe.log`, and their `-input-hash-receipt.json` receipts.
These scratch results establish candidate behavior; they are not the formal
repository CTest result.

## Registered fixture coverage

The existing two registered canonical adapter tests now include all thirteen
configurations. They use active Unity assertions without adding a test-main
or CMake registration path.

| Configuration | Required result |
| --- | --- |
| legal unused canonical NULL + INT64 42 | actual VM 42, original pool count 2, emitted Core pool count 1 |
| INT64-only baseline | actual VM 42 |
| referenced NULL constant | UNSUPPORTED, instruction 1/source 920 |
| NULL value annotation | UNSUPPORTED, instruction 1 |
| NULL instruction target | UNSUPPORTED, instruction 2/source 921 |
| NULL match token | UNSUPPORTED, instruction 1/source 920 |
| unknown unused constant ID | UNSUPPORTED |
| nullable INT64 unused constant | UNSUPPORTED |
| NULL bits 1, flags 1 or high 64-bit payload | INVALID_VALUE, expected 0, actual 1 or UINT32_MAX |
| MAY_GC flag on constant | unchanged full validator rejects UNSUPPORTED at instruction 1/source 920 |
| malformed RETURN-to-STORE instruction shape | unchanged full validator rejects INVALID_RANGE at instruction 2/source 921 |

Every case asserts fixture construction and byte-for-byte preservation of
instruction, value and constant arrays. Every failure asserts empty emission,
exact diagnostic code and instruction/source positions. The malformed STORE
case checks a shape guard; it does not establish valid STORE execution or
rejection order. The previous eight canonical metadata negative inputs and
COMPARE tag preservation remain covered.

## Current repository verification

On 2026-10-02 UTC (2026-10-03 Asia/Shanghai), Root built the frozen repository
target `zr_vm_ssa_exec_ir_execbc_vm_test` in the MSVC matrix: build exit 0,
elapsed 86.805 seconds. The exact registered CTest was:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R "^ssa_exec_ir_execbc_vm$" --output-on-failure --no-tests=error
```

The first CTest attempt timed out after 41.61 seconds, exit 8, with no test
stdout. Its cause is unlocalized. This record does not attribute the timeout
to startup, environment, or a particular test. The failed attempt remains in
`control/unused-null-formal-ctest.log` (total test time 41.76 seconds; wrapper
elapsed 42.553 seconds).

Root then ran the same executable directly from the control directory: 27
Unity tests, zero failures, zero ignored, exit 0, elapsed 1.709 seconds. Its
SHA-256 was unchanged:
`325aa4d3d96d43085003b1f2f909fcc6befb3c4ecf86018464ecdf0de0f3d64f`.
The unchanged executable passed the exact registered CTest retry, 1/1, exit 0:
test time 0.57 seconds, total CTest time 0.69 seconds, wrapper elapsed 1.042
seconds. The 27 registered cases include all thirteen NULL configurations
inside the two existing canonical adapter cases.

Formal evidence is under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/`:
`unused-null-formal-build.log`, `unused-null-formal-ctest.log`,
`unused-null-root-direct.log`, and `unused-null-formal-ctest-retry.log`.
The current decision is scoped acceptance for the recorded MSVC native
adapter gate, with the initial unexplained timeout retained. Adjacent source
and eligibility acceptance retains its previously recorded scope; this exact
retry is not evidence of a fresh broader test selection.

The real source-loop audit published genuine header PHIs and matched Oracle/
ExecBC results in five cases, but actual VM materialization remained RED for
the mixed canonical/runtime CONVERT contract or the unused NULL pool boundary.
This adapter correction does not remove residual STORE guards or establish
whole-source-loop VM success. The separately reviewed CONVERT contract design
remains open. GCC/Clang and sanitizer verification of this change is pending.
