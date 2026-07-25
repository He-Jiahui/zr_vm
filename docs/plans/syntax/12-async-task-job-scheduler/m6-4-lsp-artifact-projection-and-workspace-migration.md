# Syntax 12 M6.4: LSP Artifact Projection and Workspace Migration

## 状态与产出记录

- 状态：`completed_with_known_baseline_failures`
- 开始时间：2026-07-26 00:30 +08:00
- 完成时间：2026-07-26 03:16 +08:00
- 完成项目：
  - 新增导出的
    `ZrLanguageServer_LspSchedulerContract_ResolveArtifact`。它只消费
    `SZrFunctionSchedulerSourceFact` 和真实 `.zro` bytes，经
    `ZrCore_CanonicalConsumer` 解析 canonical scheduler receiver TypeId、
    serialized TypeRef、Scheduler/Task/Job token、exact schedule signature、
    ABI、完整 policy/requirements、owner layout/module identity、transport hash
    与 scheduler contract hash。
  - helper 以 source fact 构造 expected artifact public identity，先验证
    TypeId，再以 artifact header 的 TypeRef token 解析 scheduler contract；
    provider TypeDef 只保留 compiler provenance，不能替代 serialized TypeRef。
  - owner module hash、policy 和 transport 篡改各自返回
    `ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH`、
    `ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH`、
    `ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH`。没有 member name、
    filename、raw AST、runtime value、display text 或 diagnostic message
    fallback。
  - LSP interface regression 编译真实
    `zr.thread.ThreadScheduler.schedule<int>(Job<int>)` source，写入 `.zro`
    artifact 后验证 source/binary parity、owner module identity 和三类
    structured rejection。Windows DLL export 也由该回归覆盖。
  - 新增 LSP artifact projection 模块文档，明确 workspace edit 仍只能使用
    已捕获并复验的 document snapshot，artifact projection 不生成 source range、
    version 或 edit。
- 验收：
  - 对 byte-exact `9096792 + M6.4` isolation snapshot，GCC 11.4、Clang 14.0、
    MSVC 17.14 均编译并通过新增
    `LSP Scheduler Contract Resolves Source Call From Canonical Artifact`，三个
    进程真实 exit 0。
  - 三个 toolchain 的 18-target LSP matrix 均为 16 个真实 exit 0 和两个
    非零 target：`zr_vm_language_server_local_semantic_hover_test` 与
    `zr_vm_language_server_reachability_semantic_query_test`。它们的
    reachability/semantic-query Unity failure marker 为既有基线，不在 M6.4
    的 implementation/test write set 内，未被作为通过证据。
  - 每个 toolchain 的 main stdio/CLI、position-encoding stdio 和
    diagnostic-fix stdio smoke 均真实 exit 0，共九项。MSVC 额外验证 DLL
    导出 `ZrLanguageServer_LspSchedulerContract_ResolveArtifact`。
  - 因两项既有 LSP baseline failure 尚未关闭，M6 promotion gate 保持
    `in_progress`；本记录不宣称 M6 已整体完成。
