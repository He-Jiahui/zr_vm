# Syntax 02 M4 receiver readonly and call boundary acceptance

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M4 Receiver/read-only 与 call boundary`。

## Scope

- `const fn` 与 ordinary instance `fn` receiver effect。
- `readonly T` capability view 与 writable member rejection。
- class/struct/interface/override/generic/dynamic/native dispatch matrix。
- Unique/Shared/Weak canonical owner auto-deref。
- compiler-generated mutable receiver two-phase reserve/activate/end。
- interface/import/native receiver effect serialization。

## RED evidence

专项测试最初无法编译，因为 AST、inferred type、member info 和 compiled member artifact 均无
receiver effect，Semantic IR 也没有 reservation/activation phase。实现前 readonly handle
可以调用 writable member，`const fn` 内也能写 receiver field。

首版 compiler dispatch classification 解引用了仍在构建中的 prototype member 指针，source
pipeline 用例发生崩溃。classification 改为只消费稳定 metadata token、AST kind、override 和
generic flags 后恢复。随后 compiler integration 暴露 `scoped.Array<T>` 被误识别为
`scoped ref` contract；lookahead 收紧到后继 token 确为 `ref` 后回归转绿。

## GREEN implementation

- class/struct/interface method AST 保存 default/const receiver modifier；拒绝 top-level/static
  `const fn`，contextual `readonly T` 进入 inferred/canonical type identity 和格式化路径。
- source、interface、override 和 imported artifact 统一使用
  `SZrTypeMemberInfo.receiverEffect`；packed v34 compiled member 保持 31 个 `TZrUInt32`，
  callable effect 通过既有 `isConst` bit 投影并按 callable member kind 恢复，field constness
  不会被误当作 receiver effect。readonly requirement 不允许 writable implementation。
- `ZrParser_ReceiverCall_Analyze*` 对七种 dispatch 使用同一 canonical capability matrix；
  readonly view/ref/Shared 拒绝 writable，Unique 支持 shared/mutable auto-deref，Weak 拒绝。
- native descriptor 只通过 `READONLY_RECEIVER` 显式发布语义 readonly receiver；
  `READONLY_INLINE_VALUE_CONTEXT` 只保留 runtime/ABI 含义。真实 builtin IArrayLike、
  container Array/Map GET descriptor 已迁移，SET 保持 mutable。
- mutable compiler-generated receiver 在参数前 reserve、调用前 activate、调用后 end；
  readonly receiver 建立即时 shared loan。显式 ref 不进入 two-phase 路径。
- flow facts 区分 reserved/active；reservation 允许 shared read，拒绝第二次 reserve、write、
  move、drop，activation 后按 mutable loan 冲突。CFG forward dataflow 分别维护 must-active、
  may-active 与 available-reservation；activation 必须唯一且 reservation 在全部前驱可用，
  不依赖全局 instruction ID 的数值先后。
- compiler 在发布 executable 前对 pre-Semantic-IR 运行 structural + flow gate；receiver
  canonical Place 与 ABI staging slot 分离，local/field projection 共用 member identity，
  `holder.buffer` 的嵌套调用可以正确判定 alias。
- 未新增 VM/AOT runtime borrow fallback；source member 名称不参与 capability 或 owner
  特判。

## Review remediation

- 删除对 packed `SZrCompiledMemberInfo` 的无版本字段扩展，增加 v34 编译期布局断言。
- semantic flow 已进入真实 source compiler gate；合法 two-phase 正例不仅编译，还执行并
  返回预期结果。gate 按 compiler-generated receiver LoanId 提升冲突，覆盖 immediate
  shared 与 two-phase mutable receiver，而不是按 phase 猜测 loan 来源。
- two-phase receiver loan 绑定 source local/field Place，不再绑定 receiver staging 临时槽。
- activation 使用 CFG must/may facts；branch-only activation 与 loop backedge 均有负例。
- native metadata 不再由 inline flag 猜语义 effect；真实 descriptor 和 synthetic metadata
  boundary 均有断言。
- readonly outer receiver + writable nested argument 与 mutable outer/projected receiver 负例均
  在 source pipeline 阶段拒绝，禁止发布 executable。
- resolved receiver-call target identity 仍属于 M6 canonical fact/query 发布边界：消费者必须
  使用 `SymbolId`/declaration range，禁止按 member name 推断。

## Verification

- GCC 11.4：八套晋级测试 184/184；额外 compiler integration 127/127。
- Clang 14.0：同八套晋级测试 184/184。
- MSVC 19.44.35228 `/W4`：同八套晋级测试 184/184。
- 八套构成：receiver 19、loan/NLL 15、pre-Semantic-IR 6、Place/CFG 4、
  Place/out 5、syntax contract 7、type inference 118、named arguments 10。
- 最终独立只读复审：Critical 0 / Important 0。

## Promotion gate

- const/default receiver AST、top-level/static invalid syntax：PASS。
- readonly view const call 与 writable call/write rejection：PASS。
- class/struct/interface/override/generic/dynamic/native matrix：PASS。
- writable-to-readonly implementation variance 与反向 strengthening rejection：PASS。
- Unique/Shared/Weak owner auto-deref matrix：PASS。
- two-phase reserve/read/activate/write/end 与 second-reserve/direct-write negative：PASS。
- compiled/native receiver effect serialization：PASS。
- packed v34 reader/producer layout、callable-only import reconstruction boundary：PASS。
- branch/join/loop activation must/may dataflow：PASS。
- canonical projected receiver Place alias 与合法正例执行：PASS。
- immediate readonly receiver 与 nested mutable argument conflict：PASS。
- `scoped` module alias contextual-keyword regression：PASS。
- no runtime borrow table and no name-based dispatch policy：PASS。

结论：M4 晋级门全部通过，可以进入 M5 Escape、closure 与 suspension。
