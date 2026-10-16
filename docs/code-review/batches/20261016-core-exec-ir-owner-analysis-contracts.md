---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/parser/ssa_owner_fault_allocator.c
  - tests/parser/ssa_owner_fault_allocator.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/parser/test_ssa_conditional_cleanup.c
doc_type: code-review-batch
---

# ExecIR owner 状态分析与 ownership 测试契约审查 — 2026-10-16

## 完整单元与入口

本批五文件61个物理单元：实现12函数和2宏；公开头1结构、5接口、1状态位宏及普通guard1；ownership测试20函数（4 helper、15用例、main）；故障注入TU的4静态状态、7函数、3替换宏、1文本include块；故障头3接口及普通guard1。59项补中文Doxygen，2个guard无需重复源码注释，明细为coverage/zr_vm_core_exec_ir_owner_analysis.tsv。语句块随所属函数审查，无新增BUG/TODO，不改API、执行语句、断言、构建和测试注册。

逐符号核对全部可检索引用，并区分直接调用、头声明、宏展开与文本include。生产OwnerC在fault TU经分配器替换再次编译，仍只计同一物理定义；不把编译实例当新单元。公开分析由验证器、StateMap构建/物化、cleanup passes等消费，头include和CMake源码列表共同核验；不从缺少某类直接调用推断停用。已有共享fixture不重复计数。

## 分析不变量与调用时机

Build要求function/diag/analysis彼此满足独立存储约束，analysis须为空或先Free；入口清零并不释放旧池。借用输入的CFG、指令、值表须通过约定形状验证且分析期间稳定。分析只保留计数和三个自有池，不保存function指针或IR版本戳；查询者负责与同版IR配对并串行访问。

每个可达位置保存owner may-state集合，固定点合并全部CFG路径，不依据常量裁剪。PHI读取前驱edgeRow并写独立phiRow，避免同一PHI序列互相污染；PHI或COPY不能恢复已消费或未初始化的来源。MAY_THROW且TERMINATOR时第一个successor表达正常结果，其余边视结果为UNINITIALIZED。UNKNOWN、INITIALIZED、MOVED、DROPPED、UNINITIALIZED各占状态位，CONDITIONAL不是单一具体状态。

Build先设计数再验证容量：溢出失败保留计数而三个池为空，不能称全部归零。零指令可成功且不分配。非零路径包含三个输出池及五个scratch，共八处分配表达式，统一NULL检查前均会尝试；OOM清理已获得的输出和scratch，成功保留三池并释放scratch。Free可重复用于零态，释放后清零。无效或不可达查询返回mask0，OwnerStateAt相应返回COUNT；恰好单一具体状态位返回该状态（包括UNKNOWN），其他非零掩码返回CONDITIONAL。这些查询不是payload活性或实际GC根证明。

ConditionalCleanupValid核对指令形状、来源定义及第一个包含块的CLEANUP属性，不证明支配关系、运行期payload或所有图合法性。上层的验证、owner分析和候选提交分别承担约束，不能把外层事务、对象保活或全部验证归给本接口。

## 测试、故障注入与资源边界

15用例静态注册在test_ssa_state_map_ownership.c:380–394，main及每个helper均有单独台账；每例只记录实际断言与完整追踪的首拒绝分支。三个仅构建/Verify/BuildStateMaps的用例没有临时MaterializedState或assert helper；其图、值、StateMap和附属池由全局function交tearDown清理。其他用例按自身实际临时对象说明，Unity长跳可越过尚未执行的局部释放。

故障TU只替换文本include内的malloc/calloc/free，其他linked TU分配不在其计数范围。control重置注入ordinal、尝试数和failed标志，但不清outstanding，也不释放内存；0禁用注入。成功分配才增加outstanding，原生分配器自然失败不设注入failed。free要求属于同一hook的匹配分配，计数不是指针注册表或全进程泄漏检测。配置和计数为TU-static，要求串行；不同可执行程序有各自实例。

两个OOM用例枚举ordinal至首个未注入成功，返回后先关闭注入再断言；验证outstanding0、OOM诊断及有限旧map/root或value/root字段、指针保持。它们没有证明精确八个ordinal的运行覆盖、全部池逐字相同，或resolver非空witness路径自己的liveness分配失败。test_ssa_conditional_cleanup只是本批依赖证据，未重复其单元信用。

## 基线、修订与验收范围

独审原Ownership38提案曾拒绝三个用例的模板资源说明；原REVISE报告不可变保留。Root合并后精确修改六份文档（三枚举表述、三用例清理契约）及相应三行约束， corrected独审cb5bab26接受59份文档与61行，不把其接受扩大为源码、运行、暂存或提交验收。

在实际checkout、相同E盘构建配置下，修改前ownership目标已build0、CTest1/1、direct15全部通过；66个冻结输入和3519个保守观测输入在该次运行稳定。CTest重复同一15例不另计。原始五文件raw SHA与提案、独审及实际BEFORE输入逐项相同，没有源码副本或换树基线。

五源原始字节前缀完整保留，仅在EOF追加59份文档。Clang原始词法检查核实7169个非注释token的种类、拼写、StartOfLine/LeadingSpace及全部原行列一致；五文件共十个前后lexer进程均正常退出。独立实际source/leaf报告31bfab86接受59份Doxygen、61行、全部入口/宏/文本include与475个唯一锚，批次说明的单一UNKNOWN状态查询表述也按实际实现修正。

修改后复用同一实际构建目录和配置，五目标build0、CTest5/5，直接ownership15、state_maps42、SCCP14、retained8、LICM29，共108个场景全部通过。逐场景完整结果行分别等于本批立即修改前ownership15及前批Owner7最终四目标93的实际输出；CTest重复同一程序不另计。最终运行66个冻结输入及3521个保守观测输入保持稳定。修改前后间隔除五文件注释外，外部tests/CMakeLists.txt及两个string-format-OOM输入发生变化，均逐SHA登记；未宣称整个checkout隔离。

上述BEFORE/AFTER摘要分别为c394cb3d、da1d605e，strict为9e179cfc；完整SHA和日志保留在同任务报告根。只有五源码、本批leaf/说明及inventory五选定行的审核列，共8路径可进入提交。Root核查实际暂存差异和git diff --cached --check，独立reviewer再核最终批次和暂存内容；提交后检查路径、blob、parent、He-Jiahui身份和外部索引/工作树保留，才授61项历史交付信用。其他会话改动、原REVISE、失败证据及配置差异保留，不宣称全仓、固定checkout、独立暂存树、Linux或默认SDK通过。

## 产物

同任务E:/cargo-targets/zr_vm/reports/first-party-review-01a13463保留原提案/独审、修订提案、Root租约、BEFORE与后续strict/native/stage/commit证据，临时工具及构建复用E盘既有登记目录。无新增测试或源码复制；生命周期open，本批释放0B，既有清理自动拒绝及共享、外部、未知归属保留项仍在。
