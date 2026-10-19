---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c
  - zr_vm_core/src/zr_vm_core/runtime/global.c
  - zr_vm_core/src/zr_vm_core/execution/execution_memory.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - tests/parser/test_unpublished_type_metadata_lifetime.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - tests/parser/test_unpublished_type_metadata_lifetime.c
  - tests/parser/test_function_type_conversion_lifetime.c
  - tests/parser/test_syntax_hygiene_field_results.c
doc_type: module-detail
---

# Type Prototype Metadata Ownership

M1 hygiene requires retaining the complete identity of field values. Before
those values acquire owned inferred-type storage, both successful publication
and failed declaration assembly must have a working metadata cleanup contract.
This support gate concerns owned compiler metadata, not AST replacement,
canonical binding rollback or complete syntax-expansion atomicity.

## Ownership Boundary

A local prototype owns inheritance, interface, generic-parameter, decorator
and member buffers before it is pushed into the compiler's prototype table.
Each pending member can independently own a structured type, parameter types,
parameter names, default flags and values, generic constraints, passing modes
and decorators. Pushing a member transfers that ownership to the prototype;
pushing the prototype transfers it to CompilerState.

The published destructor already recursively releases these buffers. Source
struct and class failure returns do not consistently release the corresponding
unpublished metadata. A pending member must be released separately only before
its push; after its push the prototype destructor owns it. A failure after the
prototype push must not free the local copy and leave a dangling published row.

The private lifecycle module extracts that established responsibility
and shares it with existing extern cleanup and source abort paths. AST nodes,
strings, compiled functions, GC-managed decorator values, symbol tables and
canonical records keep their existing owners. Property-local temporaries not
yet appended to the prototype are a separate boundary, not covered by merely
freeing its published member array.

The shared published destructor's order and guards are preserved: inheritance,
interfaces, type generic constraints, type decorators, then members. A member
releases its structured type, recursively owned parameter types, names,
defaults, nested generic constraint arrays, modes and decorators. Existing
extern cleanup delegates to this same contract, replacing its independent
member-first ordering. No GC-owned values are released by either helper.
Thirty-three source abort paths now release the unpublished prototype; thirteen
of those additionally release the pending member first. Later failures after
the prototype push retain CompilerState ownership and do not free the local
copy. Large source producers remain orchestration files with narrow helper
calls; the destruction responsibility is in its own private module.

## Completing Declaration Previews

LSP bootstrap publishes queryable previews with the real `declarationNode` and
`isDeclarationShell` set. Preview field signatures are not completed native
struct layouts; an enum name and underlying display are not runtime member
metadata. Normal producers zero-initialize the flag to false. AST identity
alone must not cause a preview to skip normal construction.

Extern struct, extern enum and ordinary enum share a private publication helper.
The existing duplicate check runs first: a distinct same-name AST in the same
ITEM context remains an error. A completed same-AST declaration is idempotent;
an open shell permits construction of an independently owned candidate through
the normal decorator, field conversion, layout and type-binding paths.

Construction may grow the prototype table. Only after success does publication
re-find the same-AST open shell, release its owned metadata through the existing
lifecycle, and move the completed candidate into that slot. Failure releases
only unpublished candidate resources and leaves the shell intact. Borrowed
AST/GC references retain their owners. This is metadata handoff, not global OOM
or canonical-fact rollback, and introduces no second FFI metadata builder.

Closed generic records have a non-invalid instance key even when they borrow
the same declaration AST. Skip and completion exclude those entries. Their
fieldwise copy explicitly inherits the source shell flag rather than marking a
derived preview complete; no new closed-instance completion policy is implied.

The dedicated prototype companion uses real parsed declarations. It checks
enum members, underlying type and runtime value; struct annotation ASTs, sizes
and explicit offsets; repeated normal publication; and failure after a valid
field precedes a missing type binding. The failure case requires the normal
diagnostic while retaining the original slot, member buffer and metadata bytes.
The actual pre-repair 96-case run has 91 PASS and five exact behavioral FAIL.
Normal GCC repaired replay `m1-lsp-completion-contracts-green-r1` then reports
96 PASS, native0 and six naturally terminal exit0 children. Source4908 identity
CF3B3177, HEAD4f53caa and support/configuration/environment/binary gates remain
stable. Receipt62CD3602 is focused support evidence, not platform/M1 acceptance.

## Genuine Full-Lifetime Fixture

Seven cases parse real struct/class declarations. Two successful publications
are controls. Two pending methods have a valid int parameter but an unavailable
Missing return annotation. Child conversion and the exact explicit-module
diagnostic are proved before measurement; the production member compiler
returns at its existing error guard, before compiling the method body. One
class is parsed with both abstract and final flags and is rejected after its
field has been appended. Two cases then compile a distinct Recovery declaration
through the same compiler state.

Seven post-repair companions additionally prove real `int[]` children and
exercise pending array parameters on both producers, successful compiled array
parameter methods on both producers, rejection after a valid class method has
been pushed, and an actual `break;` body error with valid annotations on both
producers. Successful methods expose their actual source AST, compiled function,
structured int return, array parameter and passing mode. These are fourteen
total cases; the companions receive no invented pre-repair RED credit. Nested
generic constraint rejection remains a separate dynamic coverage gap.

