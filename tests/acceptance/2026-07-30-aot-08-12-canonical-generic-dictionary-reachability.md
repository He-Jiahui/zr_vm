# 2026-07-30 AOT 08/12 Canonical Generic Dictionary Reachability

## Scope

This sub-milestone closes the first owner-linked generic dictionary node slice across AOT 08 and AOT 12:

- a reference-generic dictionary instance is deduplicated by compiler-owned typed-local `typeId`, not display text;
- every retained function using that TypeId resolves to the same generated dictionary ID;
- dictionaries owned only by unreachable functions disappear with code stripping;
- a version 1 manifest publishes TypeId, stable owner function, `edge.generic_instance`, and predecessor;
- before/after/removed counts expose dictionary trimming independently from function counts;
- missing canonical identity, nonempty/null binding or frame-layout tables, or conflicting layout schema fails writer
  output, with structural checks running before ExecIR construction.

Type text remains existing candidate-discovery and debug-symbol input. Canonical shared-body definition identity,
constraint witnesses, and cross-module dictionary schema remain later AOT 08/11/12 work.

## RED Evidence

The first tests compiled against unchanged production code and ran six cases. Four existing cases passed; the new
canonical manifest/count case and missing-TypeId rejection failed. After the first identity implementation, a second
RED assertion proved the later retained owner still received `ZR_NULL` instead of reusing dictionary 1. The final
mapping resolves every owner through its canonical TypeId. Independent review then found that null frame-layout
metadata reached ExecIR before generic validation; the added negative exited abnormally until pre-ExecIR tree validation
was installed.

## Coverage Inventory

- `Box<RefA>` and `AliasBox<RefA>` with TypeId 41 form one dictionary despite different display names.
- An unreachable `Box<RefA>` with TypeId 42 remains a distinct pre-trim node despite equal display text.
- Counts report two dictionaries before filtering, one after filtering, and one removed.
- The retained manifest row reports TypeId 41, owner function 0, `edge.generic_instance`, and predecessor 0.
- Both retained function method descriptors point at `zr_aot_generic_dict_1`.
- Missing TypeId, nonempty/null binding metadata, nonempty/null frame-layout metadata, and conflicting layout IDs for
  one TypeId return false and leave no generated artifact.
- Existing lazy type-layout/SIZEOF resolution and source-generated shared-body compilation remain green.

## Validation

Effective source is commit `39af937` plus the exact four code/test overlays for this sub-milestone. Validation reused
the frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct generic sharing 9/0 and code stripping 26/0.
- WSL Clang: focused CTest 2/2; direct generic sharing 9/0 and code stripping 26/0.
- Windows MSVC: focused CTest 2/2; direct generic sharing 9/0 and code stripping 26/0.
- SHA-256 for all four code/test files matched the main worktree in both frozen trees.
- Generated C contained the 2-to-1 count delta, one TypeId 41 manifest/dictionary row, and two owner references to
  dictionary 1. All four malformed-output paths were absent.
- Independent review's initial null-frame-layout finding was fixed; re-review returned no findings.

The adjacent GCC `generic_call_typed` target built and its two runtime dictionary cases passed, while five source cases
failed during the active syntax cutover's rejection of legacy keywordless functions and `$` construction, before the
writer ran. The frozen frame/source contract suites also retained unrelated source-text skew (0/1 and 21/24) in
untouched frame-setup, scalar-local, and typed-arithmetic helpers. No parser, fixture, or unrelated helper change is
included in this sub-milestone.

The MSVC build retained only the existing MSB8029/MSB8064 warnings caused by locating an isolated build below
`%TEMP%`.

## Acceptance Decision

Accepted as the canonical generic dictionary reachability sub-milestone. Dictionary instance identity and owner-linked
trimming are now explicit and auditable; canonical shared-body identity, constraint witnesses, full AOT 08/12, and
AOT 07-12 remain active.
