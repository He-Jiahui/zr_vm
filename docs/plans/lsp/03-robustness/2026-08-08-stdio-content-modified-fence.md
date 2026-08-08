---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
implementation_files:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
plan_sources:
  - user: 2026-08-08 improve semantic inference and execute docs/plans/lsp stages
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-08-lsp-l6-stdio-content-modified-fence.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-stdio-content-modified-fence
status: completed
completed_at: 2026-08-08 17:31 +08:00
evidence_scope: stdio-input-generation-content-modified-fence
---

# Stdio Content-Modified Fence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:31 +08:00 | 已完成 | reader-observed document/workspace mutation generation、per-inbound FIFO snapshot、active request stale-response fence、`ContentModified` JSON-RPC error与rapid `didChange`/`didClose` smoke。 |

## 已实现契约

- stdin reader仍是唯一解析传输帧的线程，并在入队`didOpen`、`didChange`、`didClose`、`didSave`、project selection与workspace file mutation通知时，受锁递增input generation。它不读取或修改`SZrLspContext`、semantic cache或stdout。
- 每个inbound message记录它入队时的generation。主线程激活request时使用该FIFO snapshot，而不是读取当时的全局generation。因此reader即使已读到后续`didChange`，排在其前的request仍保持旧snapshot并被判定为stale。
- request handler在派发前、派发后和结果写出前检查活动request。cancel优先返回`-32800`；未取消但generation变化时只返回`-32801` / `Content modified`，不会发送success result。`workspace/diagnostic`在每个workspace项之间执行相同检查并释放部分result。
- 后续FIFO request在主线程先应用变更通知后才激活，并带有新generation，因此不会因前一条变更被误拒绝。不以URI、method name、source text、AST或semantic symbol重建snapshot identity。

## TDD 与验证

- RED：对提交`ea49934`前的MSVC stdio server运行新增smoke，以exit 1失败：`workspace/diagnostic must reject a response made stale by didChange`。这证明仅有cancellation registry仍会让旧diagnostic response成功返回。
- GREEN：MSVC重建`zr_vm_language_server_stdio`后，新增流程先完成v1 workspace diagnostic，再发送active `workspace/diagnostic`与紧随的`textDocument/didChange` v2。首request精确断言error `-32801`，随后诊断发布与新workspace request均精确观察v2。相同流程对`textDocument/didClose`重复执行，旧request也只能返回`-32801`，而close notification继续发布空diagnostics。
- MSVC：直接Node smoke、`ctest --test-dir .codex/build-e5-closure-msvc --output-on-failure -R '^language_server_stdio_smoke$'`均为1/1、真实exit 0；同一stdio smoke连续10次为10/10真实exit 0；`zr_vm_language_server_lsp_interface_test.exe`真实exit 0。
- POSIX：WSL GCC 11.4和Clang 14以C11 warning/include设置编译`stdio_transport.c`与cJSON，并运行最小stdin request/didChange harness。两套harness均真实exit 0，覆盖“后续通知已被reader排队、前request才激活”的原始竞态。

## 未完成边界

- 本记录只关闭L6的reader-observed content-modified fence。它不替代immutable semantic snapshot、rapid didChange/cancel/didClose完整压力矩阵、100-file workspace、p95/p99延迟或峰值内存报告；L6整体仍为进行中。
- 本次Linux证据是transport harness，不把受宿主前台120秒限制而未完成的完整Linux CMake LSP target记作通过。完整三工具链LSP矩阵仍属于后续L6门禁。
