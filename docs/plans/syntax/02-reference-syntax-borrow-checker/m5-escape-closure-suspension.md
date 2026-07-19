# 02-M5 Escape、closure 与 suspension 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M5 Escape、closure 与 suspension`。

## 状态与产出记录

- 完成时间：2026-07-20 07:20 +08:00
- 状态：已完成
- 完成项目：
  - Syntax AST 与 inferred type 独立保存 writable/readonly ref access；canonical adapter
    投影为 `ZR_CANONICAL_TYPE_REF`，`scoped` 只约束 region，不制造第二套 TypeId。
  - parser 完成 `ref T`、`ref readonly T`、`scoped ref T`、`scoped ref readonly T` 及
    nested/invalid 定向拒绝；`scoped.Array<T>` 等 qualified type 不被 contextual keyword
    误伤。
  - `SZrSemanticEscapeFact` 发布 stable fact id、source Region/Place、escape kind、source/
    target lattice 与精确 origin/escape ranges；unknown source 按无法证明安全保守拒绝。
  - compiler 两个入口在 task effects 与 executable publication 前运行 reference escape
    pre-pass；conditional ref 取最严格共同上界。
  - in/out/scoped ref、function-local ref 的 return、module/global/member/container store
    均在编译期拒绝，并提供 primary escape range 与 related origin range。
  - container/member store 只传播已知 ref provenance；普通 int/value field、array、object
    初始化与赋值不被误判为引用逃逸。
  - lambda 与局部命名 `fn` 共用 capture/escape 规则；escaping closure 不能提升 capture
    bound，可写 capture 的 mutable loan 持续到 closure binding 最后一次使用。
  - await 与 generator suspension 按 binding epoch 和 last-use 检查；owned observation 不携带
    ref provenance 进入 frame。
  - target `async fn` 不隐藏包装其返回 TypeRef；legacy `%async` 保持迁移兼容。target
    `native extern` 中普通函数要求 `fn`，legacy `%extern` 保持兼容；native ref 默认
    call-scoped，长期保存仍要求后继显式 handle/pin contract。
  - closure call result 不再继承 callee identity；`ZrParser_InferredType_Init` 与 Full/Copy
    全部初始化/保存 reference access，关闭 generic/type equality 栈残值回归。
  - 验证器按表达式与 statement/declaration traversal 拆为 943/422 行实现单元及 98 行
    internal header，未新增 VM/AOT runtime borrow table 或 fallback。
  - GCC 11.4、Clang 14.0、MSVC 19.44.35228 `/W4` 各通过同一九套 196/196 项；GCC
    额外通过 parser 75/75 与 compiler integration 127/127。
- 验收证据：
  - `tests/acceptance/2026-07-20-syntax-02-m5-reference-escape-closure-suspension.md`
  - `docs/parser-and-semantics/reference-escape-closure-suspension.md`
  - GCC build：`/home/hejiahui/zr_vm-syntax-02-m5-gcc`
  - Clang build：`/home/hejiahui/zr_vm-syntax-02-m5-clang`
  - MSVC build：`build-syntax-02-m5-msvc`
- 里程碑提交：实现、验收证据与本记录随
  `feat(syntax): enforce reference escape boundaries` 一并提交。

## 边界与后继

- M5 完成 caller/function/heap-static escape、ref return、closure 和 suspension 静态拒绝；
  ref struct/Span layout 与 GC storage capability 由 Syntax 03 消费同一 region/ref facts。
- `async fn` hot Task carrier、正式 `yield`/Iterator lowering 分别由 Syntax 12/13 完成；M5
  只定义引用不得跨 suspension 的静态边界，不引入隐藏 runtime borrow recovery。
- M6 负责 artifact roundtrip 与 LSP query consumer；resolved receiver-call target identity
  仍必须以 `SymbolId`/declaration range 进入 canonical semantic fact/query，消费者不得按
  member name 推断。
