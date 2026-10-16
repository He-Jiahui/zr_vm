---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - tests/core/test_ssa_runtime_objects.c
  - tests/core/ssa_runtime_objects_faults.c
  - tests/core/ssa_runtime_objects_faults.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_ssa_runtime_objects.c
  - tests/core/ssa_runtime_objects_concurrency.c
doc_type: code-review-batch
---

# ExecIR 对象物化、故障替身及35场景契约审查 — 2026-10-16

## 完整单元与调用入口

四文件137物理单元：Objects生产实现28函数、2类型、1枚举、1宏，共32；测试主文件44函数（35用例与9个helper/fixture/runner）、10静态状态、1局部匿名union，共55；故障TU17函数、14静态状态、8替换宏及文本include块1，共40；故障头9接口及普通guard1。136项中文Doxygen，一项guard无需重复源码注释；明细coverage/zr_vm_core_exec_ir_materialize_objects.tsv。语句块随所属函数审查，测试和宏编译实例不重复计生产正文信用；并发辅助C/H是依赖和独立后续批次，本批没有将其标为已审。

从生产公开入口、公开头、StateMap/TryRun/GC协议与实际CMake目标交叉追踪全部可检索调用；测试宏只替换文本包含的Objects正文，其他linked翻译单元保持真实实现。35用例通过Unity的RUN_TEST函数指针派发；不把无直接外部caller的静态helper或callback当死代码。匿名union仅在对应用例内提供输出/owner witness别名反例，文档使用局部anchor。

## 图、不变量和失败状态

物化接收已通过上游逻辑MaterializeState验证的function/map/owner/value输入；本函数的有限形状、计数和别名校验不验证任意坏指针的可读性或完整分配extent。图validator读取VALUE配方所索引的function.values后才做其后的部分计数检查，不能作为独立的非法valueId防线。inactive owner跳过部分payload检查/根登记，不等于所有VALUE recipe都不读该槽。

配方图允许别名和循环，因此先分配全部对象，再附接prototype及字段；prototype继承链的fast/slow环检查另有约束，不能与配方回边混淆。成功只在末尾发布objects槽和count；空图分支只清count。失败保留输出槽的协议不承诺VM堆、被GC更新的输入/原型指针、ignored链表次序或全部对象flags逐字回滚。候选GC对象交runtime管理，C free只用于本模块native工作池。

工作数组有五个必需calloc及一个按需ignored快照，全部尝试后统一NULL判定；失败路径释放已取得的数组。临时根按成功前缀计数并以LIFO弹出，临时根先于MutatorEnter握手；保持RUNNING直到根、collection pause和native池清理结束，再恢复外层mutator状态。root group最多七组，数目不能外推任意将来版本。恢复ignored只恢复有限membership，不恢复完整registry字节。

callback在TryRun内执行，且仅当返回status不是FINE时清旧mutation token；并非任意Throw都触发此分支。TryRun允许Throw(FINE)后返回FINE，现有35场景未覆盖这种特殊status，不能据缺少用例直接标BUG。native scratch cleanup要求对应有效preparation，不承诺可重复释放或外层事务完整回滚。

## 故障替身及测试边界

RawObject_Barrier的宏替身只有生产C595原型附接处一个物理调用；C615调用Gc_WriteBarrier，内部Value_Barrier与真实RawObject_Barrier在各自翻译单元编译，不受fault TU宏影响。hook先复制并清全局函数指针，再同步调用借用context；正常后续才执行真实barrier，Throw可越过这次真实调用。context不自动清空或释放，须活到同步回调结束；TU-static配置不得并发修改。名称含field的并发场景并不证明字段C615已被截获。

native calloc故障计数只覆盖文本include的六个直接表达式，不覆盖linked logical/Owner/Liveness/Hash/GC内部申请。ordinal循环在首个未注入成功处停止，不能从循环上限或静态源码遍历外推精确命中数。capacity场景首先在请求检查C224拒绝，不给图容量C387分支信用；inactive poison样例没有VALUE配方引用被毒化槽；assert_graph只检查第0对象prototype/STRUCT tag及列出的有限边和字段，不给第1对象同等断言信用。

每例记录实际断言、注册行、首拒绝路径和局部资源边界；Unity断言长跳可以越过尚未执行的局部释放。tearDown处理全局fixture，不承诺每一个根、native scope或case临时分配在任意断言失败时都自动平衡。并发helper的等待、晚期FALSE和平台区别留给该独立模块的契约。

## 修订、基线与验收

保留生产原REVISE及测试原REVISE，不改写历史。生产32修订已由独审逐份接受；测试累计修订五份Doxygen及对应映射/台账：原型首hook、两个替身/宏调用归属、native OOM循环的永久措辞、field helper的非空使用前提。99份测试Doxygen及32份生产Doxygen原草稿逐字不变，guard纳入no-comment闭合且三处include入口完整记录，合并137提案另经独立审查后才取得本次写权。

在实际checkout、相同E盘Clang-cl/静态库配置下，修改前Objects目标build0、CTest1/1及direct35全部PASS；71冻结和3522保守观测输入在该次运行稳定。CTest再次执行同一组35例，不累计为70。并发场景运行的是当前Windows分支，没有POSIX、Linux、完整仓库或默认SDK验收信用。

四源与137行台账的实际内容已经独审接受。Clang逐字消费lexer输出，确认13,840个非注释token的kind、spelling、StartOfLine、LeadingSpace及原始行列均相同；四份原始字节前缀保持完整。修改后四目标build0、CTest4/4与直接运行35+42+15+9=101例全部PASS：Objects35与本批修改前35逐例输出相同，StateMaps42和Owner15分别与前批实际Owner61最终基线相同；Liveness9只记录当前原始源码基线，不声称已有修改前后比较。CTest重复同组程序，不另增用例数。该次运行73个冻结文件和3535个保守观测输入均稳定；前后两次运行之间14个输入变化、13个新增输入按实际哈希登记，包括其他会话变化，不能解释为全checkout隔离验证。源代码验证完成后，批次说明仅更新本验收段，未变更已测试的代码。最后仍须核验实际暂存7路径及三个cached-check；提交范围为四源码、leaf、说明和inventory四选定行的审核列。

## 产物

沿用同任务E盘登记的reports、build、cache、tmp和唯一自有备用索引；不复制源码，不新增仅重复实现的测试。证据按提案、REVISE/修订、独审、租约、strict/native及stage/commit分层保留。生命周期open，本批释放0B；既有清理自动拒绝、共享/外部/未知归属及Git元数据保留项不变。
