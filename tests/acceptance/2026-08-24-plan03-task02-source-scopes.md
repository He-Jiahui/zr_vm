# Acceptance: Plan 03 Task 2.2b Source Scope Facts

## Scope

Validate that compiler source analysis publishes canonical lexical scope facts
which `ZrParser_SemanticQuery_VisibleSymbols` can consume without a name-based,
AST, or language-server fallback.

## Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| GCC static | symbols, query, contract, compiler diagnostics | 6/6, 29/29, 3/3, 46/46; all exit 0 |
| MSVC static | symbols, query, contract, compiler diagnostics | 6/6, 29/29, 3/3, 46/46; all exit 0 |
| Clang 14 WSL static | source compilation | New scope builder and focused test source compile; test link blocked by existing C11 inline ABI references |

The source tests use a nested block that shadows `value` and a `for` initializer
that ends before the following declaration. The canonical query returns exactly
one `value` at the inner return, retains the enclosing `seed` parameter, and
does not expose the loop variable after the loop.

## 状态与产出记录

- 完成时间：2026-08-24 10:32:04 +08:00
- 状态：GCC/MSVC acceptance passed。Clang test execution 仍被既有 static
  link gate 阻断，未计入本子里程碑通过声明。
- 完成项目：source module/function/block scope publication、hoisted function
  和 resolved parameter/local candidates、focused regression test、Plan 03
  status record。
- 后续项目：发布其余 scope candidate classes 与 artifact/native parity，之后才
  启用生产 LSP visible-symbol consumer。
