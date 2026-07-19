# 01-M5 规范 consumer 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md`
的 `M5 consumers`。

## 状态与产出记录

- 完成时间：2026-07-19 21:46 +08:00
- 状态：已完成
- 完成项目：
  - 建立 `SZrCanonicalConsumerProjection`，统一验证 ZRO TypeDef、TypeRef、TypeSpec、
    Signature、Contract 和 Layout section，并要求 root structural signature 逐字节一致。
  - VM module 与 AOT backend 使用同一个 core canonical open；合法 projection、无效
    signature hash、expected/actual diagnostic 的行为一致。
  - reflection 按 metadata token、debug 按 canonical TypeId、layout 按 exact type token
    查询；未知 identity 结构化失败，不存在 name guessing 参数或回退。
  - expression fact 稳定携带 TypeId；resolved generic call 从实例化参数、passing mode 和
    return type 构造 closed callable TypeId，保留 receiver/effect flags。
  - parser function-call range 覆盖完整括号；semantic call range 覆盖参数与空调用，保证
    LSP 在参数内部、参数间和 `()` 内查询同一 canonical fact。
  - `CanonicalTypeAt`、`CallAt` 与 `FormatCall` 成为共享 query；call query 拒绝返回类型
    冒充 callable，并优先 compiler signature display。
  - LSP hover 移除旧 type-name formatter 回退；signature help 优先 compiler canonical
    call fact；diagnostics 继续使用同一 semantic query/context，last-good recovery 回归通过。
  - 将 call semantic fact 逻辑拆到独立 194 行模块，使通用事实文件保持在千行阈值以下。
  - staged production diff 审计未新增 concrete built-in type-name dispatch，
    `git diff --cached --check` 通过。
  - MSVC 完成 18-target 矩阵，包含 273 个可计数用例及完整 local-query/LSP-interface
    套件；GCC 11.4 与 Clang 14 各完成 648-step build 和 18/18 target matrix。
  - 两个 WSL 快照中 27 个 M5 实现、测试与 CMake 文件与 Git index 逐文件一致；3 份收口
    Markdown 在验证后生成，不属于构建输入；快照仅叠加当前 HEAD 构建所需、非 M5 的
    `profile.h/profile.c` 配套前置基线，边界已写入 acceptance。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-01-m5-canonical-consumers.md`
  - `docs/parser-and-semantics/canonical-consumer-projection.md`
  - GCC 快照：`/home/hejiahui/zr_vm-syntax-m5-staged-gcc-20260719-r1`
  - Clang 快照：`/home/hejiahui/zr_vm-syntax-m5-staged-clang-20260719-r1`
  - `M5_INDEX_MATCH files=27`、`GCC_M5_MATRIX_PASS`、`CLANG_M5_MATRIX_PASS`
- 里程碑提交：本记录随
  `feat(syntax): complete canonical consumer migration milestone` 一并提交。

## 边界与后继

- Canonical consumer API 不解释 legacy `SZrIo`，也不允许从 legacy 类型名回退；正式
  语法/产物切换由后继迁移计划统一完成，不能在本层形成长期双格式语义。
- plan 01 的 M1-M5 已全部完成；下一依赖阶段进入 plan 02
  `fn/ref/in/out/scoped/readonly` 与 borrow checker。
