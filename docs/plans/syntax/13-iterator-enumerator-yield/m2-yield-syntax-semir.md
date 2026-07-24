# Syntax 13 M2 Yield Syntax And SemIR Record

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-25 01:01 +08:00
- 完成时间：2026-07-25 02:19 +08:00
- 完成项目：增加保留 `yield` token、单一 `YIELD_STATEMENT` AST、解析/释放/
  syntax-tree writer 支持，且明确拒绝 `iterator fn`。
- 完成项目：仅通过 canonical `zr.iteration.Iterator<T>` TypeId 及其 element
  TypeId 验证 yield；拒绝 `Iterable<T>`、不兼容 payload、top-level、accessor
  和 value-return，嵌套 callable 不会重分类外层函数。
- 完成项目：新增 `YIELD_VALUE`、`YIELD_SUSPEND`、`YIELD_RESUME`、
  `ITERATOR_COMPLETE` pre-SemIR facts；suspend/resume 保持同一 ValueId，现有
  LoanId liveness 在 suspension edge 保持 active borrow。
- 完成项目：yield 函数不生成可执行 iterator bytecode 或运行时 frame；M2 未触及
  async iteration、artifact ABI、LSP 或 legacy generator migration。
- 验收：独立 `.codex/build-s13m2-gcc`、`.codex/build-s13m2-clang` 和
  `.codex/build-s13m2-msvc` 的 syntax/semantic/SemIR Unity targets 分别为
  4/4、6/6、3/3，均以真实 exit 0 完成。详见
  `tests/acceptance/2026-07-25-syntax-13-m2-yield-syntax-semir.md`。
