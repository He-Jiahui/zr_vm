# Syntax 02 M2 reference Place and out flow acceptance

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M2 Place access 与 out definite assignment`。

## Scope

- ref/out 调用的精确 marker 与 Place 限制。
- local、field、index、deref projection 覆盖。
- out 入口未初始化、读取限制、整值/逐字段初始化。
- 分支、循环、normal return、throw、try/catch 与跨调用 out 传播。
- positional/named argument 均按目标参数 contract 匹配。

## RED evidence

新增专项测试首次在 MSVC `/W4` 下因缺少 `EZrParserPlaceExpressionKind` 和
`ZrParser_PlaceExpression_Classify()` 无法编译。补齐 Place API 后，旧 out 扫描仍把 throw
当 normal return、忽略字段/跨调用/try-catch，并始终丢弃循环体状态；对应数据流用例失败。

第一版 marker 校验只进入 type-inference 路径，完整 compile script 可绕过校验；专项用例
因此观察不到缺失 marker 错误。接入调用编译入口后用例转绿。随后新增乱序 named out
调用，暴露按源码下标匹配 parameter mode 的错误；按参数名重排 marker 后通过。

GCC 与 Clang 的 type-inference 首次并行执行分别在不同 binary-import 用例失败。两个进程
争用同一测试临时路径；相同二进制串行重跑均为 118/118，通过后不把该夹具并发冲突计为
产品失败。

稳定化回归先证明短路右支被错误视为必经路径、for init 状态被丢弃，以及 finally 将外逃
异常误当成 normal fallthrough。修正前专项套件为 5 tests/1 failure；拆分 normal 与
exceptional finally state，并补全 loop edge 后恢复为 5/5。

## GREEN implementation

- Place 公共分类器接受 local、field、index，拒绝 call chain/rvalue；既有 Place graph 覆盖
  dereference 和全部 projection overlap。
- type inference 与 bytecode call lowering 都要求 exact ref/out marker 和 writable Place；
  value/in 显式 marker 被拒绝。
- named argument 以声明/metadata 参数名映射 marker，不依赖源码顺序。
- out 状态按参数和 source struct 字段建槽，整值/字段写入分别更新状态。
- 未初始化读取在赋值 RHS、返回值、普通/ref 实参和 compound assignment 上被拒绝。
- if/ternary/short-circuit join、条件求值、for init/step、break/continue、常量真循环、
  normal return、throw、try/catch/finally 与 out call normal-return transfer 使用独立状态传播。
- finally 分别运行在正常与外逃异常状态上；正常完成的 finally 效果会进入外层 catch，
  外逃异常本身不会制造 normal fallthrough。
- 旧 460 行整参数扫描从 generic semantics 移除；新实现拆为 376 行状态模块、748 行 flow
  模块和 62 行 internal header。

## Verification

MSVC 19.44.35228.0 全新 Debug `/W4` 构建（`b-s02m2-msvc-r2`，534 steps）：
5 个套件、144/144 项通过：

- reference Place/out flow 5
- reference syntax 7
- Place/CFG graph 4
- type inference 118
- named arguments 10

GCC 11.4.0 与 Clang 14.0.0 各完成 519-step 初始目标构建；最终稳定化增量构建分别为
6/9 steps。两者均串行通过相同 5 个套件、144/144 项。

## Promotion gate

- local/field/index 调用 Place：PASS。
- dereference Place projection/overlap：PASS。
- exact ref/out marker，含 named argument：PASS。
- out entry read、整值和逐字段 exit：PASS。
- conditional/short-circuit join 与 zero/one-or-more iteration loop：PASS。
- for init/step、break/continue edge：PASS。
- normal/exception edge、try/catch/finally 与 throw：PASS。
- cross-call out normal-return responsibility transfer：PASS。
- 无 runtime borrow fallback：PASS。
- `git diff --cached --check`：提交前验证。

结论：M2 promotion gate 为 GO，Critical 0，Important 0。
