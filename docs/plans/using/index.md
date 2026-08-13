# ZR Resource/Using 重设计计划索引

> 状态：按目标语法重写。`using`仅承担Close/Dispose作用域；最终表层grammar仍是`surfacePending`。
>
> 上游权威：[syntax 02 borrow](../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md)、[syntax 04 ownership](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md)。

## 职责拆分

| 需求 | 目标机制 |
|---|---|
| 构造GC class | `new T(...)` |
| 构造resource owner | `own T(...)` -> `Unique<T>` |
| 值构造 | `init T(...)` |
| lexical deterministic close | `using`，仅Close/Dispose protocol |
| owner提前释放 | `drop(owner)` |
| union解构 | `if let` / `switch` |
| 动态插件/模块 | `loadPlugin` / `loadModule` result union |

## 文档顺序

1. [当前状态](./00-current-state.md)
2. [ownership类型](./01-ownership-as-generics.md)
3. [using scope与plugin边界](./02-using-scopes-and-plugin-guards.md)
4. [metadata/token](./03-metadata-and-token-model.md)
5. [union/pattern](./04-union-types.md)
6. [迁移](./05-migration-and-phasing.md)
7. [语法与静态检查](./06-syntax-and-semantic-checks.md)
8. [实施路线](./07-implementation-blueprint.md)

## Syntax Contract 投影

| Syntax设计 | 本目录责任 | 主计划 |
|---|---|---|
| 01 TypeRef/Place/CFG/artifact | owner/ref canonical type、availability、loan、cleanup facts | 01、03、06 |
| 02 ref/in/out/scoped/readonly | borrow冲突、out初始化、readonly receiver与逃逸 | 06 |
| 03 struct/ref struct/Span/layout | value/ref-like storage边界与Drop分类 | 01、03、06 |
| 04 resource/Unique/Shared/Weak/Drop | 确定性生命周期与GC bridge | 01、02、03 |
| 05 property | accessor内borrow与ref-return lifetime，不生成field | 06 |
| 06 `%xxx` migration | 按AST role拆分using/owner/pattern/plugin | 05 |
| 07 reference fixture | normal/abrupt cleanup与四backend验收 | 07 |
| 08 reflection | owner/resource metadata可查询但不可绕过构造 | 03 |
| 09 pooling | PoolHandle/PoolRef属于`zr.pooling`，不扩张using语义 | 01、06 |
| 10 module/package/native | plugin/module loader使用ModuleIdentity与result union | 02、05、07 |

`UsingStatementSyntax`未冻结前，本目录只定义Close/Dispose semantic contract与cleanup CFG，不允许自行决定表层拼写。

## 子里程碑必填证据

每个实现项必须列出输入Type/Place/CFG facts、生成的Drop/Close/loader/pattern contract、正常与异常退出矩阵、VM/AOT parity、LSP diagnostic以及旧语法迁移结果。仅验证正常scope exit不能宣称using/resource完成。

## 共同规则

- AST不以`isUsing`或ownership字符串混合构造语义。
- borrow/move/drop/definite assignment由Place/CFG facts完成。
- resource Drop、using Close与GC finalization是三个不同contract。
- body退出统一进入cleanup CFG，覆盖return/throw/break/continue。
- 完成证据写入对应`plan-id/detail.md`，不在正文追加执行日志。
