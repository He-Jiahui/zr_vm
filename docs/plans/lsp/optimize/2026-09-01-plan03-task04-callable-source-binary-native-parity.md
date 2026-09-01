# Plan 03 Task 4.29: Callable Source, Binary, And Native Parity

## Scope

Close the Task 4 callable-contract boundary for source, `.zro`, and native descriptor external
callables. The parser owns the identity and display facts; LSP consumers use `CallAt` and
`FormatCall` without reconstructing a target from a member name, local name, display text, or AST.

## Implementation

- External callable alias registration now requires a complete resolved external contract: owner
  identity, callable target kind, metadata token, signature token, signature hash, canonical TypeId,
  and signature display.
- A local alias call remains a free call. Its reference is unresolved with an invalid SymbolId,
  no resolved target, invalid receiver TypeId, and an empty declaration range.
- Native receiver calls publish the exact resolved receiver TypeId and reuse the canonical callable
  display. Free and constructor calls keep receiver TypeId invalid.
- External native receiver hover declines the generic receiver projection when the fact is marked
  `hasExternalTarget`, allowing the structured metadata provider to own the result.
- No member-name, local-name, display-text, or request-time AST fallback was added.

## Verification

The RED sequence was evidence-driven:

- the new parser `CallAt` characterization failed before the external alias gate was corrected;
- the fresh GCC/Clang project runners then isolated four callable-value/receiver failures;
- the receiver failures remained until the external-target hover routing was corrected.

The final focused evidence used real process exit codes and markers:

- GCC parser semantic-query symbols: `24/24`;
- GCC and Clang 12-target focused matrices: canonical consumers `21/21`, semantic facts `17/17`,
  semantic query `30/30`, call queries `31/31`, public contract `6/6`, diagnostics `13/13`,
  relations `29/29`, symbols `24/24`, and type inference `124/124`;
- GCC and Clang semantic-query parity, source-contract, and interface tests: real exit 0;
- both fresh project runners: all four new callable-value/receiver cases passed.

The same project runners still report 14 unrelated historical `Fail -` markers. MSVC, the complete
16-target matrix, and the three stdio/CLI smoke suites were not rerun on this fresh snapshot; those
remain open under Task 8 and are not claimed by this record.

## 状态与产出记录

- 完成时间：2026-09-01 19:20 +08:00。
- 状态：已完成 Task 4.29 窄片；Task 4 首项与 callable-contract parity 已关闭，Task 8 及全局计划仍进行中。
- 完成项目：parser external callable contract gate、local alias unresolved identity contract、
  resolved receiver TypeId projection、统一 `CallAt`/`FormatCall` 消费、native external receiver
  hover routing，以及 GCC/Clang focused verification。
