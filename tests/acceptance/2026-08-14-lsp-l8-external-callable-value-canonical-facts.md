# LSP L8 External Callable-Value Canonical Facts Acceptance

## Scope

- Parser/type-environment storage for binary and descriptor-provider callable values.
- Canonical external call facts with no fabricated source identity.
- LSP hover/signature consumption and fact-removal fail-closed behavior.

## Baseline

- External callable-value aliases originally had no canonical `CallAt` expression/reference.
- The project test runner returns process exit zero even when Unity cases fail, so acceptance also counts exact `Fail -` markers.
- Duplicate expression publication initially kept a stale live call fact; support commit `27468de` made expression facts last-write-wins by AST node.

## Test Inventory

- Binary metadata callable value: type, display, unresolved identity, hover, signature, removal boundary.
- Descriptor plugin callable value: the same contract through provider metadata.
- Canonical consumer regression suite.
- Complete project feature runner, including existing native/provider/binary parity cases.
- Exact construct receiver fact removal and binary property `PropertyAt` support boundaries.
- GCC, Clang, and MSVC focused builds, direct execution, and stdio/CLI smoke.

## Tooling Evidence

| Toolchain | Query and consumer gates | LSP runners | stdio/CLI p50/p95/p99 ms and peak |
|---|---|---|---|
| GCC 11.4 WSL Debug shared | canonical `19/19`, facts `14/14`, query `29/29` | local hover, interface, project: real exit `0`, `Fail -` = `0` | hover `1.33/1.59/1.64`, completion `0.96/1.07/1.12`, signature `0.50/0.63/0.72`, diagnostics `0.56/0.99/1.95`, 100-file `2.78/4.05/76.21`, `33.08 MiB` |
| Clang 14 WSL Debug shared | canonical `19/19`, facts `14/14`, query `29/29` | local hover, interface, project: real exit `0`, `Fail -` = `0` | hover `2.37/10.07/11.66`, completion `1.27/2.09/5.86`, signature `0.63/1.47/2.77`, diagnostics `0.55/4.56/14.43`, 100-file `3.64/4.19/117.44`, `32.32 MiB` |
| MSVC 19.44 Debug static | canonical `19/19`, facts `14/14`, query `29/29` | local hover, interface, project: direct process exit `0`, `Fail -` = `0` | hover `2.65/2.98/3.11`, completion `2.73/3.52/3.64`, signature `0.87/1.29/1.32`, diagnostics `1.41/2.00/2.08`, 100-file `13.90/19.71/126.28`, `39.13 MiB` |

The LSP requests are `textDocument/hover`, `textDocument/signatureHelp`, completion, diagnostics, and the stdio/CLI protocol smoke. Project fixtures are opened at document version `1`; the negative boundary removes the canonical call payload in memory and verifies that `CallAt`, formatting, hover, and signature help remain unavailable without rebuilding from local text or metadata.

## Results

The parser publishes a local alias only from an exact imported-member contract. The call fact retains a callable `TypeId` and display while target identity is unavailable. The LSP consumes this fact directly for hover and signature help. Removing the call payload makes both protocol responses unavailable.

The current full compiler-integration runner still reports the unrelated ownership failure `Plugin Guard Module Share Member Is Rejected` (`Module.share()` unexpectedly compiled). That test exercises ownership policy outside this leaf's write set and is not counted as L8 evidence or silently waived; the owning milestone must repair it before a repository-wide compiler-integration baseline can be declared green.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-08-22 18:23 +08:00 | 已完成 | LSP 08 external callable-value canonical facts。 | 三工具链表中的 direct exit、Unity marker、stdio/CLI 和内存预算。 |

## Acceptance Decision

`completed`. This leaf is complete; L8 convergence remains in progress.
