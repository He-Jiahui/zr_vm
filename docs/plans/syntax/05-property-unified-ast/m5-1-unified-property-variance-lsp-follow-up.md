# Syntax 05 M5.1: Unified Property Variance LSP Follow-up

## 状态与产出记录

- 状态：`completed_with_known_unrelated_markers`
- 开始时间：2026-07-26 03:48 +08:00
- 完成时间：2026-07-26 04:21 +08:00
- 完成项目：
  - 将 interface variance 的完整 helper 单元从 2708 行
    `semantic_analyzer_typecheck.c` 拆分为 `semantic_analyzer_variance.c`，原文件
    仅保留窄内部 API 调用，避免继续堆叠不相关的 typecheck 责任。
  - 发现并替换已失效的 interface variance fixture：legacy `pub get/set`
    声明不再产生 semantic member，新的 fixture 使用 bodyless unified
    `property item: T { get; }` / `{ set; }`，并保留原先的六个
    `invalid_variance` 位置、类别和 message 断言。
  - LSP semantic analyzer 现对 `ZR_AST_PROPERTY_DECLARATION` 使用和 compiler
    相同的 canonical accessor-kind 规则：getter-only 是 output，setter/init-only
    是 input，mixed property 是 invariant。判定只读取 AST kind/type/accessor，
    不使用 property name、hidden accessor 或 source-text fallback。
- 验收：
  - 原始迁移后的 fixture 先在 GCC 定向回归中保持 RED，证明 LSP analyzer 缺少
    unified PropertyDecl 分支；最小生产修复后 GCC、Clang、MSVC 的
    `zr_vm_language_server_semantic_analyzer_test` 均真实 exit 0，目标 variance
    assertion 均为 PASS。
  - 固定 `9096792 + M6.4/M6.4a + M5.1 overlay` snapshot 上，三个 toolchain 的
    18-target LSP matrix 都是 18 个真实 exit 0；main stdio/CLI、position-encoding
    stdio、diagnostic-fix stdio 在每个平台也均 exit 0，共九项。
  - 每个平台仍有五个一致的 Unity assertion marker：Vector3 receiver completion、
    native receiver field hover、shadowed foreach、container matrix 和 native value
    constructor。它们不在本阶段 write set 中，不计为 M5.1 通过证据，也不会被
    fixture 迁移或兼容逻辑掩盖。
