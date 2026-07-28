# Syntax 55 份状态记录重新测试、验收与 Review

## 结论

- 复验日期：2026-07-28（UTC+08:00）。
- 清点规则：递归扫描 `docs/plans/syntax` 下名称以数字开头的子目录中的
  Markdown 文件，排除 `*-implementation-plan.md`。
- 清点结果：严格得到 55 份状态记录；55 份均自述为某种完成状态。
- 新鲜验收结果：**55/55 在各自声明的叶子范围内确认完成**。
- Review 结果：初审发现的构建图、属性/引用/所有权、AOT/GC、模块初始化、
  typed frame、metadata 跨平台、线程隔离域生命周期以及 debug 测试函数图销毁
  顺序问题均已修复；当前叶子范围内没有遗留 P1/P2 功能或内存安全缺陷。
- 整体 Syntax 计划：**仍未整体完成**。根 `README.md` 的上层 promotion gate
  仍包含 06B、07B、08、09、10F/10C、11、14 等未关闭范围。55 份叶子记录
  的完成不能自动提升整个 14 部分 Syntax redesign。

原始状态文本的写法并不完全统一：包括普通/反引号 `completed`、中文“已完成”、
带 M2/M3/M4 gate 限定的“已完成”，以及
`completed_with_known_baseline_failures`、
`completed_with_known_baseline_markers`、
`completed_with_known_unrelated_markers`。本报告保留这些历史限定；“确认”表示当前
工作树已用新鲜测试重新证明该记录声明的范围，而不是改写其历史状态文字。

## 2026-07-28 上层 Gate 补充复验

本次复验重新按同一规则独立清点状态记录，机器计数为：总数 55、完成状态 55、
缺失状态 0。目录分布为 01=5、02=6、03=5、04=7、05=6、06=2、07=1、
10=5、12=15、13=3；与下方 55 行逐项矩阵一致。

本轮在推进 08 reflection 与 09 generational pool 的实现后取得以下新鲜证据：

- Windows MSVC 19.44 Debug 短路径全目标构建成功，CTest 注册 121 项。首次全量
  运行 120/121，通过 review 定位并修复唯一失败的 callable TypeId 规范化问题后，
  `language_pipeline` 定向复跑 1/1 通过。该证据应准确理解为“首次 120/121 加
  修复后原失败 suite 1/1”，不是一次修复后的 121/121。
- 修复后的 Windows 定向门禁：reflection surface 18/18、reflection stress 3/3、
  generational pool 9/9、module system 89/89；同时 buffer pool/FFI 8/8、legacy
  migration 9/9、compile time 32/32、decorator 4/4、expression fragment 3/3、
  syntax reference v1 7/7、module specifier 5/5、manifest v2 8/8 均通过，LSP
  expression-fact hover 退出 0。
- WSL GCC 11.4 Debug 短路径定向构建成功；Linux reflection surface 18/18、
  reflection stress 3/3、generational pool 9/9 通过。未将未完成的 GCC 全目标
  构建或未执行的 GCC suite 记作通过。

本轮 review 新发现并修复三项问题：

1. constructor binder 原先只按参数个数缓存并选择构造函数，无法区分同参数个数的
   重载。现以参数类型签名参与缓存键和唯一最佳匹配，真实歧义仍拒绝；测试覆盖命中、
   未命中、类型重载和重复签名歧义。
2. source 与 binary callable 共享 TypeId 时，第二次 `typeof` 会尝试绑定新的描述符并
   失败。现解析并复用已绑定的 canonical descriptor，不弱化 TypeId 身份唯一性。
3. pool guard 取得后无锁再次遍历可扩容的 slab 表，和并发 deliver 存在数据竞争。
   acquire 现于锁内解析并缓存稳定 value 地址，guard 读路径不再访问可重分配表。

上层 Gate 结论保持保守：08 的 spread 调用/AOT 等完整验收、09 的跨 consumer 集成，
以及 11、14 的实施状态仍未关闭，因此依赖它们的 10C、06B、07B 也不能宣告完成。
本轮提交推进并加固 08/09，但不修改这些根 Gate 的状态。

## 55 份记录逐项结果

