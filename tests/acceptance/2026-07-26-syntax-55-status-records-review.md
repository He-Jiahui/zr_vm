# Syntax 55 份状态记录重新测试、验收与 Review

## 结论

- 复验日期：2026-07-28 至 2026-07-29（UTC+08:00）。
- 清点规则：递归扫描 `docs/plans/syntax` 下名称以数字开头的子目录中的
  Markdown 文件，排除 `*-implementation-plan.md`。
- 清点结果：严格得到 55 份状态记录；55 份均自述为某种完成状态。
- 新鲜验收结果：**55/55 在各自声明的叶子范围内确认完成**。
- Review 结果：初审发现的构建图、属性/引用/所有权、AOT/GC、模块初始化、
  typed frame、metadata 跨平台、线程隔离域生命周期以及 debug 测试函数图销毁
  顺序问题均已修复；当前叶子范围内没有遗留 P1/P2 功能或内存安全缺陷。
- 整体 Syntax 计划：**仍未整体完成**。本轮另行关闭了 08 call spread、10F M3
  和 11 M1，但根 `README.md` 仍有 06B、07B、10C、11 后续里程碑、14 以及其他
  promotion 依赖未关闭。55 份叶子记录的完成不能自动提升整个 14 部分 Syntax
  redesign。

原始状态文本的写法并不完全统一：包括普通/反引号 `completed`、中文“已完成”、
带 M2/M3/M4 gate 限定的“已完成”，以及
`completed_with_known_baseline_failures`、
`completed_with_known_baseline_markers`、
`completed_with_known_unrelated_markers`。本报告保留这些历史限定；“确认”表示当前
工作树已用新鲜测试重新证明该记录声明的范围，而不是改写其历史状态文字。

## 2026-08-05 最终复验、旧测试清理与跨平台 review

本轮按当前 selector 再次机械清点：仅扫描 `docs/plans/syntax` 下名称以数字开头的
直接子目录，递归收集 Markdown，排除 `*-implementation-plan.md` 和独立 support
record `m5-task4-property-import-bootstrap.md`。结果仍为
`TOTAL=55 COMPLETE=55 MISSING_OR_NOT_COMPLETE=0`，目录分布为 01=5、02=6、
03=5、04=7、05=6、06=2、07=1、10=5、12=15、13=3。结论只确认这 55 份
状态记录各自声明的叶子范围，不把它们自动提升为根 Syntax redesign 全部完成。

生产 parser 静态扫描对 `%module/%compileTime/%extern/%test/%owned/%import/%borrow/`
`%loan/%unique/%shared/%weak/%func` 的字面量命中为 0。生产代码保留
`report_removed_percent_syntax` 是有意的拒绝边界：旧关键字进入 error 级
`legacy_syntax_removed` 后立即停止解析，不生成兼容 AST；migration frontend、负向
fixture 和历史文档中的旧拼写也不代表双轨。普通 `%` 与 `%=` 仍分别是取模和取模赋值，
不属于被删除的关键字语法。

旧测试清理删除了 compile-time suite 中 32 个从未注册的过时测试和对应 dead helper，
保留的 69 项注册用例均可达；debug/LSP fixture 不再期待已删除的 synthetic member、
index-window、property migration 或 ownership warning 兼容路径。LSP aggregate runner
现在累计失败并返回非零退出码，避免 `Fail -` 文本存在时进程仍以 0 假绿。CodeLens
fixture 改为真实 `#zr.testing.test#` typed TestManifest；CLI 保留 canonical case id，
生成单文件执行 harness 时只调用本地函数名。

跨平台 review 关闭了以下真实缺陷：Windows shared build 的 debug expression 与 LSP
interface 测试已链接正确但内部调用符号没有导出，现由组件 export macro 显式发布测试
所需接口；Windows child-process argument builder 的无符号递减下溢曾使启动提前失败，
现使用有界递减并给子进程继承 `NUL` stdin；断言失败被 `TryRun` 捕获后，worker 在释放
编译函数前重置 VM thread/call stack，避免 MSVC 下 `0xC0000005`；debug child-shape
用例删除旧 `_MSC_VER` argument 夹具，三工具链统一验证 local binding 与 formal facts。
另行修复了 resource cleanup 重复注册、REPL fact refresh、imported Span layout/provider
prototype 复用以及 AOT scalar latest-write provenance。

