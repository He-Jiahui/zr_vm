---
related_code:
  - tests/core/ssa_runtime_objects_concurrency.c
  - tests/core/ssa_runtime_objects_concurrency.h
implementation_files:
  - tests/core/ssa_runtime_objects_concurrency.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_ssa_runtime_objects.c
doc_type: code-review-batch
---

# 对象物化并发收集辅助契约审查 — 2026-10-16

## 完整职责与平台入口

两文件22规范单元：C含14函数、4类型、1枚举和1特性宏；H含1接口与普通guard。Windows/POSIX同一职责保留双物理定义，合计27定义span、17函数体。21项职责有26份Doxygen，guard一项无需额外注释；明细coverage/tests_core_ssa_runtime_objects_concurrency.tsv，不重复记Objects137已经审查的35用例或故障替身。语句块的顺序与失败边界归入所属函数。

查阅两平台分支、线程入口函数指针、one-shot hook注册、AOT根frame、domain暂停和实际CMake目标。原型附接C595触发宏替身hook；字段C615的Gc_WriteBarrier调用来自不同翻译单元，名称中的field不构成字段屏障被替身截获的证据。_POSIX_C_SOURCE只在非Windows且宿主未定义时提供200809L，不强制覆盖宿主较低值。

## 时序、不变量和借用生命周期

调用方输入及输出存储在worker启动前建立三组外层根；额外RUNNING层保护发布至输出根更新的窗口。物化内部临时根先结束，再释放调用方根。独立worker持自己的附着state，context、frame、map、slots与请求数组必须活到worker完成且join结束。所有当前可检索正常与异常路径保持LIFO；不把任意外部破坏根链的假设标成已证缺陷。

worker的requested原子标志与domain的pauseRequested同时可见，只证明hook记录了该组合状态；domain暂停请求也可能来自物化自身。实际worker获锁、发生一次完整收集、观察materializationReturned以及最终join分别检查，不能把单一snapshot当成worker已拥有暂停。Windows的Interlocked与POSIX acquire/release分别解释，普通结果字段在join后读取。

## 失败与测试范围

FALSE可能发生在物化成功发布以后，例如输出根push、CloseHandle或最终观测失败；接口不承诺FALSE保持旧输出。请求先做有限形状检查，后续仍要求来自有效fixture的可读数组、GC有效对象与串行测试控制。calloc只属于本TU，Objects故障TU的替换宏不注入此分配。release记录Pop失败后仍释放slots的路径需要LIFO前提；未发现当前调用链可达的违反证据，本批无新增BUG或TODO。

名义1ms sleep和有限轮询次数不是精确时钟期限。Windows join等待失败会abort，CloseHandle失败为晚期FALSE；POSIX pthread_join本身没有超时。worker未完成时必须终止测试进程，避免Unity清理仍被后台线程使用的domain。辅助函数内不执行Unity断言，外层用例在其返回后再断言。

## 基线与验收

修改前复用上一批实际四目标运行中的Objects35程序：当次C/H原始raw与compile/frozen pins逐项匹配，build0、CTest及direct35通过；其他66例不重复授为本模块基线。实际运行Windows分支；POSIX分支只有静态阅读和调用证据，无POSIX/Linux运行信用。

两份源码的Clang检查已逐字消费lexer输出，确认2170个非注释token的kind、spelling、StartOfLine、LeadingSpace及原始行列均相同；原始字节前缀保持完整。修改后实际目标build0、CTest1/1及direct35全部PASS，35例完整输出与上一批Objects137最终运行中的Objects35逐例相同。该次运行75个冻结文件及3541个保守观测输入均稳定；两次运行之间9项输入变化和6项新增输入按实际哈希登记，包含其他会话改动，不能据此声称固定全checkout或stagedtree验证。Windows结果不授POSIX/Linux信用，CTest复跑同35例不重复计数。运行后批次说明仅更新本验收段，已测试代码保持不变。当前仍须独立核验实际source/leaf与最终暂存内容，串行提交仅纳两源码、leaf、批次说明和inventory两行审核列，共5路径。

## 产物

沿用同任务E盘登记的reports/build/cache/tmp和唯一自有备用索引，无源码副本和新增测试。生命周期open，释放0B；既有自动清理拒绝、共享/外部/未知归属与Git保留项不变。
