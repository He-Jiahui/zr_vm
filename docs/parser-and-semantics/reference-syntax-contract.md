# Reference syntax and callable contract

本文说明 syntax plan 02 M1 建立的函数与引用参数语法边界。parser 负责保留源拼写与
精确位置，`syntax_contract` 负责把语法投影成 plan 01 建立的 canonical TypeId 和
`SZrCanonicalParameterContract`。borrow checker 后续阶段只消费该规范契约，不从分隔符或
旧 passing mode 猜测语义。

## Canonical grammar

| Construct | Canonical form | Delimiter |
|---|---|---|
| Named function | `fn name(parameters): Return { ... }` | `:` |
| Anonymous block | `fn(parameters): Return { ... }` | `:` + block |
| Anonymous expression | `fn(parameters): Return => expression` | `:` + `=>` |
| Function type | `fn(ParameterTypes) -> Return` | `->` |

函数类型返回值递归调用同一个 type parser，因此
`fn(int) -> fn(string) -> bool` 按右结合解析。`fn` 与 `ref` 是保留 token；`scoped` 和
`readonly` 只在参数契约位置按上下文识别。

参数在名称与冒号之后使用以下源形式：

- `value: T`
- `value: in T`
- `value: ref T`
- `value: ref readonly T`
- `value: scoped ref T`
- `value: scoped ref readonly T`
- `value: out T`

调用点使用 `ref expression` 或 `out expression`。每个调用参数保存
`SZrCallArgumentSyntax`，其中同时包含 marker 与 marker token 的 `SZrFileRange`。

## AST retention

`SZrParameter` 保留 `sourcePassingForm` 与 `passingFormLocation`。旧
`EZrParameterPassingMode` 字段仍作为编译器兼容投影存在，但没有增加枚举值；例如
`ref readonly` 与 `scoped ref` 的完整区别只存在于 source form 和 canonical contract。

函数声明保存 `fn` 与返回冒号位置；函数类型保存 `->` 位置；匿名函数保存返回冒号、
body delimiter、返回类型和 block/expression body 标记。表达式体在 parser 内生成单一
`return` block 供现有 compiler 使用，同时 `isExpressionBody` 保留原始语义。

非法分隔符和修饰符顺序在实际 token 上报告错误，例如函数声明使用 `->`、函数类型使用
`=>`、匿名表达式体使用 `->`、`readonly ref` 或缺少 `ref` 的 `scoped`。parser 会恢复并
继续生成 script AST，诊断范围不依赖后续类型推断。

## Canonical normalization

`ZrParser_SyntaxParameter_Normalize()` 接收已经驻留的 value TypeId，并生成完整参数契约：

| Source form | Passing | Ref access | Escape | Entry/exit | Call marker |
|---|---|---|---|---|---|
| value | value | none | function | initialized/unchanged | none |
| in | in | readonly | function | initialized/unchanged | none |
| ref | ref | writable | caller | initialized/unchanged | ref |
| ref readonly | ref readonly | readonly | caller | initialized/unchanged | ref |
| scoped ref | ref | writable | block | initialized/unchanged | ref |
| scoped ref readonly | ref readonly | readonly | block | initialized/unchanged | ref |
| out | out | writable | function | uninitialized/definitely initialized | out |

`ZrParser_SyntaxCallable_Intern()` 对参数契约和返回 TypeId 驻留 canonical function TypeId。
命名声明与函数类型只要契约相同，就得到同一 TypeId；分隔符不进入类型身份。

## Compatibility boundary

旧 bare/`func` 声明和 `%func` 类型在 plan 06 正式切换前继续可读。仅旧 `%func` 入口兼容
历史 `=>` 返回分隔符；规范 `fn` 函数类型严格要求 `->`。该兼容只存在于 parser，不进入
canonical contract，也不得成为 borrow checker 分支条件。

新增引用语义时应扩展 canonical contract、Place/CFG/loan 数据流与诊断，不得向
`EZrParameterPassingMode` 增加新值，也不得按 `:`, `->`, `=>` 重新推导语义。
