# Using 06：语法与静态检查矩阵

## Parser

- `resource class`、`own`、Unique/Shared/Weak TypeRef、`drop`、`if let/switch`分别形成独立AST。
- `using`最终grammar冻结前保持feature-gated `surfacePending`，不得复用旧`%using`节点冒充目标实现。
- simple declarations/statements必须`;`；newline不终止。
- `init/new/own/call/createInstance`构造类别不得混合。

## Type/borrow/dataflow

- `own`目标必须是constructible resource class。
- Unique不可copy，move后任何Place projection使用失败。
- active ref阻止move/drop；mutable loan与其他读写loan按Place overlap冲突。
- out必须在所有正常返回路径initialized。
- ref/ref struct不能逃逸到heap、capture、async suspension或更长region。
- readonly receiver不能调用writable member/property setter/ref writable getter。
- using binding必须满足Close capability；cleanup不能延长borrow超过source lifetime。
- `wake(weak)`保持runtime checked；成功分支得到新的strong owner。

## Diagnostics

每个错误提供primary range、origin/move/loan/decl range、stable code和可行quick fix。LSP只查询semantic facts，不自己用token扫描判断ownership。

## Runtime checks保留

`wake(weak)`、native/loader failure、dynamic cast、bounds和external lifetime无法静态证明，必须显式保留。静态borrow通过不等于外部native pointer永远有效。

## 语义查询与诊断验收表

交付物包括Canonical semantic facts、versioned query结果、diagnostic registry entries、related ranges和带applicability的fix records。

| 类别 | 必需facts | 主要negative | 关联范围 |
|---|---|---|---|
| owner move/drop | Place availability、last use、DropKind | use-after-move、double drop、active borrow | declaration/move/use/drop |
| borrow/readonly | loan set、region、receiver effect | overlap、escape、readonly write | origin/conflict/destination |
| out/init | CFG definite assignment | partial return、throw path未初始化 | parameter/path exit |
| resource construct | type category/constructor contract | `new resource`、`own class`、reflection bypass | type/construct keyword |
| Close scope | protocol token、cleanup region | missing Close、duplicate cleanup、suspend | binding/exit/close member |
| pattern | variant set、narrowing、payload Place | non-exhaustive、unreachable、payload remmove | scrutinee/arm/variant |
| plugin/module | ModuleSpecifier/Identity、result union | unknown/export/capability/load failure | literal/manifest entry |

parser/compiler query测试以`tests/parser/test_semantic_query.c`、`test_dataflow_engine.c`为leaf，LSP投影以`tests/language_server/test_ownership_diagnostics.c`和union diagnostics为准。每个diagnostic必须有stable code、primary、related ranges和fix applicability；message文本不能成为协议。

退出条件：所有目标规则由shared facts产生且VM/AOT前完成；LSP没有第二套ownership推断；保留的runtime check在IR中显式可见并有失败测试。
