# SSA 01.02: dynamic CFG edge capacity preflight

## RED fixture and expected rejection

The real semantic CFG fixture supplies one valid edge from entry block 0 to
block 1, but sets its dynamic `outgoingEdges.capacity` to zero while leaving
the pointer, length and element width otherwise valid. The builder must
report `INVALID_RANGE` with source block 1 and leave a pre-existing output
function untouched. Before the fix, the MSVC standalone target failed with
`FAIL: builder accepted an outgoing edge count exceeding its capacity`.

This guards the nested edge array before destination validation and before
the two-pass predecessor builder reads its contents; tests for missing edge
storage, bad destination and excessive inline successors remain in the same
focused target.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 directly compiled the real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed fixture printed
  `ssa builder CFG PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 directly compiled the same fixture without sanitizers; five
  consecutive D:-backed executions passed. The Clang ASan/UBSan build
  compiled, but executions were intermittent (some exited 1 or 139 without
  a sanitizer report while others passed). Clang sanitizer stability and
  full-backend parity are **not** claimed.

## Remaining gates

This check does not prove that arbitrary non-NULL host memory is readable,
nor does it implement pruned phi insertion, semantic terminator validation,
exception-edge definition availability or four-backend parity.