| # | 记录 | 自述状态 | 新鲜结果 | 主要证据 |
| ---: | --- | --- | --- | --- |
| 1 | 01 M1 type graph | completed | 确认 | canonical type graph 与三工具链 language pipeline 通过 |
| 2 | 01 M2 place/CFG | completed | 确认 | place/CFG、flow 与 compiler integration 通过 |
| 3 | 01 M3 pre-SemIR | completed | 确认 | pre-SemIR、struct value init 与 ASan slot relocation 回归通过 |
| 4 | 01 M4 artifact schema | completed | 确认 | artifact schema/roundtrip 与 canonical consumers 通过 |
| 5 | 01 M5 canonical consumers | completed | 确认 | parser、LSP、artifact consumer 门禁通过 |
| 6 | 02 M1 syntax contract | completed | 确认 | syntax reference v1 与 parser suite 通过 |
| 7 | 02 M2 place/out | completed | 确认 | place/out 与 reference lowering 通过 |
| 8 | 02 M3 loan/NLL | completed | 确认 | loan/NLL、receiver conflict 与 flow suite 通过 |
| 9 | 02 M4 receiver boundary | completed | 确认 | receiver-boundary property 回归通过 |
| 10 | 02 M5 escape/suspension | completed | 确认 | reference escape/suspension 与更新后的 await fixture 通过 |
| 11 | 02 M6 artifact/LSP consumers | completed | 确认 | reference fact、artifact 与 LSP consumer 通过 |
| 12 | 03 M1 layout/copy maps | completed | 确认 | type layout、inline copy 与 metadata contract 通过 |
| 13 | 03 M2 receiver effect | completed | 确认 | member access、receiver effect 与 GC teardown 通过 |
| 14 | 03 M3 ref struct | completed | 确认 | ref-struct restriction/escape/layout suite 通过 |
| 15 | 03 M4 Span core | completed | 确认 | Span、contiguous view/bounds 与 GC suite 通过 |
| 16 | 03 M5 buffer pool/FFI | completed | 确认 | buffer pool 与 FFI 29/29 通过 |
| 17 | 04 M1 Unique/Drop | completed | 确认 | Unique/Drop 19/19 与 ownership pipeline 通过 |
| 18 | 04 M2 Shared/Weak | completed | 确认 | Shared/Weak 11/11 与 cycle diagnostics 通过 |
| 19 | 04 M3 owner/borrow receiver | completed | 确认 | owner/borrow receiver 6/6 通过 |
| 20 | 04 M4 domain identity/bridge | completed | 确认 | domain bridge、multimutator 与 GC suite 通过 |
| 21 | 04 M5 domain-local handoff | completed | 确认 | same-domain handoff 与 scheduler route 通过 |
| 22 | 04 M6 cross-domain transport | completed | 确认 | transfer、race、quota 与 shutdown 路径通过 |
| 23 | 04 M7 concurrent/artifact/AOT/LSP | completed | 确认 | AOT、GC teardown、artifact 与 LSP 聚合门禁通过 |
| 24 | 05 M1 unified AST | completed | 确认 | unified property AST 16/16 通过 |
| 25 | 05 M2 explicit field/init | completed | 确认 | explicit field/init 21/21 通过 |
| 26 | 05 M3 access lowering | completed | 确认 | property access lowering 22/22 通过 |
| 27 | 05 M4 ref-return property | completed | 确认 | ref-return property 23/23 与 SemIR relocation 回归通过 |
| 28 | 05 M5 property consumers | completed | 确认 | property consumers 9/9 与 LSP/reflection 通过 |
| 29 | 05 M5.1 variance LSP follow-up | completed with known unrelated markers | 确认 | semantic analyzer 与相关 LSP suite 通过 |
| 30 | 06 M1 migration inventory | completed | 确认 | Windows 与 WSL inventory/golden 一致通过 |
| 31 | 06 M2 frontend/LSP fixes | completed | 确认 | CLI migration、legacy migration 与 LSP 通过 |
| 32 | 07 M1 reference fixture manifest | completed | 确认 | syntax reference v1 通过；范围仍仅为 07A |
| 33 | 10 M1 specifier foundation | completed | 确认 | module specifier/resolver 与 projects 通过 |
| 34 | 10 M2 artifact provider phase | completed | 确认 | provider/import、module init 与 projects 通过 |
| 35 | 10 M2 v2 declarations | completed | 确认 | manifest v2 与 metadata suite 通过 |
| 36 | 10 M2 v2 manifest admission | completed | 确认 | admission/decomposition 与 artifact consumer 通过 |
| 37 | 10 M2 v2 writer lock | completed | 确认 | ZRM/manifest writer 与 CLI integration 通过 |
| 38 | 12 M1 task syntax/effect | completed | 确认 | task CFG/inference 与 language pipeline 通过 |
| 39 | 12 M2 task frame runtime | completed | 确认 | task frame、typed-frame reset 与 GC suite 通过 |
| 40 | 12 M3 job scheduler | completed | 确认 | scheduler descriptor/runtime 与版本 contract 通过 |
| 41 | 12 M4 attached-domain scheduler | completed | 确认 | 三工具链 thread runtime 各 20/20 压力通过 |
| 42 | 12 M5 isolated-domain transport | completed | 确认 | isolated transport、ASan completion lifetime 20/20 通过 |
| 43 | 12 M6.1a artifact contract | completed | 确认 | artifact schema 与 canonical consumers 通过 |
| 44 | 12 M6.1b.1 imported identity | completed | 确认 | imported scheduler identity 与 consumers 通过 |
| 45 | 12 M6.1b.2a source fact | completed | 确认 | compiler scheduler source fact 与 LSP 通过 |
| 46 | 12 M6.1b.2b writer | completed | 确认 | artifact roundtrip/writer 与 consumers 通过 |
| 47 | 12 M6.2 task migration | completed | 确认 | task migration、Linux/Windows thread suite 通过 |
| 48 | 12 M6.3 debug/fault projection | completed | 确认 | debug/fault projection 与 thread stress 通过 |
| 49 | 12 M6.4 LSP projection | completed with known baseline failures | 确认 | 相关 LSP suite 在限定范围内通过 |
| 50 | 12 M6.4a reachability spans | completed with known baseline markers | 确认 | hover/reachability 与 LSP supporting suite 通过 |
| 51 | 12 M6.4b generic projection | completed | 确认 | native iterable/generic projection 与 type inference 通过 |
| 52 | 12 M6 umbrella | completed | 确认 | 正文 `in_progress` 为历史阶段快照；最终 gate 已关闭且聚合门禁通过 |
| 53 | 13 M1 enumerator protocol | completed | 确认 | enumerator、iterable inference 与 container suite 通过 |
| 54 | 13 M2 yield syntax/SemIR | completed | 确认 | yield syntax、semantic 与 iterator SemIR 通过 |
| 55 | 13 M3 iterator frame runtime | completed | 确认 | iterator frame、GC/drop 与声明的 no-lowering 边界通过 |

