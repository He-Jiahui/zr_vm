---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_core/include/zr_vm_core/array.h
  - tests/parser/test_function_type_conversion_lifetime.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.h
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_function_type_conversion_lifetime.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_syntax_hygiene_field_results.c
doc_type: module-detail
---

# Function Type Conversion Ownership

Function annotations are converted through the same AST-to-inferred-type
primitive used by field publication. A producer must not wrap an unsafe failure
path: successful parameter inference can allocate owned storage before a return
annotation is rejected. This is a lower support gate for M1, not a new callable
representation or public ABI.

## Existing Signature Boundary

The signature helper initializes parameter-type and passing-mode arrays with
capacity equal to the parsed parameter count. Core arrays use capacity one when
that count is zero. Each parameter type is an owned inferred value. Parameter
conversion failure already releases the pending parameter and both arrays.

The original explicit return conversion returned directly on failure, leaving
the parameter arrays without an owner. The private function-type module now
initializes the return value before parameter conversion and uses one failure
release helper. It recursively releases every initialized parameter and return
value, then both array buffers, and resets the outputs to freeable empty states.
Successful conversion retains its existing caller-owned cleanup and display
behavior. The callable-name builder and mode prefixes moved into this same
private module; no public inferred-type or compiler ABI changed.
Canonical bindings, diagnostics and source ASTs have independent owners.

## Actual Allocator Fixture

The nine cases parse genuine variable annotations. Successful one/two-parameter
functions and a missing parameter are controls. Missing returns cover zero,
one and two parameters. An unavailable opaque Widget return is tested only
after an actual Root Widget binding, canonical nominal identity and successful
Root child conversion have been proved.

Two later companions verify an owned array parameter (`fn(int[]) -> Missing`)
and a successful `fn(in int, ref int, out string) -> int`. The former proves the
parsed one-dimensional annotation and actual ARRAY/INT child structure before
measuring recursive cleanup; the latter checks each parsed mode and exact
result display. Neither invents a parameter type or signature record.

Children and error-message storage are warmed independently. Failed complete
signatures are never used as warm-up calls. The probe saves both the global
allocator and its user data, delegates all allocations, and counts only
successful ARRAY allocations and frees during one complete conversion and
result cleanup. A successful realloc counts release of the old block and
allocation of the new block; a failed realloc counts neither. The allocator is
restored before assertions and again defensively during teardown.

Primitive Root cases require exactly two allocations and their exact capacity
bytes. Opaque context queries allocate scratch arrays, and composite parameters
own nested arrays, so those companions permit additional allocations but still
require total allocation/free count and byte balance. This does not assume
normalization scratch is cached. The native RED
process exposes leaked raw buffers; its teardown is not credited as leak-free.

## Evidence

RED-r1 native18479 naturally terminates1/build0/test4: seven cases, three PASS,
four FAIL, zero ignored. The zero/one/two-parameter missing-return cases pass
exact allocation prerequisites and observe two allocated arrays versus zero
freed arrays. The marked-return case instead fails an overly strict fixture
allocation-count prerequisite (ten instead of two); it receives no functional
RED credit. The corrected fixture preserves the strict Root checks while
accounting for contextual scratch. Fresh RED-r2 naturally terminates1 with
build0/test4: three PASS, four FAIL, zero ignored. The three Root return failures
still observe allocated2/freed0; the marked companion observes allocated10/
freed8 after every source and child prerequisite passes. Independent read-only
fixture review accepts the corrected ledger and finds no other issue. Logs are
`m1-msvc-function-type-lifetime-red-r2-*`.

The three-file private extraction and cleanup repair is frozen at root
452DC6EE, module39F6015E and headerB11D42F8. Root independently reverses the
complete change in memory and reproduces prior raw source SHA C41D173D,
including its mixed line endings. Shared internal and public headers remain
unchanged. Independent static production review finds no actionable issue.

GREEN-r1 native66218 naturally terminates0/build0: fifteen programs, 408 PASS,
zero FAIL and zero ignored, including all seven initial cases. GREEN-r2
native89345 adds the two companions and naturally terminates0/build0 with
410 PASS, zero FAIL and zero ignored. Its incremental build also rebuilds a
disjoint foreign frame-roots source; no whole-tree immutable-state claim is
made. No new own source warning appears beyond established MSVC D9025 option
override messages. Logs use `m1-msvc-function-type-lifetime-green-r1-*` and
`m1-msvc-function-type-lifetime-green-r2-*`.

The immediate upward field replay `m1-msvc-field-results-red-r2-after-converter-*`
naturally terminates1/build0/test11 and still has nine PASS/eleven FAIL/zero
ignored at the same result identity checks. Field publication remains a
separate repair; the lower source lease is retired and M1 remains unaccepted.

See [field result identity](macro-field-result-identity.md) and the
[validation ledger](../testing-and-validation/macro-foundation-validation.md).