最新验证证据如下：

- Windows MSVC 19.44 Debug shared build 完整 CTest 为 **126/126**，0 失败，
  547.06 秒；其中 `testing_reference`、`language_server`、`debug_expression_diagnostics`
  和 `debug_variable_child_shape` 均在同一次全量运行中通过。
- WSL GCC 11.4 Debug 完整 CTest 基线为 **128/128**；本轮最终增量对严格 percent、
  CLI migration/testing、LSP 和 debug 的 8 个高风险入口复跑为 **8/8**。
- WSL Clang 14 Debug 完整 CTest 基线为 **126/126**；同一最终高风险矩阵复跑为
  **8/8**。
- Syntax migration inventory 协议测试为 **9/9**；当前仓库扫描结果为
  `SCANNED=902`、`EXCLUDED=456`、`ALLOWLISTED=14`、
  `REVIEWED_CURRENT=597`、`FINDINGS=0`、`UNKNOWN=0`，所有 finding 分类均为 0。
- MSVC 全目标构建在修复 export 边界后成功完成；两个 Copilot 报告的 LNK2019
  不是漏写 `target_link_libraries`，因为目标原本已链接对应 import library，真实原因是
  DLL import library中缺少未标 export 的内部测试接口。

因此本轮验收结论为：55/55 状态记录在声明范围内完成，严格 `%keyword` 破坏性切换
保持完成，清理后的当前测试集在三工具链证据下成立；未发现仍需阻止本次提交的 P1/P2
问题。根 Syntax redesign 的上层 promotion 状态仍以根计划自身记录为准。

## 2026-08-02 严格切换复验与最终 review

本轮再次按同一叶子规则机械清点：排除 `*-implementation-plan.md` 和单独的
`m5-task4-property-import-bootstrap.md` support record 后，结果为
`TOTAL=55 COMPLETE=55 MISSING=0`。目录分布为 01=5、02=6、03=5、04=7、
05=6、06=2、07=1、10=5、12=15、13=3。

用户列出的旧 production parser surface 已逐项复核。`%module`、`%import`、
`%extern`、`%compileTime`、`%test`、`%owned`、ownership `%` builtins/type
qualifiers 和 `%func` 只由 `report_removed_percent_syntax` 识别，以便产生 error 级
`legacy_syntax_removed`；statement/expression/type 入口随后返回 `NULL`，不会生成旧
AST。`native extern`、`comptime`、`import(...)`、`fn(...) -> R`、canonical ref
TypeRef 与普通 `%`/`%=` 运算保持当前语法。internal intermediate text 中的 `%`
分隔符不属于 source keyword。

全新 WSL GCC 11.4 Debug 隔离快照的本轮定向结果为：parser 74/74、percent cutover
6/6、reflection surface 18/18、module system 78/78、project manifest v2 10/10、
comptime runtime 14/14、external source provider 8/8、CLI project incremental
12/12，LSP advanced editor features 0 failure。CLI cache 用例明确证明首轮 miss、
二轮 hit、同长度语义修改 miss、损坏
snapshot rejection/repair；首次与 hit 产物相同，语义修改产物不同，恢复原源码后再次
逐字节复现首次 `.zro`。

独立 review 发现并关闭六项问题：production parse 曾在 removed syntax 后返回恢复 AST；
cache key 曾缺少源码语义摘要；snapshot 曾缺少全内容完整性摘要；TypeId lookup 曾可能把
相同文本身份的另一 numeric ID 对象返回给调用者；typed-ref callable fallback 曾只填
`parameters` 而遗漏 `parameterModes`；formatter migration scan 曾把相邻模运算误判为
`%keyword`。修复后，removed syntax 的公共 parse 入口释放 partial AST 并返回 `NULL`，
parser 回溯 cursor 同时恢复 fatal 状态；cache v5 认证源码和完整 snapshot；TypeId 冲突
fail closed；callable 两套 mode 投影同步；spaced/adjacent 模运算均保留。六项原失败证据
均在同一隔离快照复跑转绿。

