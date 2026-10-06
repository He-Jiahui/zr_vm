---
related_code:
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
implementation_files:
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
plan_sources:
  - .codex/plans/20261005-ssa-host-aot-target.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - docs/acceptance/ssa-host-aot-target.md
doc_type: testing-guide
status: finite-host-aot-target-green
---

# Host No-Argument I64 AOT Target Test Guide

## Actual fixture and modes

The new fixture is `tests/parser/test_ssa_host_noargs_i64_aot_target.c`.
It links the existing `ssa_literal_script_fixture` implementation once; it
does not copy it, include a `.c` file or recreate the source pipeline.
Target `zr_vm_ssa_host_noargs_i64_aot_target_test` and CTest
`ssa_host_noargs_i64_aot_target` register the direct fixture. Actual r2
configure/build/CTest exited 0; the full target suite passed all 14 cases.

| Mode on the whitelisted host | Intended cases and distinction |
| --- | --- |
| `--prerequisites-only` | two real 9/8 prerequisite cases; no target success assertion |
| `--features-only` | three target-success cases; each still prepares and checks real prerequisites |
| default | two prerequisite, three feature and nine refusal cases, 14 total |

The alternate unsupported-host branch is compilable source for a refusal
check. It supplies no dynamic Linux/other-target evidence and no inferred
skipped-test result. The real RED stub passed two prerequisites, then failed three feature assertions.

## Prerequisite chain

Each literal source is processed through actual public parser, canonical,
preparation, compiler, assembler, finalizer and module APIs from the shared
fixture. The compacted graph retains three instructions and its real NOP,
IDs, function token, signature hash and source maps. VERIFY_ALL checks the
source and compacted graph. Source observations must remain unchanged.

The fixture obtains the real callable return type, constructs and appends an
actual host INT64 layout row, attaches an owned primitive frame and lowers
through `ZrParser_ExecIr_LowerAotWithCanonicalCallable`. It checks the
NOARGS_I64 callable, frame row/slots/hash, constants, slot assignments and
source-map coordinates. The present empty state map keeps its real function
token, signature hash and generation; zero counts do not erase its identity.
The projection remains nonrunnable. No Oracle or backend execution is used.

## Success and refusal assertions

The three feature cases request a target for real 9, compare real 8 with real
9 across distinct semantic contexts and literal bits, then repeat the request
with and without a diagnostic. An independent arithmetic byte encoder computes
the expected schema hashes through Core Stable64; it does not invoke the
production encoder. Every target field and Core ValidateTarget must match.
Successful diagnostics are all zero. Full observations before/after detect
mutation of live source, graphs, module tables and projection storage.

The nine refusal cases cover null required arguments, zero callable ID, an
actual primitive ID used as callable, zero actual row ID, contradictory row
type/size/alignment/hash, and actual readable context length above capacity.
They check failure status, complete output-byte preservation and unchanged
observations. Temporarily modified row/context metadata is restored before
any assertion, digest or teardown. No arbitrary pointer, fabricated canonical
source, out-of-bounds probe, GC stress or large allocation is used.

Actual refusal expectations match the final API: null arguments INVALID_ARGUMENT,
zero callable INVALID_ID, primitive callable INVALID_SIGNATURE, contradictory
rows INVALID_LAYOUT and context length above capacity INVALID_RANGE. The nine
refusal cases contain eleven calls because the null case makes three calls.
An ordinary fixture/setup/encoder failure must be retained as a failed
prerequisite, and cannot establish genuine target-feature RED.

## Ownership and finite proof

Global owners are initialized before Unity assertions. Teardown frees AOT
projections before compacted graphs and source fixtures, then destroys the
runtime. Borrowed views remain valid only while their real owners live.
Byte observations are mutation detectors within one test, not ABI hashes.

The [acceptance record](../../docs/acceptance/ssa-host-aot-target.md) owns
actual mode exits, counts, source/log/binary/metadata pins, failed prerequisite
history, RED commit and later GREEN. Direct checkout builds reuse the E-drive
build/reports/tmp roots without source snapshots.

Descriptor/module binding, real-frame scalar backend, source/native execution,
retention, Linux/full MSVC matrices and full SSA47 remain OPEN. No network,
FFI, provider, capability or security execution is part of this target gate.

## Actual finite result

RED commit `ebb1db928eb01dc6351d3b659971423d4846e2c4` follows two prerequisite
passes and three feature failures. r2 passed 14 target + 15 host layout + 19
source frame cases, zero failures/ignored and no observed UBSan diagnostic.
The independent encoder consumes 52 triple bytes and 88 ABI bytes (the fixed
triple itself is 22 bytes). MSVC /c passed the new TU with zero warnings.
The aggregate r2 driver exit was not observed after interruption; phase exits
and naturally terminal MSVC evidence are recorded separately in acceptance.

## Separate source module contract and descriptor successor

Host target GREEN completed at `6bdedf45c1b202cd1f2c1f4164bc0c235611fc46`.
Its original proof above remains frozen. The independent
[source module contract and descriptor gate](ssa-source-aot-descriptor.md)
binds the actual original module before compaction; actual RED is committed
and r2 functional 55-case GREEN passed. Both changed TUs passed MSVC compile-only checks; full matrix remains OPEN. A valid target record does not prove
module binding, borrowed descriptor lifetime, native execution or retention.