## 最终验证矩阵

| 环境 | 构建 | 注册/聚合测试 | 最终差量与压力证据 |
| --- | --- | --- | --- |
| Windows MSVC 19.44 Debug | 全目标增量成功；提交前除并发 `debug_metadata` RED 外的目标重建成功 | 历史完整 CTest 119/119；提交前其余 117 项通过，`debug_library` 修复后连续 100/100 | thread runtime Debug 20/20；MSVC ASan thread 20/20；`debug_library` ASan 5/5；metadata golden 1/1 |
| WSL GCC 11.4 Debug | 最终 331/331 链接成功 | 完整注册矩阵由 1–97 与 98–119 两段确认 119/119；最终差量 4/4 | thread runtime 20/20；language pipeline 811.76 s、projects 107.81 s |
| WSL Clang 14 Debug | 最终 331/331 链接成功 | 早期完整运行仅两个 CLI 启动超时，二者定向复跑通过；最终差量 4/4 | thread runtime 20/20；language pipeline 704.83 s、projects 170.27 s |

Clang 的“119/119 覆盖”是完整运行 117/119 加两个超时项成功复跑的拼接证据，
不是伪装成单次全绿。最后的源码差量在三工具链都完成全目标构建，并在三工具链
重新执行 core、language pipeline、projects、metadata 和线程压力门禁。

其他关键证据：

