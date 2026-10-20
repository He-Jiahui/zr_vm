---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_registry.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c
  - tests/parser/test_semantic_query.c
  - docs/cli-and-tooling/optimization-remarks.md
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
plan_sources:
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/parser/test_semantic_query.c
doc_type: module-detail
---

# Diagnostic Message Catalog

## Registry and Message Ownership

The registry owns stable descriptor IDs, reason codes, severity, category, help
URI and two message keys per descriptor: `diagnostic.<code>.title` and
`diagnostic.<code>.message`. The message catalog supplies text for those keys.
Both arrays are static and retain ownership; enumeration and lookup return
borrowed const pointers. Unknown indices, codes, IDs and message keys return
NULL under the existing API contract.

`ZR_DIAGNOSTIC_MESSAGE_PAIR` creates the title and message entries together.
English is the base text. A missing Simplified Chinese translation falls back
to that same English text; lookup does not invent text for an unknown key.
No allocation, provider execution, cache policy or diagnostic identity changes
are involved in supplying an additional registered message pair.

## Optimization Reasons

The registry contains94 descriptors. Its existing IDs5001-5023 cover all23
non-NONE optimization reason names. These entries have informational severity
and style category: they describe pass evidence for success, missed and blocked
decisions rather than changing compiler error handling. Their reason names
match the core optimization enum and `ZrCore_OptimizationRemark_ReasonName`.

The original table contained71 pairs and142 keys, so the23 optimization
descriptors had neither a title nor a message. The repair appends their23 pairs
using the existing macro. Existing entries, order, keys and locale behavior
remain intact. The table now contains94 pairs and188 keys. Descriptor
registration, numeric IDs and severity/category are unchanged.

## Validation and Limits

The existing30-case semantic-query fixture checks an exact descriptor count,
stable IDs, bidirectional descriptor lookup, unique IDs/codes/message keys,
per-descriptor title/message presence and nonempty English text. It requires
exactly two keys for every descriptor, and checks English resolution and
Simplified Chinese fallback for every entry. Updating the old71 descriptor
expectation to94 preserves these independent coverage checks.

Focused native RED naturally exits1/build0/test2:28PASS/2FAIL/0ignored. Its two
failures are registry71-versus94 and messages188-versus142. Root whole-byte RAM
inversesa6cc26 and6b9b27 restore the complete prior message and test preimages;
an independent reviewer verifies both inverses and all23 added key mappings.
No filesystem source copy is created.

First GREEN native15104 naturally exits0/build0/test0, all30PASS/0FAIL/0ignored.
The candidate is frozen at messagesF35FF276 and testDAEB5FFC, while registry
26818FAB is unchanged. This closes the focused catalog failure. The broader M1
pipeline has other open failures and must be replayed on the current build;
this gate does not establish platform or whole-milestone acceptance.

See [foundation validation](../testing-and-validation/macro-foundation-validation.md)
and [optimization remarks](../cli-and-tooling/optimization-remarks.md).