最终 review 又关闭一项 P1：snapshot test 曾直接调用 parser shared library 未导出的
`MixValue`/`Lookup`/`Store` 内部符号，MSVC 共享库配置因此出现三个 `LNK2019`。测试现
直接构造公开 compiler cache entry，并只经公开 Export/Import/Free API 验证 byte-stable、
完整性和事务性。MSVC 19.44 Debug 共享库目标成功链接并运行 14/14；WSL GCC 同目标也为
14/14。

本次追加 provider review 又关闭四项边界问题：function access modifier 曾被 parser
消费后丢弃；部分导入失败只释放 pointer array；重复 provider alias 失败后可能遗留
binding；传递 provider 图未晋级时 provider source 仍可能递归激活 build dependency。
当前仅 `pub`/`pro` transform 对 consumer 可见，private helper 保持 provider-local，
失败路径完整回滚；传递 build-dependency import 在递归准备前以
`compiletool.provider.transitive_not_promoted` fail closed。GCC、Clang、MSVC 均为
provider 8/8。

clean intended snapshot 的 syntax migration inventory 协议为 9/9；最终报告为
`machineApplicable=0`、`maybeIncorrect=0`、`blocked=0`、
`targetNotPromoted=0`、`unknown=0`。649 条 `requiresReview` 均为保留的
文档/测试语义分类，不是 production parser 兼容规则；6 条 allowlist 是精确的
删除/未知语法负例。benchmark source generator 中最后一条可执行旧语法已迁到
`import(...)` 与 `fn`。

严格切换还暴露并关闭了旧 fixture 的假绿：compiler integration 的自定义 `ZR_TEST_FAIL`
会提前返回但被 Unity 记为 PASS。正向 fixture 已全部迁到 `fn`、`init`、`resource class`、
`own`/`share`/`weak`/`ref`/`intoGc`、fully-qualified Iterator 和 canonical `const`/`comptime
fn`；负向 fixture 改为验证当前真实拒绝边界。导出全局 reference 校验也同步识别新
`referenceAccess` 表示。最终 compiler integration 为 127/127，附加自定义 Fail 为 0。

最终 WSL GCC 11.4 Debug 隔离树全目标 Ninja 构建成功。焦点矩阵保持 parser 74/74、
percent cutover 6/6、reflection 18/18、module 78/78、manifest 10/10、comptime 14/14、
provider 8/8、CLI cache 12/12、LSP advanced 0 failure。完整 CTest 为 121/126；
`language_server` 已通过。五个残余失败是已知上层基线：`language_pipeline` 在
60 秒 suite 上限前暴露 4 条既有 AOT C source-contract 文本断言失败，另四项为本次
隔离树未带入并行 Debug 工作的 `debug_agent`、`debug_truncation`、
`debug_variable_child_shape`、`debug_library`。这些失败不属于 55 份叶子或严格 parser
切换的完成证据，也继续阻止宣称根 Syntax redesign 全绿。

结论仍保持三层分离：55/55 只确认历史叶子记录声明的范围；严格 production parser
切换已完成；root Syntax redesign 仍因 07B 的 13 个 design-pending 项，以及
08/09/10C/11/14 的剩余 promotion 证据而未完成。

## 2026-08-03 callable contract review 补充验收

同一机械 selector 再次得到 `TOTAL=55 COMPLETE=55 MISSING=0`，目录分布仍为
01=5、02=6、03=5、04=7、05=6、06=2、07=1、10=5、12=15、13=3。严格百分号切换
定向测试保持 6/6：旧 `%module/%compileTime/%extern/%test/%owned/%import/%borrow/%func`
等拼写只进入 fatal `legacy_syntax_removed` 诊断，不再产生旧 AST 或 lowering；算术 `%` 与
`%=` 保持有效。

