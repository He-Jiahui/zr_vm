---
related_code:
  - zr_vm_language_server_extension/src/projectSupport.ts
  - zr_vm_language_server_extension/src/workspaceProjects.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/selectedProjectSync.ts
  - zr_vm_language_server_extension/src/projectActions.ts
  - zr_vm_language_server_extension/src/projectActionsWeb.ts
  - zr_vm_language_server_extension/src/debug/configProvider.ts
  - zr_vm_language_server_extension/src/structure.ts
  - zr_vm_language_server_extension/schemas/zrp.schema.json
implementation_files:
  - zr_vm_language_server_extension/src/projectSupport.ts
  - zr_vm_language_server_extension/src/workspaceProjects.ts
  - zr_vm_language_server_extension/test/projectSupport.test.js
  - zr_vm_language_server_extension/test/workspaceProjects.test.js
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/code-review/batches/20261013-review-plan.md
  - "user: extension project discovery contract lease r1, 2026-10-15"
tests:
  - zr_vm_language_server_extension/test/projectSupport.test.js
  - zr_vm_language_server_extension/test/workspaceProjects.test.js
doc_type: workflow-detail
---

# Extension 项目发现与项目选择契约

## 范围与单元清点

本批复核 `projectSupport.ts`、`workspaceProjects.ts` 及各自两份现有 Node 测试，逐项登记于
[project discovery coverage ledger](../coverage/zr_vm_language_server_extension_project_discovery.tsv)。
共 74 个单元：旧台账的 53 个精确 key 与结论全部保留，新增 21 个可调用单元。最终决定为
53 项 `commented`、10 项 `no-comment`、7 项 `BUG`、4 项 `TODO`。新增行中 11 项有就近意图
注释，10 项是所属函数契约已覆盖的局部纯谓词或单项查找；每个决定均说明理由。

TypeScript AST 交叉核对为：`projectSupport.ts` 有 11 个函数体与 3 个类型声明，11 个函数体由
9 个既有函数和 2 个新增 `filter`/`sort` 回调组成；`workspaceProjects.ts` 有 30 个函数体和
1 个接口，30 个函数体由 19 个既有函数与 11 个新增回调组成。另保留原登记的状态与独立风险
语句块。测试侧 `projectSupport.test.js` 的 2 个测试回调已各有旧 key；`workspaceProjects.test.js`
有 13 个函数体，其中 2 个 helper、3 个测试回调沿用旧 key，新增登记 URI、事件及 VS Code 替身、
VM require 和读计数器的 8 个可调用单元（7 个箭头函数和 1 个类方法）。普通类型注解中的函数型字段没有被重复算作函数体。

新增 21 个 key 为：

- `projectSupport.ts`：`pickBestProjectForFile::filter-contained-candidate`、
  `pickBestProjectForFile::sort-deepest-root-first`。
- `workspaceProjects.ts`：`discoverWorkspaceProjects::create-per-uri`、
  `discoverWorkspaceProjects::drop-invalid-project`、`discoverWorkspaceProjects::sort-by-relativePath`、
  `createWorkspaceProject::find-open-document`、`resolveSelectedProjectUri::find-active-project`、
  `resolveSelectedProjectUri::find-stored-project`、
  `resolveSelectedWorkspaceProject::find-selected-project`、
  `selectWorkspaceProject::map-quick-pick-item`、`selectWorkspaceProject::find-picked-project`、
  `findProjectFile::map-quick-pick-item`、`lastPathSegment::drop-empty-segment`。
- `workspaceProjects.test.js`：`projectHost::uri.toString`、`projectHost::EventEmitter.event`、
  `projectHost::getWorkspaceFolder`、`projectHost::getText`、`projectHost::openTextDocument`、
  `projectHost::fs.readFile`、`projectHost::sandbox.require`、`projectHost::diskReads`。

