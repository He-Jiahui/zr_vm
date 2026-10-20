---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
tests:
  - tests/debug/test_debug_data_breakpoint.c
  - tests/debug/test_debug_truncation.c
  - tests/debug/test_debug_agent_protocol.c
  - tests/cmake/debug-runtime-targets.cmake
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# 调试文本与数据断点合同

本批完成 debug.c 剩余29个既有审查单元：22函数、7个随父函数审查的有意义语句块。追加22段中文Doxygen，包含职责、使用时机、前提、必要约束、资源所有权与生命周期、54个方向参数、19个非void返回及3个void完成状态和References。台账结论25commented、2既有TODO镜像、2既有BUG镜像，0新增单元或问题家族。此前Trace与Agent的61项逐行保留，与本批互斥且覆盖本文件全部90项（72函数18块），inventory据此标为reviewed；没有把支持文件计为已完成。

文本辅助函数为当前协议展示提供定长拷贝、截断标记、函数名称与来源、枚举名称。含目录分隔符只是保留文本尾部的启发式，不验证路径存在性或安全性。NULL输入、零容量、小容量、普通文本和路径分支分别解释，返回成功或调用结束不意味着名称完整。function_name或source的回退依实际分支，取自VM的字符串和函数对象均为借用，不能因转成native指针而延长有效期。UTF-8按字节截断的既有TODO仍需核查协议对多字节字段的要求；本批ASCII场景不替代该确认。

数据断点先在暂停帧的元数据与当前Core槽位之间重解析local或upvalue，再建立比较基线。frameId、scopeId、localIndex及upvalueIndex只对当前停止视图有效，不能作为跨resume稳定身份；一基活跃变量枚举与实际存储槽不可混用。GetLocal可能物化值、分配或Throw，FALSE/NULL与非局部退出分别处理。临时线程访问需要成对结束，但正常尾部恢复不保证Throw之后也恢复。解析失效时保留旧基线并跳过当前检查，不推导条目自动移除或所有写入事件都被观测。

dataId为当前实现的有限文本格式；sscanf接受已匹配字段后的尾随内容，不是严格完整语法验证。info在有效输出条件下返回TRUE仍可能因小容量截断dataId；独立复核已将原稿该返回承诺收窄。只有容量为零时允许空description输出。安装data breakpoint前重解析初值，再提交新条目；realloc可能使内部条目指针失效，调用方不可跨增长保存它们。清理器释放自有C数组，浅拷贝的last_value不产生VM所有权或新的GC root；VM及被引用对象的有效期仍由上层保持。

当前readWrite路径检查CompareDirectly的值变化，不提供读取事件，也不等于完整写入追踪。观察者按现有停止优先级消费命中。深帧scope算术编码进入动态handle区间的既有BUG保留函数/块两镜像：Core没有99帧上界，ReadScopes的编码在frameId>=100时可能达到1000，ReadVariables优先走动态句柄分派，无匹配会拒绝或碰巧读取其他句柄。完整静态路径已连接，不包含本批深栈运行复现或行为修复。

独立复核提出14项严格typed元数据修订，涉及实际契约及派生后缀针；原F与I保持不可变，effectiveF及Root修订单独存证。29行仅修正同实体既有记录，first8契约与effectiveF一致，运行及已应用事实补充到第9列；没有新增或迁移审查身份。所有其他713行raw保持，其中此前61项全部逐字保留。

实际应用在128791B2992LF的当前已提交源码后追加26888B558LF，结果155679B3550LF；原前缀含Agent的既有合同。11357程序atom和143字符串/字符字面量的拼写、绝对UTF8偏移、物理行、UTF8字节列四元组相同，剥除后缀精确逆回批前raw。这是有限正则投影，不是编译器lexer/AST或运行时等价证明。原F的23个有限whole支持输入与74行锚、独审7个补充输入，以及已提交Snapshot42+Frame39的受控EOF组合桥均据实际针核验，借证不扩展整模块信用。

复用实际checkout的E盘clang-cl构建目录，前后各构建一次，无参数直接运行data_breakpoint2、truncation5及agent_protocol8。每阶段15PASS/0FAIL/0IGNORE，八个native子进程全部await退出0，三套stdout/stderr逐字节相同，无归一化；共15个不同既有用例、30次PASS实例。当前FileAPI守卫15个递归target、965源文件、922首方include头文件；仅本批受控注释后缀逆投影，其余有限输入逐字保持。900秒build/600秒test是本任务watchdog，均未触发，不是现有CTest显式TIMEOUT。

现有用例提供local/upvalue变化、文本截断、TCP能力/manifest/暂停/求值/句柄失效/断连与重连回归证据，不覆盖22函数所有分支、严格dataId解析、多字节协议要求、OOM/Throw注入、Unity长跳转清理、并发、GC、WSL或全仓通过。历史diagnostics56中的getter失败未在本批重跑，也未修复。

独占测试cwd在两阶段开始与结束时均为空，未生成额外夹具；正常socket/thread/VM尾部清理不证明所有早期失败都能清理。提交后仅按精确hash移除本批自有临时索引与空cwd，保留共享构建、缓存、其他会话产物和必要失败证据。未创建源码副本或新增测试。

全仓510叶台账43979个完整唯一键总量保持，无重复。inventory为1801 reviewed/2460 pending，48个外来inventory行85格及4个外来暂存路径保留。单个文件完成不代表全仓验收。

实际配对证据：debug-text-data-breakpoint-current-native-pair-r1.json，SHA256 6a79dde142b59c1a511d660998b859887bfff6cbd718cf9c5d4bd783485c4820；源码应用：debug-text-data-breakpoint-current-source-application-r1.json，SHA256 e09ccde9150c6ccc39e18992d257158aac821696ee825dcd1a82c31cec23a2b1。
