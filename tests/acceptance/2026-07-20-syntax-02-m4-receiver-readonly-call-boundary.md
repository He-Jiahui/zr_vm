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
  `SZrTypeMemberInfo.receiverEffect`；compiled member metadata 保存 receiver effect，readonly
  requirement 不允许 writable implementation。
- `ZrParser_ReceiverCall_Analyze*` 对七种 dispatch 使用同一 canonical capability matrix；
  readonly view/ref/Shared 拒绝 writable，Unique 支持 shared/mutable auto-deref，Weak 拒绝。
- native descriptor 通过 `READONLY_RECEIVER` 显式发布 readonly receiver；旧
  `READONLY_INLINE_VALUE_CONTEXT` 保持兼容，metadata import 不再把所有实例方法强制设为
  writable。
- mutable compiler-generated receiver 在参数前 reserve、调用前 activate、调用后 end；
  readonly receiver 建立即时 shared loan。显式 ref 不进入 two-phase 路径。
- flow facts 区分 reserved/active；reservation 允许 shared read，拒绝第二次 reserve、write、
  move、drop，activation 后按 mutable loan 冲突。two-phase loan 必须有唯一且晚于 origin 的
  activation instruction。
- 未新增 VM/AOT runtime borrow fallback；source member 名称不参与 capability 或 owner
  特判。

## Verification

三工具链最终矩阵在里程碑记录中给出。本文件随最终验证结果一起提交。

## Promotion gate

- const/default receiver AST、top-level/static invalid syntax：PASS。
- readonly view const call 与 writable call/write rejection：PASS。
- class/struct/interface/override/generic/dynamic/native matrix：PASS。
- writable-to-readonly implementation variance 与反向 strengthening rejection：PASS。
- Unique/Shared/Weak owner auto-deref matrix：PASS。
- two-phase reserve/read/activate/write/end 与 second-reserve/direct-write negative：PASS。
- compiled/native receiver effect serialization：PASS。
- `scoped` module alias contextual-keyword regression：PASS。
- no runtime borrow table and no name-based dispatch policy：PASS。

结论待三工具链矩阵完成后写入里程碑记录。