Class compilation lazily imports normal builtin metadata. Every class case
therefore first compiles a real `class Baseline {}` from the same retained
script AST outside the allocation window. Its actual source prototype, empty
member list and Root named binding are checked. Only its scalar prototype
count survives into measurement: rejection must preserve that count, and
successful subject/recovery publication must add exactly one row. The fixture
does not assume or manufacture a fixed builtin count.

The allocator ledger saves the real allocator and user data and delegates all
requests. It records successful ARRAY allocations and resizes, including a
resize of a pre-window buffer. Failed reallocations retain the old record.
Successfully released tracked buffers are removed even when a wrong tag is
observed; tag/size mismatches and ledger overflow are prerequisite failures.
It follows compiler, AST and complete runtime/global destruction, then requires
zero live tracked buffers and bytes. The saved allocator is an execution-budget
wrapper whose user data is the Global, so it must never be called after Global
destruction. No synthetic post-destroy reclamation is performed. The RED process
leaks raw buffers until process exit; RED teardown is not credited as leak-free.

## Native Evidence

- RED-r1 did not naturally finish. Root stopped only its identified own PID7788
  after finding a fixture-only post-Global allocator use. The driver terminated
  with exit1/build0/test-1; there is no behavioral RED credit or test footer.
- RED-r2 naturally terminated1/build0/test6: seven cases, one PASS, six FAIL,
  zero ignored. The struct failures observed eight live buffers; the four class
  cases stopped at incorrect builtin prototype-count assumptions and receive
  no class ownership credit.
- RED-r3 native5738 naturally terminated1/build1. The new baseline helper used
  a nonexistent AST field. No test ran and no behavioral credit is assigned.
- Corrected RED-r4 naturally terminated1/build0/test5: seven cases, two PASS,
  five FAIL, zero ignored. Both successful controls pass. All actual AST,
  child-conversion, diagnostic, compiler-context, baseline and recovery checks
  pass; only final survivor checks fail. Pending struct/class and their Recovery
  cases retain eight arrays each; abstract/final retains five. Independent
  read-only fixture review finds no further setup, counter or lifetime issue.

Logs retain the distinct `m1-msvc-unpublished-type-metadata-red-r1-*` through
`-red-r4-*` report stems. At that lifecycle checkpoint, the six-path repair is frozen at State
84CB83B6, Struct4B133E77, Class0BE0A8F0, Extern7DEE51EA, module7A6B938B and
header91FE3C84. Root independently reconstructs the four original raw files in
memory and reproduces their exact pre-lease SHA hashes FC79203A/ADE6E88E/
CE434C60/478085A4. The State's original 110-line destructor is regenerated from
the actual new module; unrelated raw bytes and mixed line endings are preserved.
Public and existing private headers are unchanged. Independent production
review finds no actionable regression. GREEN-r1 native67316 naturally ends
0/build0 with fifteen programs, 408 PASS, zero FAIL and zero ignored. GREEN-r2
native5625 adds the seven companions and naturally ends0/build0 with 415 PASS,
but reports a fixture-only C4090 const qualifier warning. Root corrects the
assertion helper's parameter qualifier; GREEN-r3 native21375 naturally ends
0/build0 with fifteen programs, 415 PASS, zero FAIL, zero ignored and no C4090.
No own new production-source warning appears beyond the established D9025
option override. Logs retain all three distinct GREEN stems.

The lower nine-case function-type lifetime replay native48500 independently
passes after this repair. Upward field RED-r3-after-prototype-lifecycle
native4010 naturally ends1/build0/test11 with the same twenty-case nine PASS/
eleven FAIL result-identity losses. The lifecycle source lease is retired and
frozen; that evidence alone implies no field capture or complete M1 acceptance.
The [function-type ownership](function-type-conversion-ownership.md)
lower gate is GREEN. The later [field result identity](macro-field-result-identity.md)
repair adds separate capture hooks to struct/class/extern sources while the State
and lifecycle helper remain unchanged. Its first twenty-eight field cases pass,
but expose an [open array annotation](open-array-annotation.md) prerequisite.

Ten post-repair ownership companions extend this fixture to twenty-four cases.
They use actual source struct/class and native-extern struct producers for
successful array capture, failed callback annotation and distinct Recovery
declaration, plus an abstract/final class rejected after capture. Pre-window
checks prove the real int[] field, int[] callback parameter, return conversion
and complete function annotation path. The callback converter owns temporary
signature buffers but its result is a display-only CLOSURE; no retained signature
children or field-callability claim is made. The strict ledger follows captured
int[] ownership and pending-conversion cleanup through full Global destruction.
Independent fixture review accepts these companions. The standalone
`m1-msvc-unpublished-type-metadata-green-r4-fields-*` replay under native42537
naturally ends build0/test0 with all twenty-four cases PASS, zero FAIL, zero
ignored. No prior RED credit is invented for the ten companions. M1 acceptance,
toolchain matrix and original programs
remain open.
