---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
tests:
  - zr_vm_debug_agent_test
  - zr_vm_debug_step_edges_test
  - zr_vm_debug_agent_protocol_test
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# 调试断点、暂停和单步控制合同

本批逐项复核67个旧单元：42函数、7类型、11声明接口和7所属语句块；64项已注释、3项保留同一同步TODO家族，无未审查项、无新增BUG/TODO家族或单元信用。完整叶742行仅更新同一67键的合同列，表头与外675行逐字节保留。源文件本批覆盖debug.c的46/90、debug_internal.h的7/89、公开debug.h的14/41；所外44/82/27项不获本批信用，三个inventory记录为pending，不能以本批子集接纳整文件。

ZrDebug_Pause仅把请求写入volatile槽，由VM下一trace观察点消费；不立即挂起、不等待确认，执行路径仍须产生trace。barrier/load/store的现有同步TODO在三行台账镜像保留，平台全栅栏不把volatile变成标准C原子，也不提供互斥或请求队列。TCP暂停用例不构成race或C内存模型证明。

断点规格文本在调用期间借用，提交函数复制到固定数组并可能截断。保存的resolved_function与指令偏移只借用仍存活的函数元数据，不持GC根。设置行/函数断点按类别替换并保留其它类别，不按source分组保留其它行断点；非零spec.kind仍覆盖fallback，调用方应遵守所需类别和可表示容量前提。替换先计数、分配、复制与解析，再free旧数组并发布，普通calloc失败保留旧集合；调用方仍须串行化集合访问，不把这个顺序称为并发原子、全事务或非局部回滚。

源码标识比较只作有限容量词法处理：反斜杠转斜杠，Windows折小写，裁尾分隔符；不解析真实文件、链接或上级目录。仅至少一侧像模块名时允许stem兼容，不证明全局唯一性。行断点在childFunctionList优先查子函数，函数断点先查当前根；入口常量表中的函数/非native closure另行补根比较非入口根和较窄跨度，但不声称遍历所有对象图。命中计数先增加再过滤，uint32解析及计数不检查回绕；条件仅支持实际有限阈值语法，不是一般表达式。

trace回调先轮询协议并补解断点，再按uncaught、caught、pause、entry、普通断点、data breakpoint、step选择一个原因。无客户端且不等待的启动分支清标志并提前返回。条件和日志格式化委托既有受限求值入口；文本发送、定位发布、计数变化没有共同事务或外围非局部恢复点。日志可被固定容量截断，发送不证明网络交付。

单步先保存借用帧、函数、偏移与逻辑深度，再清变量句柄和暂停请求并设置模式。into看位置推进；over区分更深帧与同深度换帧尾调用；out等待深度下降并避开异常仍在的非CATCH点。继续/单步请求不会自行执行VM、延长帧寿命或等待实际停止。ZrDebugStopEvent拥有内联文本副本，内部最近快照可被下一停点覆盖，恢复后旧变量句柄失效。宿主NotifyException应在状态/函数释放前调用，优先借用保存异常帧并可能阻塞于暂停循环；通知不执行Throw，也不拥有或释放异常对象。

保留7个准确现有意图块：模块名退化、子函数优先级、常量函数根、停止原因优先级、替代数组提交、暂停请求消费和单步恢复点。新增60段Doxygen只追加于EOF，分布为debug.c39、内部头7、公开头14；包含适用Summary、参数方向、Returns/值模型、约束与生命周期及References。三个原始LF前缀1945+448+340行共2733行全部字节保留，新增1208LF。有限正则分别核对10291/1990/1099个程序atom以及143/13/1个字符串/字符字面量的拼写、原行、UTF-8字节列和绝对偏移；不是编译器lexer、预处理或AST证明，也不因既有大文件做行为重构。

在同一实际checkout、同一物理E盘复用B执行既有直接入口。修改前后构建与三个EXE均真实await且退出0，有序17+4+8例均PASS/0FAIL/0IGNORE、footer/OK完整，原始stdout与stderr按每入口逐字节比较一致。没有新增测试，不将测试源、借用依赖或未覆盖故障路径授为本批审查信用。

BEFORE协议组越过本任务180s watchdog，驱动40116实际退出1、timed_out=true保持；终止辅助46728返回Access denied/1，原协议进程44752随后communicate自然退出0并完整输出8PASS。Root与独立reviewer仅按完整29条断言结果接纳有限修改前基线，不声称180s时限通过、驱动全绿或完全无干预。AFTER协议任务watchdog明确改为600s，原CTest没有本批显式timeout；这不是程序或CTest修改，也不比较前后180s耗时保证。原失败/超时记录不可改写。

本轮FileAPI17目标有限973源码、930首方.h include目录枚举与language_debug_gauntlet项目7个物理只读文件用于身份围栏；不是实际compiler依赖全闭包或SDK完整验证。独占cwd起始不存在/每阶段为空，唯一潜在输出debug_agent_binary_fixture.zro及其普通remove预先登记，实际阶段后快照为空；非局部断言仍可绕过普通清理，未知介入文件不得清理。无race/OOM注入、性能、WSL或全仓验收结论。

三个inventory只投影已核实源码哈希、pending状态与子集说明。原有外来48行85格、正常索引4个既有暂存路径和叶外675行保留。整叶及全局checker的实际结果、外围诊断与本批逐键格式/锚点核验分别记录；部分通过不等于全仓完成。

配对证据：debug-breakpoint-trace-control-current-native-pair-r1.json，SHA256 c8c6f59dd6fde2d7c055338033b231b00528d17396c210f40d2109d4aa4c0c92；源码应用：debug-breakpoint-trace-control-current-source-application-r1.json，SHA256 302aed6bf17cea63fa4f28ff6700429dd984f5e3c9898d2a0ee02896f80a7657。
