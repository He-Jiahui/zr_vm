# Syntax 13 M2 Yield Syntax And SemIR Acceptance

## Status

Completed 2026-07-25 02:19 +08:00. Started 2026-07-25 01:01 +08:00.

## Required Contract

- `yield expression;` is one lexer keyword and one `YIELD_STATEMENT` AST node.
- Only a resolved canonical `zr.iteration.Iterator<T>` function carrier permits
  yield; `Iterable<T>` and source-name fallbacks do not qualify.
- A valid yield records `YIELD_VALUE`, `YIELD_SUSPEND`, and `YIELD_RESUME` with
  one ValueId; the function records one `ITERATOR_COMPLETE` fact.
- Yielding functions have no executable iterator bytecode or runtime frame in
  M2. `return;` completes, while `return expression;` is rejected.

## Evidence

Independent `Debug` Ninja build directories completed with real process exit 0:

| Toolchain | Build directory | `zr_vm_yield_syntax_test` | `zr_vm_yield_semantic_test` | `zr_vm_iterator_semantic_ir_test` |
| --- | --- | --- | --- | --- |
| GCC | `.codex/build-s13m2-gcc` | 4 tests, 0 failures | 6 tests, 0 failures | 3 tests, 0 failures |
| Clang 14 | `.codex/build-s13m2-clang` | 4 tests, 0 failures | 6 tests, 0 failures | 3 tests, 0 failures |
| MSVC 19.44 | `.codex/build-s13m2-msvc` | 4 tests, 0 failures | 6 tests, 0 failures | 3 tests, 0 failures |

The semantic target deliberately compiles invalid carrier, payload, return,
top-level, and accessor programs. Their expected compiler diagnostics are
printed during the test but each Unity binary exits 0 with no test failures.
The fresh Clang and MSVC dependency builds retain pre-existing warnings outside
the M2 paths; none are emitted from the M2 source or test files.
