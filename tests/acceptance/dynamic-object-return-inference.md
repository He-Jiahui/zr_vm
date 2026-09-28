# Dynamic OBJECT Return Inference

## Accepted contract

An unannotated local can hold a runtime OBJECT value while its source binding
has no canonical TypeId. Reading that local in `return 42 + marker` must let
expression type inference continue. The read's semantic reference fact retains
the real source SymbolId and `ZR_SEMANTIC_ID_INVALID` TypeId; inference must not
invent a canonical type or claim that an active source CFG can lower the load.

`ZrParser_IdentifierType_Infer` had returned false immediately after a
successful type-environment lookup and reference-fact recording when such an
OBJECT local had no TypeId. It left `cs->hasError` unset. The caller treated the
return expression as uncompiled, so the old pinned Span script ended by
returning the result of its second `pin.close()` instead of compiling its
explicit `return value + gcMarker` statement.

The narrow fix lets this identifier inference succeed and preserves the
unresolved TypeId in the reference fact. The new test in
`test_type_inference_dynamic_return.inc` checks the source binding, expression
result, and reference identity independently of code generation.

## Focused evidence

The GCC Debug cache is `D:\tmp\zr_vm\ssa-optional-member-gcc`. Its RED build
completed 12/12 steps. Before the production change, the new direct inference
test failed `Expected TRUE Was FALSE`; the type suite reported 125 tests and
three failures. After the guard change and a 4/4 target rebuild, the new case
passed. The suite then reported 125 tests and two failures, both already in
the RED run:

- `test_type_inference_source_generic_class_member_substitutes_closed_field_type`
- `test_type_inference_source_generic_inheritance_substitutes_closed_base_member_type`

The focused logs are `dynamic-return-red-type.log`,
`dynamic-return-green-type.log`, and `dynamic-return-green-build.log` in that
cache. The complete type suite is therefore not accepted as green.
The existing `zr_vm_pre_semantic_ir_test` binary, dynamically linked to the
rebuilt parser library in the same cache, ran 114 tests with zero failures.
Its output is `dynamic-return-green-presemantic-smoke.log`; this was a smoke
run of the existing binary, not a rebuild of that test target.

## Open downstream boundaries

An isolated straight-line source CFG fixture with the same two-line script
became blocked at `Failed to lower load through pre-execution Semantic IR`
after identifier inference succeeded. Its experimental run reported 36 tests,
one failure; its 35 existing cases passed. The fixture was removed from this
change so it does not add a failing test. The active-CFG load path needs a
separate producer/fallback investigation, while the unresolved TypeId contract
above stays intact.

The fresh FFI target built 7/7 steps after the type fix. The old pinned Span
positive case then failed compilation at `ffi_pinned_span_gc.zr:11:10-11:12`
with `Active contiguous view prevents source move or drop` on the first
`pin.close()` after the last `view[1]` read. Its focused GDB run reported five
tests, one failure; the four post-close guard cases passed. No runtime value
of 43 was observed after this change. View lifetime analysis is a separate
open issue.
