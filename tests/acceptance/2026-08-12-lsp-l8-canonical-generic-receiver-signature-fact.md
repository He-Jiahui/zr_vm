# LSP L8 Canonical Generic Receiver Signature Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-12 11:55 +08:00 | 已完成 | closed generic receiver signature help fails closed when its canonical call fact is unavailable. |

## Acceptance Contract

1. `Box<int>.shape(matrix)` formats its closed receiver and normalized const generic only through `CallAt/FormatCall`.
2. Removing `hasCallInfo` from that same expression fact makes signature help unavailable.
3. LSP must not recover the label from AST specialization, receiver/method names, or an open generic declaration.

## Evidence

- RED reproduced a closed generic label from `signature_prepare_ast_specialized_receiver_member` after the canonical fact was removed.
- GREEN extends the declaration-fact gate to source class, struct, and interface methods.
- GCC, Clang and MSVC each passed facts 13/13, local query 32/32, expression hover 9/9, local hover 12/12, interface 110/110, project 58/58 and stdio/CLI 2/2 with real process exit 0.
