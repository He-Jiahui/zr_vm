# LSP L8 Canonical Direct-Call Signature Expression Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-12 04:40 +08:00 | 已完成 | source function declaration direct-call signature fails closed when its canonical call fact is unavailable. |
| 2026-08-12 11:42 +08:00 | 已完成 | source receiver-call coverage proves the same fail-closed behavior. |

## Acceptance Contract

1. `inspect(1)` resolves through canonical `CallAt/FormatCall` and displays `inspect(value: int): int` while its fact is present.
2. Removing `hasCallInfo` from that same expression fact makes signature help unavailable.
3. The implementation must not rebuild a signature from local overload resolution, callee text, or AST text.
4. `counter.read()` resolves through canonical `CallAt/FormatCall`; removing its call fact also makes signature help unavailable without local member recovery.
5. Callable value assignments remain outside this leaf until parser publishes equivalent call facts.

## Evidence

- RED: the missing fact still produced a signature through the local fallback.
- GREEN: `test_lsp_direct_call_signature_fails_closed_without_canonical_call_fact` passes in the interface suite.
- 04:40 matrix: GCC, Clang and MSVC each passed facts 13/13, local query 32/32, expression hover 9/9, local hover 12/12, interface 108/108, project 58/58 and stdio/CLI 2/2 with real process exit 0.
- 11:42 receiver follow-up: GCC, Clang and MSVC each rebuilt and directly passed interface 109/109 with real process exit 0.