本轮 review 重新打开并修复了 Gate 10F 的一项实质问题：旧 schema 把 callable hash 直接
等同于 ABI signature hash，且类型准入包含具体名字 blacklist。schema v4 现持久化独立的
canonical callable vector，并通过 `.zro`、C AOT、LLVM AOT 传播；aggregate/union layout
由 canonical `SZrTypeLayout` 验证，类型准入改为 capability/layout facts。WSL GCC 11.4
与 Clang 14 的新鲜定向证据均为 native extern 29/29、AOT C stripping 37/37、percent
cutover 6/6；MSVC 19.44 同为 29/29、37/37、6/6，其中一项 Unix-only LLVM runtime
loading case 按平台忽略。

该修复加强了 10F 叶子完成证据，但不改变验收边界：55/55 仍是叶子记录结论，root
Syntax redesign 仍因 07B 的 13 个 `design-pending` 项与 08/09/10C/11/14 剩余 promotion
证据保持未完成。

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

上层 Gate 结论保持保守。后续 2026-07-29 复验已另行完成 08 call spread、10F M3
和 11 M1，但 11 根 Gate 与 14 尚未关闭，因此依赖它们的 10C、06B、07B 仍不能
宣告完成。

## 2026-07-29 最终提交前复验

提交前再次从当前 `HEAD` 和工作树执行独立清点，仍严格得到 55 份状态记录：
目录分布为 01=5、02=6、03=5、04=7、05=6、06=2、07=1、10=5、12=15、
13=3；状态分布为 `已完成`=23、`completed`=29，以及三种带历史限定的
completed 状态各 1。缺失状态 0，非完成状态 0。

仓库迁移 inventory 首次复跑为 4/5：失败项仅为当前已跟踪计划文档与旧 golden
不一致。按当前 Git 跟踪集合机械重建
`tests/fixtures/syntax_migration_inventory/expected/repository-inventory.json` 后，协议
测试复跑 5/5。同时将 Python 协议对 `%type` 和动态 `$Type(...)` 的旧断言同步到
生成器及 C 端测试已经采用的 Gate 08 分类。golden 新增 41 个 `historicalPlan`
排除项和 25 个已扫描文件条目；findings 仍为 769、unknown 仍为 0，分类计数及
目标计划计数均未改变。

使用 Windows MSVC 19.44 Debug 重新构建 7 个定向目标，452 个 Ninja 步骤完成并
成功链接。随后直接运行对应可执行文件，合计 160/160 通过：reflection surface
18/18、reflection stress 3/3、generational pool 9/9、legacy migration 9/9、module
system 89/89、thread runtime 25/25、syntax reference v1 7/7。

随后使用 WSL 短路径隔离镜像完成新增 Gate 的新鲜复验：08 call spread 聚合 53/53，
10F native extern 27/27、FFI 29/29、pin 2/2、AOT call 5/5、frame 1/1，11 M1
compile-time 61/61、ref struct 11/11、reference escape 13/13、task 17/17。Gate 08
实现与测试已纳入本次提交，不再是未提交实验。根 promotion gate 的其余未完成边界
保持不变。

最终独立 review 又关闭两个遗漏：AOT record append 的 OOM 分支现在释放四个 owned
路径字符串；模块顶层 `return/if/while` 现在按 runtime phase 检查 CompileTool 使用，
新增 Windows 定向回归后 compile-time suite 为 61/61。Windows ASan native extern
为 27 项、0 失败、2 项明确忽略；其中 errno 项因静态 CRT 的 DLL 私有状态不适用，
该策略仍由 WSL 与动态 CRT MSVC Debug 覆盖。隔离提交树中的 migration inventory
协议为 5/5；新版 LSP 零遗留断言依赖另一会话尚未提交的 fixture 迁移，未混入本次提交。

最终隔离索引冷构建还确认了三个提交边界遗漏并完成 RED/GREEN 修复：LLVM AOT 补齐
`RESET_STACK_NULL{,2}` 与 `PROPERTY_REF_CREATE_LOCAL` lowering 后 native extern 从
26/27 恢复为 27/27；当前 `import("...")` 解析入口与具体 compile-time 诊断文本纳入后，
Gate 11 从 49/61 恢复为 61/61。干净 `HEAD` 本身仍缺少 pool 工作线的 contract role 和
stable-slot protocol 枚举，导致 `pooling.c` 无法独立编译；验收快照只覆盖这两个未提交
基础头继续验证，本提交不混入该旁支工作。