导出的 TypeScript 类型、事件及函数均有紧邻导出的中文 Markdown JSDoc，按单元适用情况说明
`Summary`、`Parameters`、`Returns`、`Constraints and lifecycle`、`References` 或字段模型；内部
函数也记录输入输出、失败边界和借用生命周期。测试注释界定假宿主的能力，未把单清单 VM 测试
描述成完整扩展集成。源码与测试只增改注释/空白，没有改变声明、表达式、调用、断言或运行行为。

## 既有问题结论

7 个已证实 BUG 及 4 个待核 TODO 均保留原 key、分类和证据；本批没有通过补文档暗中修复行为：

- BUG：`normalizeFilePath` 把 `/` 归一为空，导致 POSIX 根路径拼接与匹配失效；根目录 `.zrp`
  经 `dirnameFromPath` 得到错误基目录；`joinNormalizedPath` 在 `D:/project/../../src`
  折叠中丢失盘符根；JSON `null` 越过静态类型断言并在字段读取时抛错，可使桌面 LSP 客户端
  启动的项目发现拒绝；单个 `readFile` 拒绝经 `Promise.all` 使整次项目发现失败；活动项目优先于保存选择，
  桌面事件重入可把用户选定的 B 改回活动文件所属 A；“deepest source root”测试仅有一个匹配根，
  删除最长根排序后仍可通过。
- TODO：确认清单 `source` 是否允许 Windows 盘符绝对路径及其与 DAP `path.resolve` 的语义；确认
  结构视图先后两次项目扫描之间工作区变化时的快照一致性；确认解析和 Quick Pick 是否应按传入
  `folder` 限定多根工作区；确认 `findProjectFile(folder)` 是否应拒绝其他根中活动的 `.zrp`。

DAP 的 `createLaunchSourceContext` 将读取和解析包在 `try/catch` 内，失败会回退默认 `src`/`bin`。
取消 Quick Pick 后 `find` 仍遍历但不匹配项目；记忆选择须匹配本轮发现候选，不会无条件返回保存 URI。

上述范围之外的真实 VS Code 多根工作区、文档变化事件、LSP 生命周期与传输行为不从这些单元
外推。`workspaceProjects.test.js` 的 `openTextDocument` 是禁止调用的失败哨兵；选择事件的
`EventEmitter.event` 只提供模块加载所需的空替身，不覆盖订阅、派发或 Disposable 生命周期。

## 锚点和验证证据

旧台账 53 行的 unit key 和决定未变；四个受改文件的调用/证据行号按当前差异映射更新。测试夹具
`workspaceProjects.test.js` 原第 19 行的一行式 `fs.readFile` 箭头函数随周边注释移动到第 50 行，
回调函数体仍是单行；台账引用指向实际回调。新增可调用单元行指向当前 AST 函数体及真实容器调用点；测试回调另指向其 VM
调用、产品消费或断言。没有把函数声明冒充上游调用。

`python scripts/code_review_inventory.py check-batch docs/code-review/coverage/zr_vm_language_server_extension_project_discovery.tsv`
已退出 0。另逐条核对当前工作树中本模块四文件的 `callers`/`evidence` 行号，均落在非空
源码行；与 HEAD 台账比较，旧 53 行的 key 和 decision 零漂移。该锚点检查不替代调用方向和语义
判断，相关调用链仍按实现及测试逐项登记。

Root 管理的预编辑 R2 基线记录于
`E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/extension-project-client-preedit-node-r2-results.json`
（SHA-256 `f0b041878374107340dd415ded12f0cacf092763474d65a78f6fe0b6a3e83d85`）：选定的 3 个 TS
文件 typecheck/emit 成功，原 3 个 Node 测试文件 9/9 通过。测试在原 checkout 路径执行；仅将
指定测试文件中的 `../out` 导入精确路由到 E 根外部产物。workspace 测试还会从当前工作树内存
转译 `workspaceProjects.ts` 并使用假 VS Code；这不是完整扩展构建、真实 VS Code/LSP 会话、真实
传输或多根选择覆盖。该证据是注释前 baseline，不单独证明本批后的测试结果；本批 producer 未
运行构建或测试，注释后的等价及相同 Node 用例复核由 Root 的冻结后验证记录负责。