- FFI suite：29/29。
- MSVC native numeric pipeline：214 个检查、退出 0；ASan 无错误。
- MSVC ASan：thread runtime 20/20；struct value init、Span/view、property
  ref-return、receiver boundary 四个定向目标各 5/5，40 份最终日志中 0 个
  AddressSanitizer error marker。
- 提交前复验发现 `debug_library` 的低概率 teardown UAF；修复后常规构建连续
  100/100、MSVC ASan 5/5。并发 LSP 会话新增的 `debug_metadata` 编译 RED 不属于
  本次提交，未被改动、暂存或用于覆盖本轮 55 项结论。
- Syntax migration inventory 协议测试 5/5；55 份状态记录、55 行验收结果、
  55 行“确认”机器计数一致。
- metadata v2 golden：三工具链统一为 canonical 40-byte value slot、8-byte
  maximum scalar alignment，对应哈希 `0x11237ADD493636F1`。
- 没有残留临时 GC 诊断标记；纳入本轮验收范围的最终复跑进程均退出 0。

## Review 发现与修复

1. **P1，构建图不完整（已修复）。** Rust binding、AOT container smoke、LSP
   whitebox、Windows 长对象路径等目标缺少真实依赖或源文件。已补齐 include/link/source
   边并缩短 MSVC 对象路径，三工具链全目标构建通过。
2. **P1，AOT/GC 生命周期和类型安全（已修复）。** 修正 sentinel/raw type 验证、
   AOT 动态库延迟卸载与函数 pin、scalar-local 时序同步、frame/body/container
   lowering，以及 reflection fixture 布局重建。
3. **P1，模块初始化摘要 UAF（已修复）。** 递归 import 会扩容摘要缓存；循环现在
   每次递归后按 module 重新解析 summary，不再跨重分配持有内指针。
4. **P1，typed frame 的 GC 可见脏槽（已修复）。** 带 frame slot layout 的函数在
   pre-call 清零完整逻辑 `stackSize`，避免临时槽旧字节被 GC 当成对象指针；新增
   focused regression 15/15，并由 core/runtime 聚合门禁覆盖。
5. **P1，compiler pre-SemIR slot UAF（已修复）。** value construction materialize
   argument 时扩容 `preSemanticIrSlots`，此前继续读取 destination 内指针。所有可能
   跨 materialize/push 的 value construct、view/bounds、receiver/property-ref 路径改为
   值快照，写回前按 `stackSlot` 重取。ASan 原始 RED 稳定命中，修复后消失。
6. **P1，isolated-domain completion UAF（已修复）。** caller signal completion 后
   worker 可立即释放 launch，caller 却仍访问 provider runtime。worker-slot 计数现于
   发布 completion 前回收，signal/unlock 成为 caller 最后一次 launch 访问。ASan 与
   三工具链各 20 轮压力均通过。
7. **P2，metadata hash 宿主 ABI 漂移（已修复）。** Clang 的私有 union 大小与 MSVC
   不同，旧 writer 使用宿主 `sizeof`。metadata v2 改用 40/8 canonical layout 和专用
   schema validator，三工具链 golden 一致。
8. **P2，平台测试假设陈旧（已修复）。** Windows import thunk 不能与 DLL 内函数地址
   直接比较；reflection 改为实际调用 stored native function。Unix-only AOT 动态库执行
   被正确限定，Windows 仍验证生成 C contract；LLVM/private fixture 也同步新字段。
9. **P1，debug 测试函数图 teardown UAF（已修复）。** `debug_library` 返回对象可继续
   持有脚本闭包；旧清理助手先释放入口/子函数图，最终 GC 再扫描闭包时会读取已释放
   child function。清理路径现临时 ignore 入口函数，重置 VM 栈并立即完整 GC，随后
   unignore 并释放函数图。原始常规压力与 ASan RED 均稳定复现，修复后分别 100/100
   与 5/5 通过。

## 验收边界

本轮可以把这 55 份叶子状态记录作为一个已重新验收的集合：**55/55 confirmed**。
这不关闭根 Syntax 计划尚未实现的更高层切片，也不改变个别记录对历史 baseline 或
范围的限定。后续只有在根 `README.md` 的剩余 promotion gate 分别关闭后，才能声明
整个 Syntax redesign 完成。
