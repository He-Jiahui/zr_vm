---
plan_id: aot-07-codegen
record_id: 2026-08-01-constructor-bitmap-layout-verifier
status: completed
completed_at: 2026-08-01 03:28:59 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Constructor Bitmap Layout Verifier

## 状态与产出记录

- 完成时间：2026-08-01 03:28:59 +08:00
- 状态：A7.2D constructor bitmap layout verifier 子里程碑完成；A7.2、AOT 07、AOT 12 与 AOT 07~12
  总目标继续进行。
- 完成项目：core 发布 constructor receiver 初始化 bitmap 结构布局查询，从 canonical receiver TypeLayout
  `fieldCount` 计算 uint64 word count、对齐 tail offset 与 frame 容量，不再由 AOT 猜测 runtime 私有布局。
- 完成项目：公共查询 fail closed 校验 TypeLayout schema/hash、`cTypeId`、payload size/alignment，并在失败时
  重置 offset/count/pointer 输出；custom resolver 仅放宽 `cTypeId`，schema/hash 与 payload shape 始终严格。
- 完成项目：bitmap 必须属于 constructor 的 direct inline-struct slot 0 parameter；全部 direct、indirect alias
  与 borrowed alias 物理 storage envelope 必须结束于 bitmap tail 之前，under-alignment、缺失 tail、zero field
  与 duplicate flag 均拒绝。
- 完成项目：runtime unwind 对声明但无法解析的 bitmap 或多个 flagged row 在任何 Drop 前失败，不退化为对
  malformed receiver 执行完整 Drop；合法 custom resolver 继续按初始化 bit partial drop。
- 完成项目：ExecIR 在 code stripping 前调用 core 查询，不可达 malformed owner 不会被过滤掩盖，也不会
  留下 generated C；沿用既有 `frameLayoutManifest`，不新增 reachability schema。
- 计划映射：完成 AOT 07 A7.2D bitmap frame ABI verifier 与 AOT 12 pre-filter graph-input gate；field-init
  dataflow、receiver/return/destination ABI、per-field cleanup lowering 与完整 A7.2 保持开放。

## 代码与文档产出

- `function.h` 与 `function_frame_place.c` 提供公共结构查询并复用 runtime bitmap 指针解析；ExecIR 在复制
  frame sidecar 前校验 flagged owner。初始 API/ExecIR 集成被共享工作树的 `3d67352` 一并提交，本记录对应
  的专用提交补齐 review hardening、扩展测试、模块文档、计划链接与验收证据，不重写已有历史。
- `test_type_layout_inline_copy.c` 覆盖 public output reset、canonical schema/identity/shape、custom resolver
  shape drift、duplicate flags、runtime partial drop 与 direct/alias physical envelope。
- `test_aot_c_code_stripping.c` 覆盖不可达 owner 的 slot0/parameter、constructor/field/tail/alignment、direct、
  indirect 与 borrowed overlap 矩阵。
- 模块文档同步 `csharp-value-type-semir-aot.md` 与 `aot-function-reachability-manifest.md`；验收入口为
  `tests/acceptance/2026-08-01-aot-07-constructor-bitmap-layout-verifier.md`。

## 验证结果

- 初始 review-hardening RED：HEAD 实现接受 hash 损坏的 canonical receiver，WSL GCC core 为 37/38；AOT
  stripping 保持 34/34。首版 canonical 检查误施加到 custom `cTypeId`，现有 unwind 用例再次为 37/38。
- 独立复审发现 custom resolver 同时放宽 size/alignment 的 P2；hash-valid size drift 稳定复现 37/38。
  修复后 payload shape 始终严格，bitmap 声明解析失败在 Drop 前返回 false。
- 独立复审要求 duplicate flag 的直接 P3 覆盖；新增解析成功且物理 envelope 合法的第二 flagged row 后，
  三编译器 core 均保持 38/38。最终复审返回 `No findings`。
- WSL GCC 11.4 与 Clang 14.0、Windows MSVC 19.44.35228.0 均通过 core 38/0 与 AOT stripping 34/0；
  GCC 相邻 generic reference sharing 为 9/0。
- 三份代码/测试文件在主工作树、WSL 冻结树与 Windows 冻结树 SHA-256 完全一致；最终 hash 前缀分别为
  `127429be`、`098b7268`、`635b9047`。失败输出在三套 build tree 均不存在。
- `git diff --check` 通过；MSVC 仅保留冻结目录位于 `%TEMP%` 的既有 MSB8029 与项目基线 warning。
- 已向 AOT/Syntax traceability 会话发送顶层计划链接协调消息；未修改 `tests/CMakeLists.txt`、活跃
  syntax/LSP/debug/CI 路径或脏 `function.h` 工作树内容。

## 未完成边界

- 本切片不证明每个 field store 的 dataflow 完整性，不生成 exception CFG/per-field cleanup lowering，也不
  生成 receiver、return、aggregate destination、spill 或 address-taken slots。
- 不声明 C/LLVM frame ABI golden parity，不生成 GC/ref provenance/debug safepoint map，不完成 A7.2。
- 四 backend parity、release policy、binary size/behavior 总验收、AOT 07/12 与 AOT 07~12 总目标仍开放。
