# Syntax 02 M1 reference syntax contract acceptance

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M1 语法与 canonical contract`。

## Scope

- `fn` 命名函数、类/结构体方法、接口签名和匿名函数。
- `:`、`->`、`=>` 三类分隔符及精确范围。
- `in/ref/ref readonly/scoped ref/scoped ref readonly/out` 参数源契约。
- 调用点 `ref/out` marker 与范围。
- 语法到 canonical parameter/callable contract 的唯一投影。

## RED evidence

新增专项测试时，基线没有 `ZR_TK_FN`、`ZR_TK_REF`、独立 thin/fat arrow、
`sourcePassingForm`、调用 marker 或 `syntax_contract.h`，测试在源接口层不能编译。第一次
MSVC 命令还因未载入 Visual Studio SDK 而先停在 `setjmp.h/stdlib.h`；该环境问题不计作
功能 RED，随后所有 Windows 验证均通过 `VsDevCmd` 执行。

主 parser 回归首次发现 `%func(int)=>int` 被规范分隔符切换误拒绝。修正后兼容只限旧
`%func` 入口，新 `fn(int) => int` 仍按目标规则拒绝。

## GREEN implementation

- lexer 分开产生 `FN`、`REF`、`THIN_ARROW` 和 `FAT_ARROW`。
- named/anonymous/function-type AST 保存关键字、分隔符、body kind 和精确 range。
- 参数 AST 保存七种 source form；旧 passing mode 未增加枚举值。
- call AST 保存与参数对齐的 `SZrCallArgumentSyntax` marker/range。
- expression body 合成为兼容 return block，但保留 `isExpressionBody`。
- `SyntaxParameter_Normalize` 与 `SyntaxCallable_Intern` 生成 canonical ref/function TypeId；
  相同命名声明和函数类型驻留为同一 callable TypeId。
- 调用参数、匿名函数和参数修饰符分别拆到独立 parser 模块，避免继续膨胀既有大文件。

## MSVC verification

环境：MSVC 19.44.35228.0，Debug，`/W4`，通过 `VsDevCmd` 载入 SDK。

通过 7 个套件，共 249 项：reference syntax 7、parser 75、named arguments 10、
type inference 118、semantic query 16、canonical graph 18、canonical consumers 5。

## GCC and Clang verification

- GCC 11.4.0：535-step build，7/7 套件、249/249 项通过，`GCC_M1_MATRIX_PASS`。
- Clang 14.0.0：535-step build，7/7 套件、249/249 项通过，`CLANG_M1_MATRIX_PASS`。
- GCC 快照：`/home/hejiahui/zr_vm-syntax-02-m1-staged-gcc-20260719-r1`。
- Clang 快照：`/home/hejiahui/zr_vm-syntax-02-m1-staged-clang-20260719-r1`。
- 两套快照的 19 个 M1 文件与 Git index blob 全部一致：`M1_INDEX_MATCH files=19`。

`git checkout-index` 不导出 submodule 工作树，因此两个快照叠加了当前仓库已检出的 Unity、
xxHash、utf8proc、cJSON、tinydir、libffi 和 libuv。纯 index GCC build 还复现了当前 HEAD 的
既有前置缺口：`value.h` 已引用 `ZR_PROFILE_HELPER_VALUE_CONSTRUCT`，而配套 profile 枚举
尚未提交；两个快照仅叠加工作树中的 `profile.h/profile.c` 后继续。上述依赖叠加不修改
19 个 M1 文件，index 校验在叠加后执行并通过。

## Promotion gate

- named/anonymous/function type 合法分隔符与右结合返回：PASS。
- 非法分隔符、非法 modifier order 与 parser recovery：PASS。
- 七种参数契约和 call marker/range：PASS。
- 相同 callable contract 获得相同 canonical TypeId：PASS。
- 未增加 legacy passing mode，canonical 层不按 delimiter 分支：PASS。
- `git diff --cached --check`：PASS。

结论：M1 promotion gate 为 GO，Critical 0，Important 0。
