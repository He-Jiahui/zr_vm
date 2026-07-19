---
plan_id: lsp-03-robustness
record_id: 2026-07-20-module-identity-reverse-dependency-invalidation
status: completed
completed_at: 2026-07-20 02:10 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: module-identity-reverse-dependency-invalidation
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
related_tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/stdio_smoke.js
---

# ModuleIdentity Reverse-Dependency Invalidation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 02:10 +08:00 | 已完成 | 在既有ModuleIdentity/moduleName依赖图上建立首个可计数的反向依赖失效范围：显式返回类型的顶层普通函数body edit保留importer语义分析；无返回注解函数body edit与公开signature变化保守重分析传递闭包；增加累计preservation/reanalysis及单次reanalysis计数；GCC/Clang/MSVC十五目标矩阵、project场景与stdio/CLI烟测 |

## 已完成契约

- 更新后的source module仍先完成自身document parse和semantic analysis。本阶段只缩小其transitive importers的重分析范围，不声称changed module已经局部parse或局部bind。
- 只有增量分类为`ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY`、旧声明为顶层`ZR_AST_FUNCTION_DECLARATION`、新AST仍定位到顶层普通函数且存在显式`returnType`时，才认定module contract保持不变并跳过reverse-dependency traversal。
- 无返回类型注解的公开函数继续保守重分析importer，因为body可能改变推断出的公开契约。signature、module、fallback、method/property/lambda和无法重新定位声明的变化也继续走完整反向依赖传播。
- project index新增累计`reverseDependencyPreservationCount`、累计`reverseDependencyReanalysisCount`和每次刷新`lastReverseDependencyReanalysisCount`。只有成功更新一个transitive importer后才增加reanalysis计数，每次project refresh开始前重置单次计数。
- 传播继续复用现有按record `moduleName`排队、去重并遍历transitive importers的图，不新增第二套依赖身份或LSP私有类型推断。

## TDD与范围证据

- RED在现有source module refresh用例中加入显式返回类型函数body edit，精确失败为`A body-only source edit should preserve importer semantic analysis`；该失败固定了“body变化不得无条件重分析importer”的新要求。
- GREEN第一步把`answer(): int`的body从`1`改为`2`：change impact保持declaration body，main analyzer指针和execution count不变，preservation增加1、累计reanalysis不变、单次reanalysis为0，hover仍为`int`。
- 负边界把无注解`inferred()`的body从`1`改为`1.5`：main analyzer仍复用同一实例，但execution count和累计reanalysis各增加1，preservation不变，单次reanalysis为1。
- signature边界把`answer(): int`改为`answer(): float`：importer再次重分析，累计reanalysis增加到2，completion/hover公开契约更新为`float`。

## 工具链与回归证据

- 最新验证基线为`HEAD=070a437`加本阶段三个代码/测试文件。WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228均完成相同十五目标矩阵：`run=15`、`exit_failures=0`；相关project测试进程均`exit=0`，`LSP Source Module Refresh Reanalyzes Open Documents`通过。
- 三套矩阵均保留一个当前基线失败标记：`Semantic Analyzer Generic Function Symbols Surface Signature Detail`因`%ref parameter requires the 'ref' argument marker (line 6)`失败。project suite均保留四个与本阶段无关的binary metadata/descriptor plugin/reference/highlight失败；因此不声称完整suite全绿。
- MSVC首次增量运行因旧静态对象ABI不一致使15个进程全部以Windows异常码退出；清理761个产物并完整重建662个对象后，15个目标全部正常退出，证明旧结果属于缓存污染而非本阶段语义失败。
- GCC、Clang和MSVC的`stdio_smoke.js`均`exit=0`；GCC/Clang使用WSL stdio服务与CLI，MSVC使用干净重建的647对象stdio缓存。

## 未完成边界

- 本阶段没有module public signature hash，也没有旧到新ModuleIdentity的edge migration；module rename、import alias/package export、public type/property/resource/layout、`.zrp/.zrm`替换和native descriptor/schema仍保守传播或待专门证据。
- 显式返回类型规则当前只覆盖顶层普通函数。method、property accessor、constructor、generic constraint、receiver effect、lambda和跨文件声明合并尚未建立contract-preservation证明。
- local signature/generic/receiver变化的direct-caller失效尚未实现；这是下一子里程碑。
- cancellation、snapshot race、stale response suppression、历史snapshot/LRU/256MiB上限、延迟百分位预算和剩余provider parity仍待后续阶段。
- 工作树中的syntax计划、receiver-call测试、core profiling helper和构建目录属于并发任务或外部baseline，不在本记录与提交范围内。
