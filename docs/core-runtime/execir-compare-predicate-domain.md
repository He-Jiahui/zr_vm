---
related_code:
  - tests/parser/test_ssa_compare_predicate_domain.c
  - tests/cmake/ssa-compare-predicate-domain.cmake
  - tests/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
implementation_files:
  - tests/parser/test_ssa_compare_predicate_domain.c
  - tests/cmake/ssa-compare-predicate-domain.cmake
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
plan_sources:
  - .codex/plans/20261004-core-compare-predicate-domain.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_compare_predicate_domain.c
  - tests/cmake/ssa-compare-predicate-domain.cmake
doc_type: test-module-detail
---

# Core COMPARE selector domain fixture

Status: Root's V51 actual semantic RED adopted; the authorized finite Core and
Oracle guards are implemented, with actual GREEN pending Root. The fixture and
registration remain frozen. Shared selector/header and source producer work is
separately owned. This document records the finite behavior and observed RED;
it does not claim production acceptance before fresh GREEN execution.

## Historical selectors and value kinds

COMPARE uses `instruction.typeToken` as its operation selector. Its value
records' type tokens remain opaque identities; tagged Oracle initial values
determine signed or float evaluation. The actual historical switches define:

| Selector | Shared name suffix | Predicate |
| --- | --- | --- |
|0|EQUAL|left == right|
|1|LESS|left < right|
|2|LESS_EQUAL|left <= right|
|3|GREATER|left > right|
|4|GREATER_EQUAL|left >= right|
|5|NOT_EQUAL|left != right|

These are `ZR_EXEC_IR_COMPARE_KIND_*` names, not canonical TypeIds. The
Oracle default previously accepted other selectors as EQ. The implemented
guards reject that path while retaining the whole generic0..5 domain.

## Actual Core API input

The fixture allocates Core-owned values, block, operand/result pools and
instructions through Function APIs. It has two external UNKNOWN values and
one COMPARE-defined boolean result. COMPARE source501 and RETURN source502 are
the only instructions. No parser lowering, fabricated typed bytecode, constant
pool, provider callback, CALL, FFI or scheduler operation participates.

Each of six named legal cases first passes CoreVerifyALL, then runs the actual
direct Oracle for signed and finite float inputs in each of three relations:
less, equal and greater. The36 expected boolean truths come from explicit
tables; each run must return a bool, execute exactly two instructions and emit
zero observable events. This covers both COMPARE switch branches.

Unknown7,9 and UINT32_MAX cases first establish a legal EQ Core/Oracle baseline.
Only their actual instruction selector changes. CoreVerifyALL, signed direct
Oracle and float direct Oracle must each return false with the exact diagnostic:

- INVALID_VALUE, functionToken43, blockId0, instructionId1, sourceId501.
- expectedVersion5 and actualVersion equal to the rejected selector.

The initialized Oracle result stays empty with no executed instruction and no
return/event publication. Selector and source IDs remain unchanged. These are
independent consumer checks; failure of CoreVerify does not suppress the direct
Oracle guard test. The historical unguarded RED path is bounded pure COMPARE/RETURN.

## Markers and acceptance boundary

Expected semantic RED is9 cases,3 failures,0 precondition failures,60
precondition passes and36 truth passes. Only the three unknown cases fail;
all nine rejection diagnostics report FAIL. Expected GREEN changes the failure
count to0 and those nine rejection markers to PASS. Natural exits are1 and0,
respectively. Build/link or baseline verifier/Oracle failures do not establish
feature RED. Root observed V51 actual RED before authorizing production guards.

Declared support is20 TUs: fixture +14 existing Core support +five Oracle TUs,
fully listed in the plan. There are no parser/harness TUs. Root owns actual
native dependency/link capture and execution. Root compiled/linked the closure
in V49 and ran V51 with a fresh fixture plus nineteen unchanged sealed support
objects. Changed production requires a fresh GREEN epoch; no runtime result is
inferred from source presence.

## CTest registration

`tests/CMakeLists.txt` includes the independent fragment, defining executable
`zr_vm_ssa_compare_predicate_domain_test` and CTest
`ssa_compare_predicate_domain`. The fragment lists all20 actual fixture/Core/
Oracle TUs, derives the checkout root relative to its own list directory, and
uses only Core/common includes. It can be included from a bounded standalone
C project without any parser, harness, Core module target or provider library.
Standard libm is linked on UNIX.

C11 is required with extensions disabled. /UNDEBUG or -UNDEBUG keeps assertions
enabled in every build configuration. The actual executable's natural exit
determines CTest status; no WILL_FAIL or wrapper masks initial RED. Labels are
ssa/core and timeout is30 seconds. Actual configure/build/CTest remains pending
Root; the fixture and shared selector header stay frozen during that gate.

Root's V49 native20-TU compilation/link passed, but all nine cases failed the
Build precondition before any truth or rejection check. That immutable false
receipt is not feature RED. Actual Core AddBlock requires an invalid entry ID
before ENTRY insertion and sets it itself; the fixture had assigned entry1 too
early. The fixture-only repair removes that assignment and retains the actual
API path. All selectors, truth/rejection assertions and expected markers stay
unchanged. V51 subsequently established actual semantic RED; CTest configuration
still requires Root's separate native gate. The V49 failure itself did not
authorize production guards.

## Actual RED and finite production guard

Root V51 receipt SHA256
`7950a2a21aa9dfe910da29f151bc29a4a513d6ce756cc53b5e1e624dab65043b`
(122509 bytes) records expected RED and validation-pipeline acceptance true,
with semantic acceptance false. Formal support is20 TUs, one fresh repaired
fixture and nineteen unchanged V49 support objects. Natural exit1, closed EMPTY
job and no outstanding actions accompanied9 cases,3 unknown failures,0
precondition failures,60 precondition passes,36 truth passes and9 rejection
diagnostic failures. Whole pipeline elapsed33.106 seconds. The six legal
selector cases passed; the three unknown cases each exposed both Core and
independent signed/float Oracle acceptance.

After that evidence, the finite repair changes exactly three production files:

- `exec_ir_verify.c`: existing instruction metadata validation rejects COMPARE
  selector above named NOT_EQUAL using INVALID_VALUE and exact instruction/source.
- `exec_ir_interpreter_validate.c`: the independent Oracle validation scan
  applies the same domain before allocating or publishing an execution result.
- `exec_ir_interpreter.c`: both numeric switches use all six shared selector
  names, with EQUAL explicit; unknown default emits INVALID_VALUE and fails.

Both metadata rejection paths report block0, expected NOT_EQUAL and the actual
selector. Other opcode selectors, public APIs, header/producer/parser behavior,
numeric conversion and each legal relation expression remain unchanged. No
new private helper or translation unit is needed. Fixture and registration
hashes stay frozen during Root's fresh GREEN/repeat/regression gate.

The finite truth matrix does not establish unsigned edge cases, NaN semantics,
source lowering, other backends or a complete SSA milestone. It checks the
historical legal selector meanings and rejection before unknown EQ fallback.
