# 02-M2 Place access 与 out definite assignment 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M2 Place access 与 out definite assignment`。

## 状态与产出记录

- 完成时间：2026-07-19 23:52 +08:00
- 状态：已完成
- 完成项目：
  - 新增公共 Place expression 分类，将 local、field、index 接入统一 Place 限制，并沿用
    Place graph 的 dereference/projection overlap 语义；call chain 和 rvalue 被拒绝。
  - type inference 与 bytecode call lowering 均严格验证 value/in/ref/out marker，不允许
    省略 ref/out 后由 overload resolution 猜测。
  - positional 和 named argument 均映射到目标参数 contract；乱序 named out 调用不会按
    源码下标误配 passing form。
  - out 参数入口为 uninitialized，读取旧内容非法；标量和 source struct 字段使用独立
    状态槽，整值写入与字段逐项写入具有不同效果。
  - normal return/fallthrough 要求全部 out 槽 initialized；throw/exception edge 不承诺
    新值，返回 false 仍按 normal return 处理。
  - 完成 if/ternary/短路逻辑 join、条件求值后的零次迭代、for init/step、break/continue、
    常量真循环、try/catch/finally 和跨调用 out 正常返回责任转移。
  - finally 对正常路径与外逃异常路径分别传播，异常经 finally 继续外逃时不会被误判为
    normal fallthrough；外层 catch 可见 finally 已完成的初始化状态。
  - 移除 generic semantics 中旧的整参数布尔扫描；状态建模和 flow 传播拆分为独立模块，
    未增加 VM/AOT runtime borrow fallback。
  - MSVC 19.44 全新 Debug `/W4` 构建 534 steps；MSVC、GCC 11.4、Clang 14 各通过
    5 个套件、144/144 项。GCC/Clang 初始目标构建均为 519 steps，最终稳定化增量构建
    分别为 6/9 steps。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-02-m2-reference-place-out-flow.md`
  - `docs/parser-and-semantics/reference-place-out-flow.md`
  - GCC build：`/home/hejiahui/zr_vm-syntax-02-m2-gcc-worktree`
  - Clang build：`/home/hejiahui/zr_vm-syntax-02-m2-clang-worktree`
  - MSVC clean build：`b-s02m2-msvc-r2`
- 里程碑提交：初始实现与记录进入 `05a96a8`；短路、循环和 finally edge 的最终稳定化
  随 `fix(syntax): stabilize out-flow edge joins` 一并提交。

## 边界与后继

- M2 只收敛 Place access 与 out initialization，不创建 LoanId，也不实现 reborrow/NLL。
- 下一阶段 M3 基于 canonical ref contract、Place overlap 和 CFG facts，实现 shared/mutable
  loan creation、liveness、reborrow 与 last-use 收缩。
