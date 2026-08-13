# LSP L8 Canonical Closure Callable-Value Signature Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-13 13:46 +08:00 | 已完成 | Source lambda callable values now publish one canonical target identity, declaration range and callable signature fact for hover, definition and signature help. |

## Scope

- Preserve exact lambda AST declaration identity through compiler callable binding.
- Publish the lambda's resolved SymbolId, canonical callable TypeId and full declaration range through the existing reference/call query facts.
- Require LSP consumers to fail closed when the call fact is unavailable.

## Test Inventory

- Parser positive case: `add(20, 22)` resolves to the lambda SymbolId; `DeclarationOf`, query declaration range and lambda source range are identical; `FormatCall` is `add(left: int, right: int): int`.
- LSP positive case: hover uses the callee reference range, definition uses the lambda declaration range, and signature help equals the canonical formatted label.
- LSP negative case: clearing the exact call fact's `hasCallInfo` makes signature help unavailable, proving no lambda AST, variable-name or callee-text recovery.

## Evidence

The fixed code/test snapshot was `6d9a22e + 8-path overlay`, byte-exact `8/8`. GCC 11.4, Clang 14.0 and MSVC 17.14.38 static Debug runs each passed canonical consumers 19/19, semantic facts 13/13, semantic query 29/29, expression hover 9/9, local hover 12/12, interface 112/112, project 58/58 and compiler integration 127/127 with direct process exit 0.

GCC and Clang CTest stdio/CLI each passed 2/2. MSVC CLI passed; the same stdio process had two isolated warm p95 tail failures before three clean standalone reruns passed with hover p95 7.01/12.98/24.69ms and completion p95 22.46/47.47/14.45ms. No threshold, whitelist or production code was changed for that rerun.

## Acceptance Decision

Accepted for the source lambda callable-value contract. Binary/native/provider callable values and remaining L8 fallback convergence are still open.
