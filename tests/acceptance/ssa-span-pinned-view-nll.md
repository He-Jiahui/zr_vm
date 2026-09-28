---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/include/zr_vm_parser/semantic_value_facts.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/parser/test_span_core.c
  - tests/parser/test_span_core_pinned_nll.inc
  - tests/parser/test_reference_loan_nll.c
  - tests/parser/test_buffer_pool_ffi.c
doc_type: acceptance-record
status: partial
---

# Pinned Span scalar last use and loan liveness

## Scope and cause

A pinned `Span<u8>` view used to keep its source loan live through a later
primitive return expression. The last view read produced a SemIR `CONVERT`
whose input was the view value but whose result was a resolved `VALUE` scalar.
Both global and CFG reaching-value propagation copied the view loan to that
scalar result. Consequently, `pin.close()` after the read was rejected as an
active contiguous view conflict, even though no later instruction used the
view.

The liveness pass now stops propagation into a result only when its semantic
value has a valid, matching type witness and explicit `VALUE` ownership.
Unknown and reference-like results retain the conservative propagation. The
input view remains a use at the read or conversion, so `pin.close()` before a
future view read is still rejected. Created-loan and contiguous-view seeding
are unchanged. This is a small rule in the existing
single-purpose loan-liveness module; extracting a new module for it would
split the same two propagation loops without separating a responsibility.

## Test-first GCC evidence

The separate `test_span_core_pinned_nll.inc` fixture checks that a pinned view
read into a scalar permits `pin.close()` and that the function returns 43. It
also checks that closing the pin before a subsequent `view[0]` read fails to
compile. The parser test helper returns only a compiled function or NULL, so
the negative diagnostic is verified from the focused executable's output.

With only the test fixture added, a fresh GCC Debug `zr_vm_span_core_test`
build completed. The direct run reported **18 tests, 4 failures**. The new
positive failed `Expected Non-NULL`: the compiler reported `Active contiguous
view prevents source move or drop` at `span_pinned_scalar_last_use.zr:9:10`,
the `pin.close()` after the last read. The new negative passed and reported the
same diagnostic at `span_pinned_close_before_read.zr:6:10`, its close before
the later read. The other three failures were already present in Span core.

After the two propagation sites were guarded, the incremental GCC build
recompiled `semantic_ir_loan_liveness.c`, relinked the parser library, and
relinked the Span test executable. The direct run reported **18 tests,
3 failures**: both new cases passed, including runtime result 43, and the
negative still printed the specific active-view diagnostic. The unchanged
three failures are
`test_span_array_runtime_mutation_slice_and_readonly_view_share_storage`,
`test_span_array_source_survives_gc_compaction_while_view_is_live`, and
`test_span_constant_slice_index_elides_only_proven_bounds_checks`.

The adjacent `zr_vm_reference_loan_nll_test` binary built and passed **15/15**.
A fresh `zr_vm_buffer_pool_ffi_test` binary was built from the then-stable
shared test source. GDB control-flow isolation of the older pinned Span+GC
case reported **3/3** including descriptor setup, and the positive executed
to integer 43. Isolation of PoolLease return/reuse, live-view close rejection,
and GC stress reported **5/5** including descriptor setup. The default FFI
suite is not claimed green; its separate `using(existingLocal)` duplicate
marker failure remains open.

## Reproduction

All build outputs and logs were kept under the existing
`D:\tmp\zr_vm\ssa-optional-member-gcc` cache. The relevant commands were:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-optional-member-gcc \
  --target zr_vm_span_core_test -j 4
/mnt/d/tmp/zr_vm/ssa-optional-member-gcc/bin/zr_vm_span_core_test
cmake --build /mnt/d/tmp/zr_vm/ssa-optional-member-gcc \
  --target zr_vm_reference_loan_nll_test -j 4
/mnt/d/tmp/zr_vm/ssa-optional-member-gcc/bin/zr_vm_reference_loan_nll_test
cmake --build /mnt/d/tmp/zr_vm/ssa-optional-member-gcc \
  --target zr_vm_buffer_pool_ffi_test -j 4
```

The FFI binary has no selector for these older positives, so the recorded GDB
runs jumped over unrelated default cases and stopped after the selected test
block. They are focused evidence for the named cases, not a full-suite pass.
