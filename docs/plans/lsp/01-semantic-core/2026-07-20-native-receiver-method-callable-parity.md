---
plan_id: lsp-01-semantic-core
record_id: 2026-07-20-native-receiver-method-callable-parity
status: completed
completed_at: 2026-07-20 20:18 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: native-receiver-method-call-site-callable-parity
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_callable_contract.h
related_tests:
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/language_server/descriptor_plugin_fixture_float.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_receiver_callable_cases.h
  - tests/language_server/stdio_smoke.js
---

# Native Receiver Method Callable Parity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 20:18 +08:00 | 已完成 | native builtin与descriptor-plugin instance method call-site通过exact semantic query identity合并当前descriptor和parser canonical function TypeId；hover/signature help共享closed types、receiver effect、参数名/文档和provider generation；incomplete contract返回unavailable且禁止member-name/AST/raw-owner fallback；三工具链十六目标矩阵与三套stdio/CLI smoke完成且marker归零 |

## 已实现契约

- signature help从parsed call context取得精确callee range，再由`ZrLanguageServer_LspSemanticQuery_ResolveAtPosition`解析native provider、owner和`METHOD` descriptor。LSP不接收独立member name，也不按owner type字符串寻找方法。
- instance method必须同时由`ZrParser_SemanticQuery_CallAt`在同一callee range发布canonical function `TypeId`。descriptor只提供当前provider generation的method identity、参数名和文档；closed参数/返回TypeId、passing/escape与receiver effect全部来自canonical fact。
- hover与signature help共用`ZrLanguageServer_LspExternalCallableContract_FromResolvedMethod`和同一formatter。readonly/mutable分别显示`const fn`/`fn`；builtin `LinkedList<int>.addLast`固定为`fn addLast(value: int): LinkedNode<int>`。
- static method返回`NOT_EXTERNAL`并保留既有consumer。已识别instance method若descriptor type为unknown、canonical effect不受支持、method generic clause非空或structured字段缺失，则返回`UNAVAILABLE`，不得进入member-name、AST文本或raw `T` substitution fallback。
- bare method reference没有`CallAt` fact，继续既有metadata hover；本记录只关闭call-site consumer parity，不把reference display伪装成call contract。

## TDD与支持层证据

- isolated RED先固定`LinkedList<int>.addLast(1)`的exact method query identity。旧consumer实际返回`fn addLast(int): LinkedNode<int>`和parameter `int`，证明closed canonical types已存在，缺口仅是descriptor参数名与统一hover/signature投影。
- builtin GREEN固定query source kind、module/member/owner identity、`fn addLast(value: int): LinkedNode<int>`、parameter `value: int`、active parameter 0及hover provider source。
- descriptor-plugin GREEN固定`ProbePoint.total(): int`，替换为raw descriptor `total(): float`并reload owning project后重新查询；canonical display变为`fn total(): double`，证明结果同时跟随current provider generation和parser canonical normalization。
- negative fixture发布`incomplete_total(): unknown`。query仍识别native method，但signature help必须返回false/null，证明不会回退到canonical-wide callable、AST或member name。
- stdio在真实document中调用`LinkedList<int>.addLast(1)`，直接验证`textDocument/signatureHelp`与`textDocument/hover`共享closed label和builtin provider source。

## 工具链与回归证据

- 正式隔离快照为`HEAD d5d9764 + 9个LSP code/test exact paths`；9个文件与共享工作树逐一SHA-256一致，Syntax 03 parser/core/compiler/AOT/CMake/tests中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0和MSVC 19.44.35228 / `VSCMD_VER=17.14.36`均在独立static cache重建并运行同一16目标矩阵；每套16/16真实process exit 0，`Fail -`、`FAIL:`与`:FAIL:` marker为0。
- 每套project features为`54/54`，UTF-16 ranges为`3/3`，source contracts为`38/38`；parser query/facts、canonical consumers/graph、semantic analyzer/query、interface、incremental parser、language feature matrix和expression facts均在同一runner通过。
- GCC、Clang和MSVC分别直接执行更新后的`tests/language_server/stdio_smoke.js`；三套language-server stdio与CLI子进程真实exit 0。runner未使用会被外层PowerShell提前展开的bash`$?`或`$code`。
- 新增external callable contract/signature实现通过Clang与MSVC warning审计。Clang的5个metadata provider unused-static warning属于隔离HEAD既有代码，不由本阶段新增逻辑产生。

## Snapshot、Schema与协议边界

- 本阶段不修改parser semantic fact、native descriptor ABI、artifact/schema generation、document snapshot或cache key。consumer只借用当前query生命周期内的descriptor与canonical context，不跨generation缓存指针。
- provider reload后重新运行metadata semantic query与parser `CallAt`，因此descriptor identity和closed canonical TypeId必须属于同一最新document analysis。
- 直接协议覆盖为`textDocument/signatureHelp`和`textDocument/hover`；project fixture额外覆盖descriptor plugin watched reload与incomplete contract。
- 本阶段没有p50/p95/p99、峰值内存、cancellation latency或snapshot race压力报告，因此不晋级完整L6 robustness。

## 未完成边界

- native generic method clause、effectful method display、bare/static method统一contract尚未完成；当前分别返回unavailable或保留既有consumer。
- constructors、properties、meta-members及binary callable provider尚未进入同一external callable query adapter。
- public type/property/layout hash、package/alias export传播、artifact/provider replacement、snapshot race、cancellation及性能/内存预算仍待后续子里程碑。