## 2026-07-30 原子切换复验

本次按用户要求执行生产 parser 的一次性破坏性切换。生产目录静态扫描对
`%module/%import/%extern/%compileTime/%test/%owned/%borrow/%loan/%unique/%shared/`
`%weak/%func/%in/%out/%ref/%using` 为零命中；这些拼写不再拥有生产解析路径，只能
出现在 migration 输入、负向测试或历史设计文档中。普通 `%` 取模运算保持有效，未知
`%identifier` 按普通语法错误处理，不会自动成为 migration rule。

同时删除了旧 `%test` 的专用 AST、compiler result/test-function 旁路、runtime
reflection test metadata 与中间文本 `TESTS` section。二进制函数格式保留一个恒为零的
旧字段 tombstone 以维持后续字段对齐；读取到非零旧 test metadata 时明确拒绝，避免把
旧制品静默解释成新语义。未来 Gate 14 测试发现必须使用普通 `fn` 加 typed
`TestManifest`，不得恢复 parser 特例。

本轮重新机器清点仍严格得到 55 份状态记录，目录分布仍为 01=5、02=6、03=5、
04=7、05=6、06=2、07=1、10=5、12=15、13=3，完成标记缺失为 0。因此 55/55 的
结论仍只适用于各记录声明的叶子范围。根 Syntax 计划不能据此宣告完成：Gate 14 的
typed `TestManifest`/test host/runner 尚未实施，06B/07B 等 promotion 依赖也未全部
关闭；当前 compiler integration 与 module system 上层套件仍暴露尚未迁移的旧 fixture。

切换后的新鲜定向证据：Windows MSVC Debug 构建成功，parser 74/74、percent cutover
4/4、legacy migration 10/10、compile-time current surface 29/29、named arguments
10/10；WSL GCC Debug 构建成功，parser 74/74、percent cutover 4/4、legacy migration
10/10。两边合计执行 215 项定向断言，0 失败。MSVC 对 `io.c` 仍报告两个既有的
`ZR_NO_RETURN` 后不可达代码 warning，本次新增拒绝分支不再产生额外 warning。
从只包含本次 25 个暂存文件的独立 Git index 导出短路径干净树后，MSVC 冷构建同一组
五个目标成功，提交态再次执行 127/127 通过，证明结果不依赖工作区其余未暂存改动。

上层复跑不是全绿：compiler integration 为 127 项、44 失败，module system 为 91 项、
43 失败。失败集中在旧 fixture、旧 function/type/decorator surface、尚未晋级的 compile-time
metadata 迁移及依赖它们的 project/module roundtrip；这两组结果是根计划未完成的验收
阻断证据，不应被定向 parser 全绿覆盖。

本次 review 还修正了二进制 tombstone 的宿主宽度风险：writer 与 reader 均固定读写
`uint64`，避免 32 位宿主按 `size_t` 读取导致后续字段错位；并确保 migration-only split
property parser 在 collection append 失败时释放 AST，不能回落到生产 AST。

## 2026-07-30/31 最终隔离复验

在继续迁移旧 fixture、修复下游 AOT/debug 回归并补齐 Gate 11 typed contract 后，
上述 compiler integration 44 项失败和 module system 43 项失败已经全部关闭。最终证据
不是用定向 parser 结果覆盖旧失败，而是重新从当前工作树同步到 WSL ext4 短路径隔离
源码树，重新配置、构建并串行执行完整注册矩阵：

- Ubuntu 22.04 / GCC 11.4 / Debug 冷构建完成 3176/3176 个 Ninja step；最终源码同步后
  CMake 重新配置和全目标增量构建退出 0；
- CTest 单次完整运行 123/123 通过，0 失败，总耗时 573.17 秒；其中
  `language_pipeline` 在完整运行中再次通过，独立修复后复跑也为 1/1、216.34 秒；
- 先前全量基线暴露的 7 个失败点全部闭环：CLI AOT writer、AOT descriptor
  diagnostics、两个 generic AOT、debug truncation、debug variable child shape 和
  `language_pipeline`；
