# Syntax 12 M6.4a: LSP Reachability Fact Spans

## 状态与产出记录

- 状态：`completed_with_known_baseline_markers`
- 开始时间：2026-07-26 03:18 +08:00
- 完成时间：2026-07-26 03:45 +08:00
- 完成项目：
  - 根因已定位为 `ZR_AST_VARIABLE_DECLARATION` 的 `location` 只覆盖
    `var` keyword，而 semantic query、hover 和 diagnostic projection 会查询
    pattern identifier 或 initializer 中的位置。事实已经生成，但 position
    lookup 因 range 过窄无法命中。
  - `semantic_record_reachability_fact` 现在仅通过 AST range 合并生成
    canonical fact range：declaration、pattern、可用 type name 与 initializer
    均参与。没有 source text、identifier name、display text 或 diagnostic
    message fallback。
  - `zr_vm_language_server_reachability_semantic_query_test` 与
    `zr_vm_language_server_local_semantic_hover_test` 在 GCC 11.4、Clang
    14.0、MSVC 17.14 上均恢复为真实 exit 0。
- 验收：
  - 固定 `9096792 + M6.4/M6.4a overlay` isolation snapshot 上，GCC、Clang、
    MSVC 的 18-target LSP matrix 均为 18 个真实 exit 0；本阶段修复的两个
    reachability target 在三套工具链中均包含在内。
  - 每个 toolchain 的 main stdio/CLI、position-encoding stdio 和
    diagnostic-fix stdio smoke 均真实 exit 0，共九项。
  - 每套矩阵仍保留相同六个 Unity assertion marker：receiver completion、
    native receiver hover、foreach shadowing、container matrix、native value
    constructor 与 interface variance。它们的测试进程退出码为 0，但不属于
    本阶段实现或通过证据；M6 总状态继续为 `in_progress`。
