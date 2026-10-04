---
related_code:
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/type_layout.c
plan_sources:
  - docs/plans/ssa/05-data-layout/01-objects-layout-maps.md
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/core/test_type_layout_metadata_contracts.c
doc_type: acceptance-record
status: pendingRootFormal
---

# TypeLayout explicit offset tables match flagged fields

An explicit GC table could point at an in-bounds non-GC value slot while the
real GC_VALUE field was omitted. The builder hashed that supplied table, and
the validator checked count, range and hash without checking table-to-field
correspondence. The real non-union public GC visitor consequently visited the
wrong slot. Ownership and ref tables had the same validation gap.

Validation now compares each explicit table with the matching flagged field
offsets as a multiset. Table permutations remain valid. Union variants may
share an offset, so repeated offsets are accepted when their multiplicity
matches the fields. The null-table field-derived fallback is preserved. This
uses the existing shared validator and requires no allocations, temporary
arrays or byte-size multiplication. UInt32 counters cannot exceed their
respective UInt32 map/field counts. Worst-case work is quadratic; no existing
field-count limit was found and this slice does not invent one. A general
untrusted-metadata work budget remains separate work.

## Formal test source

The existing metadata-contract suite gains nine cases: in-bounds wrong-kind
offsets for GC/ownership/ref, duplicate-and-missing offsets for all three
tables, valid permutations and null fallback, legal union overlap with
active-tag scanning, and wrong union overlap multiplicity.

The wrong GC map case calls the real public
`ZrCore_TypeLayout_VisitGcValuesWithRegistry` only if the malformed descriptor
passes validation. A callback records offsets in aligned SZrTypeValue array
storage. It does not dereference managed children, invoke GC or free objects.
The RED callback recorded offset 40 instead of the flagged slot at zero on
this MSVC ABI. Valid explicit and null-table visitors visit the two expected
slots; the valid overlapping union visits only its active field.

## Private native RED and GREEN

Evidence directory:
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/review/data-layout/private/`.
The repository was not written, staged or committed for these runs.

The native driver read actual compile defines, includes and linker inputs from
the existing matrix/msvc/build.ninja without invoking Ninja. It copied only
the completed core, Unity, xxHash and utf8proc archives plus four built harness
objects from the checked-multiply target. Those harness defines match the
metadata-contract target. inputs.json records each source/copy SHA256 before
and after copying; the RED/GREEN receipts recheck these eight inputs after
execution and confirm the three audited repository source files are unchanged.
The private test source is the full existing suite plus the nine cases.

MSVC 19.44.35228.0 used /MDd /Od /RTC1 /W4 /std:c11 /utf-8 with the existing
platform and Unity defines. All command execution, compiler products and logs
used D-only working directories through control/run_native.py. No Linux
provider archive or unfinished build result was used.

- RED test compilation and static link succeeded without warnings. The
  baseline completed archive reported 18 tests, seven expected failures and
  zero ignored; exit code 7. The existing nine cases and valid permutation,
  null-fallback and union-overlap controls passed. red-run.log contains the
  actual wrong-slot visitor observation and all seven failed assertions.
- After RED, the private type_layout.c was patched and compiled as a standalone
  object placed before the unchanged core archive in the static link. The
  linker selected the private implementation without duplicate-symbol errors.
  GREEN compilation/link succeeded without warnings; green-run.log reports
  18 tests, zero failures, zero ignored and exit code 0.

The first compiler-launch attempt could not resolve bare cl from the parent
Python environment. Importing the already-cached VsDevCmd environment before
calling the unchanged root native runner fixed tool lookup. That launch
failure is not counted as RED.

## Integration boundary

These are private native results, not registered CTest results or final
acceptance. Root integration must record current registered
type_layout_metadata_contracts and adjacent TypeLayout/inline-array/pool gates
after provider build ownership is available. Allocation failure and
cancellation are not new execution branches because the comparison allocates
nothing and exposes no cancellation input. Existing caller-owned table
lifetime requirements remain in force. This does not close the whole SSA
05.01/05.02 milestone or repair separate nested-GC/drop/alignment contracts.

Linux, sanitizer and NDEBUG executions have not been performed for this slice.
The executed MSVC Debug tests use unconditional Unity TEST_ASSERT checks and
do not depend on standard C assert or NDEBUG.
