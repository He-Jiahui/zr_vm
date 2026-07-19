---
plan_id: lsp-03-robustness
record_id: 2026-07-20-local-signature-generic-direct-dependency-invalidation
status: completed
completed_at: 2026-07-20 05:56 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: local-signature-generic-direct-dependency-invalidation
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
related_tests:
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_dependency_cases.h
  - tests/language_server/test_lsp_local_semantic_receiver_dependency_cases.h
  - tests/language_server/stdio_smoke.js
---

# Local Signature And Generic Direct-Dependency Invalidation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 05:56 +08:00 | 已完成 | 顶层普通函数signature/generic变化按canonical resolved reference declaration range区分未受影响scope与direct dependency；无返回注解函数body变化按同一边失效；显式返回类型body变化保留稳定caller；receiver target缺失、poisoned/unavailable facts保守失效；新增direct/fallback分类计数；稳定HEAD三工具链十六目标矩阵与stdio/CLI smoke完成 |

## 已实现契约

- 同长度、声明外cached scope、坐标与scope hash最终仍稳定时，顶层普通函数signature/generic变化不再无条件清空单一scoped-query cache。只有canonical reference facts证明该scope没有指向变化声明的resolved edge时才允许保留。
- resolved `SZrSemanticReferenceFact.declarationRange`是依赖身份。普通调用和函数值读取都按声明range识别；不比较成员名、display string或源码token，也不在LSP复制overload/generic类型规则。
- 无显式返回类型的普通函数body可能改变推断公开契约，因此对其direct dependency执行同一失效。显式返回类型函数的body-only edit不改变本阶段采用的callable contract，可保留坐标/hash稳定的direct caller cache。
- facts数组无效、semantic/query diagnostics非空、未解析call、closure标识符缺少同范围resolved reference或变更声明无法映射到受支持的顶层普通函数时返回unknown并保守失效。
- public metrics保留总`scopedCacheInvalidationCount`，并新增`scopedCacheDirectDependencyInvalidationCount`和`scopedCacheConservativeInvalidationCount`。自身声明重叠、byte长度/坐标/hash变化属于local invalidation，只增加总计数。
- 当前仍只有一个owned scoped analyzer/cache；本阶段证明的是该单一cache每次更新的实际保留或失效范围，不宣称已有多函数cache或局部主document bind。

## Receiver边界

- Syntax 02 M4已经把receiver effect写入source/imported/native member contract，但其完成记录明确把resolved receiver-call target fact publication留给后续consumer convergence。
- receiver effect本身不是target identity。当前member signature/receiver变化不能按direct caller精确传播，测试要求它进入conservative fallback并增加fallback计数。
- 在canonical call/reference query发布receiver call的resolved `SymbolId`和declaration range前，不允许用member name推断target。该缺口仍属于同一L6表项的后续子里程碑。

## TDD与范围证据

- 首个RED在旧的body-only preservation实现上运行既有24项加两个新用例：generic signature无法保留unrelated scope；无返回注解函数body变化错误保留direct caller。实现resolved declaration-range依赖后转为26/26。
- poisoned boundary随后RED为`Poisoned dependency scope was reused without a proven edge set`。依赖判定改为unknown/none/direct三态后转为27/27。
- 显式返回类型body/direct caller反向边界直接GREEN；函数值读取边界先证明变化后cache必须失效，并确认scoped analyzer发布了resolved declaration edge。
- 分类计数RED在GCC编译期精确失败于两个metrics字段不存在。四态change decision接入后，首次运行29/30；实际函数值计数为`total=1, direct=1, conservative=0`，测试随canonical事实修正后为30/30。
- receiver signature用同长度`const fn`到`fn`加空白更新，change impact保持declaration signature；无返回注解method body从`1.0`改为`"a"`时保持declaration body。两种unsupported member变化都保守清除unrelated cached scope并记录`total=1, direct=0, conservative=1`。

## Snapshot、Schema与编辑版本

- TDD基线起点为`HEAD=c9c51d9`，最终验证基线为稳定`HEAD=62eefc2`加本阶段工作树。`c9c51d9`这个Syntax提交意外包含了本阶段早期LSP scope-cache实现和29项测试，但没有LSP计划记录；本记录对应的独立LSP提交继续包含分类metrics、receiver fallback测试、文档和最终验证证据，不重写已推送历史。
- focused fixtures全部从document version 1开始，真实内容更新使用version 2；generic连续边界使用version 1到4。相同或stale version仍由既有严格单调门禁拒绝。
- 本阶段不改变query/schema generation编号。依赖读取当前scoped semantic context的canonical expression/reference/query-diagnostic arrays，并受document AST、完整scope range与scope AST hash共同约束。
- focused测试通过内部document update入口模拟didOpen/didChange生命周期；三工具链最终stdio smoke在稳定HEAD上验证了真实`initialize/didOpen/didChange/request/didClose`通路。

## 工具链与回归证据

- TDD和最终focused入口均为`zr_vm_language_server_local_semantic_query_test`，GCC、Clang和MSVC各为31/31、无`Fail -`、exit 0。
- 稳定`HEAD=62eefc2`加本阶段工作树上，GCC 11.4、Clang 14和clean-rebuilt MSVC 19.44.35228各构建并运行同一十六目标矩阵，全部16/16进程exit 0。三者都只保留相同既有基线：semantic analyzer的`Generic Function Symbols Surface Signature Detail`一个`%ref`标记，以及project suite的binary import metadata、descriptor-plugin receiver completion、binary references和binary document highlights四个标记；因此不声明全仓GREEN。
- GCC和Clang专用Ninja缓存分别完成507步shared stdio/CLI重建，MSVC专用缓存完成clean后650步重建；三者运行`node tests/language_server/stdio_smoke.js <server> <cli>`均exit 0。MSVC环境为`VSCMD_VER=17.14.36`。
- 延迟p50/p95/p99、峰值内存与256MiB/LRU预算未在本子里程碑测量，因此不声明达到L6性能退出条件。

## 未完成边界

- receiver method/member、property accessor、constructor、dynamic/interface/native dispatch的resolved target尚未进入canonical reference/query，不能精确失效direct callers。
- 主document仍整文件parse/analyze；没有partial reparse、multi-function semantic cache、历史snapshot上限或workspace semantic cache LRU。
- module public hash、ModuleIdentity edge migration、public type/property/layout/import/artifact传播仍待后续。
- cancellation、snapshot race、stale response suppression、乱序请求、provider全面对等和所有延迟/内存预算仍未完成。