最终交付源码与本叶原始字节 SHA-256（Root 同行注释措辞修订后）：

| 路径 | SHA-256 |
| --- | --- |
| `zr_vm_language_server_extension/src/projectSupport.ts` | `8638d98384b527d4f07d2a70104ce9cc7a968da5ef0f03164fc13141289b6d72` |
| `zr_vm_language_server_extension/src/workspaceProjects.ts` | `640d1c2bd957ffcf19dbd8b44bf9fdc446d0276f9eedf5218dcd1cf309e41787` |
| `zr_vm_language_server_extension/test/projectSupport.test.js` | `8cd1a7cd274e6b93844bccd1845432ddcaa9ab3b42b3ef7fb4e941ba39ea655d` |
| `zr_vm_language_server_extension/test/workspaceProjects.test.js` | `16889e6892a44939b742539d93ff79804b6536eaeb9914e393ac50dd5321af8f` |
| `docs/code-review/coverage/zr_vm_language_server_extension_project_discovery.tsv` | `e883ed6b4743042b395175c02f2ed1a97bb5716b23711849fec914422cdc187f` |

## Root 最终验收与验证边界

最终四文件的 3,438 个有效 token 的 kind、原始拼写和源码语法结构均与父提交一致，去注释 emit 的 token/语法结构也相同。产品 TS emit 字节相同；JS 测试的多行对象仅有打印空白差异。最终 R4 报告 `extension-project-final-comment-equivalence-r4.json` 的 SHA-256 为 `fa538b723d38fb905482b3df065980d1f8d567e0bdc1c880648ed3743325c207`。

实际修改后 R1 验证编译选定 3 个 TS 文件成功（1.000 秒），原 3 个 Node 测试文件 9/9 通过（0.969 秒），用例编号、名称、结果与预编辑 R2 一致；三个实际生成的产品 JavaScript 文件逐字节相同。实际运行早于最终同行注释措辞修订，由 R4 将精确交付字节绑定到相同程序，没有声称实际运行编译了最终注释。

运行时 250 个保守输入 pin 包含台账元数据，只有本批四源和本叶共五项 raw hash 漂移，其余 245 项及当时 10 个冻结输入在运行窗口稳定。后续客户端注释写租约不改变该历史窗口证据。所有验证子进程均已等待退出；不外推完整扩展构建、真实 VS Code/SDK/LSP 传输或多根选择覆盖。

29 个反向锚点逐一核对原始源码行：activation、config、debug_launch 和 project_actions 四叶只改 28 个行号数字，key、decision、契约不变；structure 的测试第 16 行仍有效，未修改。本叶 74 项与 activation 的 `check-batch` 通过。其余三叶仍有移除的 pnpm-lock、旧 LSP 路径或 cliLauncher 空行锚点错误；内存读取 HEAD 台账执行相同检查，证明最终全部错误已存在，另两个旧 projectSupport 空行引用已更正。不授予这些反向模块整批通过信用。

- 实际 R1 summary：`extension-project-final-node-r1-summary.json`，SHA-256 `1a82d5475fe342ba1fff388269dc3e2c50fbdfecfdc929a5d1472982538d95bb`。
- 实际前后 comparison：`extension-project-final-node-r1-comparison.json`，SHA-256 `af5fa7c659e22dd2265e61f1bf04761a35faea2d9d33fa71865cb1a9b1fcc266`。
- 同行措辞和反向行号修订：`extension-project-final-wording-anchor-repair-r1.json`，SHA-256 `9aa6be05685893ab146cbd1007e2b3d4fa4ae2764cfdbd6f77924ad4452402d3`。
- check-batch 与既有错误比较：`extension-project-final-check-batch-r1.json`，SHA-256 `2fbb41d839d2191bb78048287beb4d36a87e2d2d98b352e4792da0724cd6e64e`。

没有新增测试或 BUG/TODO 结论。
