# 2026-07-30 AOT 12 Native Callback Materialization Edge

## Scope

This sub-milestone classifies callable materialization from a reachable function as a native-callback dependency when
the destination slot has exact structured escape metadata:

- `GET_SUB_FUNCTION`, callable `GET_CONSTANT`, and `CREATE_CLOSURE` emit `edge.native_callback` for a matching
  `NATIVE_BINDING` row carrying `NATIVE_HANDLE`;
- materialization without that exact slot contract remains `edge.direct_call`;
- a nonempty escape-binding table with a null row pointer fails graph construction;
- the public writer removes its partial generated-C file after malformed metadata rejects stripping.

This is a reachable creation-site edge, not an externally registered callback descriptor root. Descriptor discovery,
cross-module callback identity, and callback ABI/lifetime/thread/exception contracts remain later AOT 07/11/12 work.

## RED Evidence

Unit and public-writer tests were added against unchanged production code. The first GCC focused build failed because
`ZR_AOT_REACHABILITY_REASON_NATIVE_CALLBACK` did not exist. The code-stripping test translation unit compiled and
linked in that build, isolating RED to the missing graph reason and collector behavior.

## Coverage Inventory

- A matching native escape row classifies `GET_SUB_FUNCTION` as `edge.native_callback` and preserves its child.
- Real function constants prove both `GET_CONSTANT` and `CREATE_CLOSURE` use the same reason.
- The existing ordinary `GET_SUB_FUNCTION` case remains `edge.direct_call`.
- A nonzero escape-binding count with a null table rejects graph construction.
- The public writer preserves functions 0/1, removes function 2, emits the callback predecessor chain, and reports
  one removed function.
- The malformed public-writer case returns false and leaves no generated C artifact.

## Validation

Effective source is commit `5af236c` plus the exact five code/test overlays for this sub-milestone. Validation reused
the frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct reachability 33/0 and code stripping 26/0.
- WSL Clang: focused CTest 2/2; direct reachability 33/0 and code stripping 26/0.
- Windows MSVC: focused CTest 2/2; direct reachability 33/0 and code stripping 26/0.
- SHA-256 for all five code/test files matched the main worktree in both frozen trees.
- Generated C retained functions 0/1, omitted function 2, and published
  `node[1] = reason=edge.native_callback predecessor=0`; the malformed negative left no artifact.
- Independent review returned no findings.

The adjacent `escape_pipeline` target built successfully but its CTest result was 0/1, with 2 direct-suite passes and
10 failures. Every failure occurred before reachability graph construction because the active syntax cutover rejects
legacy keywordless functions and `%import`; no parser or fixture change is included in this AOT sub-milestone.

The MSVC build retained only the existing MSB8029/MSB8064 warnings caused by locating an isolated build below
`%TEMP%`.

## Acceptance Decision

Accepted as the native-callback materialization-edge sub-milestone. Reachable callback creation now has a stable,
auditable reason chain and malformed escape metadata fails closed. Explicit callback descriptor roots, full AOT 12,
and AOT 07-12 remain active.
