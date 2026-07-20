---
plan_id: lsp-03-robustness
record_id: 2026-07-20-canonical-source-public-contract-hash
status: completed
completed_at: 2026-07-20 08:57 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: canonical-source-public-contract-hash
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_public_contract.c
related_tests:
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_public_contract_cases.h
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_public_contract_cases.h
  - tests/language_server/stdio_smoke.js
---

# Canonical Source Public-Contract Hash

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-20 08:57 +08:00 | 已完成 | parser semantic query发布source module canonical public-contract hash v1；LSP按旧/新hash、export count和module identity区分reverse-dependency保留、变化与unavailable保守传播；覆盖声明顺序、generic owner归一化、参数名、passing/escape/effect contract、unsupported/poisoned边界与module identity漂移；三工具链十六目标矩阵、26项semantic query、project场景及stdio/CLI smoke完成 |

## 已实现契约

- `ZrParser_SemanticQuery_PublicContract`只消费同一semantic context、type environment和module AST的canonical facts。成功时返回稳定`hash`和`exportCount`；任何无法证明canonical的输入返回false并把输出清零。
- hash schema固定为version 1并使用稳定FNV-1a编码。primitive、nominal module/name、generic parameter ordinal、generic instance、array、tuple、union、never、ref、owner、readonly/nullable和function type均按结构编码，不散列进程地址、临时`TypeId`或generic owner `SymbolId`。
- callable contract包含有序参数名、canonical type、passing form、escape upper bound、entry/exit initialization、temporary policy、call-site marker、return type、receiver effect和effect flags。generic declaration flags也进入hash。
- export先生成独立条目，再按kind/name/contract hash排序，因此顶层声明顺序不会改变结果。当前AST/type environment没有保留普通顶层函数的完整visibility事实，所以所有普通顶层函数都纳入hash，形成保守过失效；变量只纳入显式public mutable declaration。
- LSP source record保存hash、export count和availability。旧/新记录都available且module identity、hash、count相同才保留importers；任一值变化则传播；任一侧unavailable也传播。module name发生漂移时，遍历从旧identity起点查找原importers。
- public-contract分类另有match/change/unavailable累计计数；既有reverse-dependency preservation、cumulative reanalysis和last-refresh计数继续描述实际传播结果。

## 保守边界

- named generic constraint目前只有源码字符串，没有canonical constraint `TypeId`，因此整个query返回unavailable，禁止跨provider按名字假定相等。
- public type/layout声明、extern、compile-time、intermediate declaration/statement、public const、default/variadic/decorated callable、错误类型、缺失或损坏fact、semantic/query diagnostics以及context/type-environment owner不一致均返回unavailable。
- nominal definition token没有进入source contract hash；provider/artifact generation失效仍由既有module/provider图负责。
- 本阶段不发布receiver-call resolved target identity，也不按member name推断依赖。`SymbolId`加declaration range的receiver target属于后续consumer convergence。

## TDD与范围证据

- 首个parser RED因public-contract query type/API不存在而编译失败。初始实现转为21/21后，审计RED在参数名与unsupported surface上出现2项失败；修正后23/23。
- 最终三项负边界先只重编测试并链接旧GCC parser库，得到26项中的3项失败：mismatched semantic owners、named generic constraint和intermediate artifact均被错误接受。生产修正后同一入口为26/26。
- parser用例同时固定private variable变化不改hash、public function/public mutable variable变化改hash、声明顺序稳定、generic owner ID漂移稳定、generic flags/参数名改变hash，以及default/variadic/public const/diagnostic/public type返回unavailable并清零。
- project fixture以version 1打开source/importer；version 2只改private变量类型并保持hash和importer execution count；version 3改public参数名、version 4改public返回类型并各重分析一次；version 5加入unsupported public type并证明unavailable保守传播。
- MSVC首次暴露project测试直接链接两个DLL私有查询符号。测试改为只读遍历公开测试结构中的project/file arrays，没有新增生产导出或ABI。

## Snapshot、Schema与编辑版本

- parser query schema为public-contract hash v1。本阶段不修改protocol generation、artifact schema generation或document snapshot编号。
- TDD与GCC/Clang最终验证基于稳定`HEAD=344cf6a`加本阶段10个exact paths；共享树同时存在Syntax M6非LSP中间态，因此最终MSVC另建固定`344cf6a` snapshot并只覆盖这10个paths。
- MSVC snapshot逐文件SHA-256与本阶段工作树一致，排除所有Syntax M6 dirty paths；配置为`BUILD_SHARED_LIB=OFF`、`BUILD_STATIC_LIB=ON`，使用`VSCMD_VER=17.14.36`。
- 真实project编辑版本为1到5。相同或stale version仍由既有严格单调门禁拒绝；本阶段未增加version旁路。

## 工具链与回归证据

- GCC 11.4和Clang 14分别完成671步与570步目标重建；MSVC 19.44.35228在隔离snapshot完成597步静态目标重建。三者运行同一十六目标矩阵，均为16/16进程exit 0。
- 三工具链的`zr_vm_semantic_query_test`均为26/26；真实project场景`LSP Source Module Refresh Uses Canonical Public Contract Hash`均PASS；stdio/CLI JSON-RPC smoke均exit 0。
- 三者保留相同既有基线：semantic analyzer一个`%ref` call-marker文本失败，以及project suite四个binary/descriptor navigation文本失败。它们不属于本阶段，本记录不宣称全仓GREEN，也没有在LSP添加兼容分支。
- 早期MSVC共享树运行处于Syntax M6中间态并在canonical consumer处中断，明确作废；早期隔离DLL配置使用了错误开关名并暴露私有符号链接差异，也不计入最终矩阵。
- 本阶段没有测量p50/p95/p99、峰值内存、256MiB workspace cache或cancellation观察延迟，因此不声明达到完整L6性能退出条件。

## 未完成边界

- public type/property/layout的可用hash、import/alias/package edge迁移、binary/native/artifact同签名hash和provider generation仍待后续。
- 顶层普通函数visibility尚未成为canonical fact，当前hash可能对private函数变化保守传播。
- 主document仍整文件parse/analyze；没有partial reparse、多函数cache、snapshot race/stale response suppression或workspace LRU预算证据。
- resolved receiver/method/property/constructor target必须由canonical reference/query发布`SymbolId`和declaration range后再进入精确依赖图。
