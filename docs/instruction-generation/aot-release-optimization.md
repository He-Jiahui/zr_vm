---
related_code:
  - zr_vm_parser/include/zr_vm_parser/aot_generic_policy.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c
  - tests/parser/test_ssa_generics_lto_pgo.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/03-generics-lto-pgo.md
tests:
  - tests/parser/test_ssa_generics_lto_pgo.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/aot_generic_policy.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c
doc_type: module-reference
status: implemented
---

# AOT release optimization policy

The parser-owned release policy keeps generic sharing and release-link decisions
pointer-free and deterministic. A generic dictionary key includes signature,
layout, ownership, effects, and target ABI hashes. Dictionary sharing is only
allowed when every component matches; equal layout alone is insufficient.

`ZrParser_AotGenericPolicy_Decide` applies the release budget in a conservative
order. Recursive depth and cold profile cases remain dictionary-shared. Hot
specialization is selected only when the projected module code size remains
within the configured budget. A 64-bit intermediate sum prevents code-size
overflow from being mistaken for a valid specialization.

`ZrParser_AotReleasePolicy_MarkRoots` validates root indices and marks all
explicit release roots, including reflection, native callbacks, serialized
types, capability entries, and future patch use. An invalid root clears the
partial result and reports its index and value, so metadata stripping cannot
silently remove a legal dynamic entry.

`ZrParser_AotReleasePolicy_ValidateProfile` keeps development builds free of
LTO/PGO, requires toolchain support for LTO and ThinLTO, and compares target,
IR, and compiler fingerprints before enabling PGO. Stale profiles and
unsupported link modes fail with a specific diagnostic; no stale profile is
reported as enabled.

Validation command (WSL, standalone while the shared CMake target is pending):

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
  -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c \
  tests/parser/test_ssa_generics_lto_pgo.c -o /tmp/generic_test && /tmp/generic_test
```

The focused test covers incompatible ownership/layout keys, hot specialization
within budget, budget/depth fallback remarks, all future-use roots, stale PGO,
and unsupported ThinLTO. Shared CMake registration and the `ssa_generics_lto_pgo`
CTest entry remain the parent stage's integration responsibility.
