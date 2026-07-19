# 02-M1 语法与 canonical contract 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M1 语法与 canonical contract`。

## 状态与产出记录

- 完成时间：2026-07-19 22:37 +08:00
- 状态：已完成
- 完成项目：
  - lexer 将 `fn/ref` 识别为保留 token，并将函数类型 `->` 与匿名表达式体 `=>` 分成
    独立 token；命名/匿名返回类型继续使用 `:`。
  - 顶层函数、类方法、结构体方法、接口方法签名均接受规范 `fn` 入口；匿名 block 与
    expression body、嵌套右结合 function type 全部落入明确 AST。
  - AST 保存 fn/colon/arrow/body delimiter、七种参数 source form、调用点 ref/out marker
    及精确 source range；非法组合在实际 token 上产生诊断并完成 script recovery。
  - 未扩充旧 `EZrParameterPassingMode`；新增 `SyntaxParameter_Normalize` 和
    `SyntaxCallable_Intern`，统一生成 canonical ref/function TypeId 与完整 callable contract。
  - 命名函数和 function type 在参数/返回契约相同时驻留为相同 TypeId；delimiter 不进入
    类型身份或后续 semantic branching。
  - 旧 `%func(...)=>...` 历史兼容仅限旧 parser 入口；规范
    `fn(...) -> ...` 保持严格，不污染 canonical contract。
  - 将 call argument、anonymous fn 和 reference modifier 解析拆为独立模块；专项、parser、
    named arguments、type inference、semantic query、canonical graph/consumer 回归通过。
  - MSVC、GCC 11.4 与 Clang 14 各通过 7 个套件、249 项；两套 WSL 均完成 535-step
    目标构建，19 个 M1 文件与 Git index 逐文件一致。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-02-m1-reference-syntax-contract.md`
  - `docs/parser-and-semantics/reference-syntax-contract.md`
  - GCC 快照：`/home/hejiahui/zr_vm-syntax-02-m1-staged-gcc-20260719-r1`
  - Clang 快照：`/home/hejiahui/zr_vm-syntax-02-m1-staged-clang-20260719-r1`
  - `M1_INDEX_MATCH files=19`、`GCC_M1_MATRIX_PASS`、`CLANG_M1_MATRIX_PASS`
- 里程碑提交：本记录随
  `feat(syntax): establish reference syntax contract` 一并提交。

## 边界与后继

- M1 只建立语法与 canonical contract，不在 runtime 增加 borrow fallback。
- 下一阶段 M2 以 call marker、Place 和 CFG 为输入，实现 `ref/out` Place 限制与 `out`
  definite assignment，覆盖字段、索引、解引用、分支、循环、异常边和跨调用传播。