- debug 注册组 18/18 通过；Gate 11 compile-time/attribute/comptime runtime/
  declaration transform 定向合计 52/52 通过；
- `percent_syntax_cutover`、`cli_syntax_migration`、`legacy_migration` 为 3/3；生产
  parser 对 16 组已删除 `%` 关键字字面量静态扫描为 0 命中；
- migration inventory 当前报告为 `machineApplicable=0`、`unknown=0`、
  `requiresReview=610`、`blocked=3`，878 个文件被扫描，412 个按结构化原因排除。
  三个 blocked finding 是 `%future`、`%mutex`、`%atomic` 的明确负向覆盖。

复跑过程中额外发现一份 CFG throw-effect 测试仍使用旧 `(...) -> { ... }` lambda。
该 fixture 改为 `fn(...): ReturnType { ... }` 后，原 CFG 可达性断言 2/2 通过，完整
`language_pipeline` 随后全绿。旧 decorator 正向测试也已改为生产 parser 拒绝测试，
不再把 `%` 输入当作成功路径。

最终机械清点仍为 `TOTAL=55 COMPLETE=55 MISSING=0`，目录分布保持
01=5、02=6、03=5、04=7、05=6、06=2、07=1、10=5、12=15、13=3。该结论只
确认 55 份历史叶子记录的声明范围。Gate 11 M4/M5 仍缺完整 typed Patch、事务回滚和
全部 consumer，Gate 14 仍未实现，08/09/10C/06B/07B 的上层 promotion 也未全部关闭；
因此根 Syntax redesign 继续保持未完成。

## `%` 语法的当前状态

原报告中“06B 前兼容输入仍由生产 parser 接受”的描述已被本次原子切换取代。当前
准确边界是：

- 生产 parser 不再接受已列入迁移表的 `%` 关键字，也不再接受与其绑定的旧语义旁路；
- 显式 legacy migration frontend 仍可识别旧输入并生成诊断/编辑方案，但不会把旧 AST
  交给生产 compiler；
- negative tests、migration fixtures 和历史文档保留旧拼写是必要的验证材料，不表示
  双轨兼容；
- `comptime var` 也不是 `%compileTime var` 的新拼写。设计要求按语义迁为 `const` 或
  comptime block local，不能做机械 token 替换。

所以生产 parser 继续接受上述 `%` 关键字并不正常；本次切换后，该兼容面已经关闭。
55 份叶子记录保持确认，但整个 Syntax redesign 仍未完成。

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
- Syntax migration inventory 协议测试最终复跑 5/5；55 份状态记录、55 行验收结果、
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
10. **P2，AOT record append OOM 路径泄漏（已修复）。** record array 扩容失败时
    现在除关闭 library、释放表并解除 GC pin 外，也释放 `moduleName`、`sourcePath`、
    `zroPath` 和 `libraryPath` 四个 owned 字符串。
11. **P1，模块顶层 CompileTool phase 旁路（已修复）。** module declaration list 与
    runtime phase 已拆开传播；顶层 `return`、`if`、`while` 使用 provider 均稳定报
    `compiletool.phase_mismatch`，Windows compile-time suite 61/61。
12. **P1，LLVM AOT native extern 注册路径缺少 lowering（已修复）。** 隔离提交树稳定
    复现 opcode 227 unsupported；补齐 stack reset 与 local property-reference lowering 后，
    WSL native extern suite 从 26/27 恢复为 27/27。
13. **P1，Gate 11 当前 import 与诊断传播未进入提交边界（已修复）。** `import("...")`
    现与 `%import` 兼容入口共同解析，`zr.import` 保持拒绝；compile-time 错误保存具体消息
    而不是通用摘要，WSL compile-time suite 从 49/61 恢复为 61/61。

## 验收边界

本轮可以把这 55 份叶子状态记录作为一个已重新验收的集合：**55/55 confirmed**。
这不关闭根 Syntax 计划尚未实现的更高层切片，也不改变个别记录对历史 baseline 或
范围的限定。后续只有在根 `README.md` 的剩余 promotion gate 分别关闭后，才能声明
整个 Syntax redesign 完成。
