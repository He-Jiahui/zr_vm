---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_logpoint.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_logpoint.h
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_logpoint.c
tests:
  - zr_vm_debug_expression_diagnostics_test
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# 条件断点与 logpoint 当前合同

本批完整审查四个实际文件的9个旧单元：4函数、2接口、1所属语句块和2包含守卫。旧flat路径迁移到breakpoint子目录，保持单元身份和顺序，0新增信用。7项已注释，2个标准守卫无需重复注释；没有未审查单元或新BUG/TODO。新增6段Doxygen含Summary、真实参数方向、Returns、约束/生命周期与References，另只更正logpoint.c:75的一条过强旧注释。原211行的程序和字符串拼写、UTF-8列与EOL保持；绝对字节偏移可因该注释变长移动。有限正则核验不等于编译器AST证明。

条件入口的返回值表示求值是否成功，outSatisfied表示是否命中。有效空条件成功且满足；普通失败保留假满足位。非空条件固定frameId=1、allowedEffectFlags=NONE、allowLegacyCompatibility=false，不借旧parser越过canonical或capability拒绝。纯literal算式不依赖暂停帧元数据；引用表达式仍借用当前首帧事实。零能力不保证解析准备零分配，本层不建立TryRun或事务恢复点。

truthy helper按type读取初始化后的共享标量槽或对象指针，不执行用户转换；整数/浮点零为假，NaN比较沿C语义，string按指针而不按长度。agent/state、文本、帧和返回值的引用均在同步使用中借用，不延长VM对象寿命，也不新增并发或重入隔离。

logpoint按首个{与其后首个}配对，不支持嵌套或转义；未配对后缀作为字面文本。临时片段、表达式和总输出各自按字节容量截断，实际只求值复制后的表达式前缀。输出满后仍可能处理后续插值。append依赖已有NUL与合法snprintf不重叠前提。普通false插值变成有界错误片段并继续；Throw/abort不属于该普通返回保证。格式化true允许插值失败或截断，不证明完整错误标记、尾换行或事件已发送；发送由debug.c:1358的调用方负责。

调用证据覆盖4261个首方路径域中的当前检索、两个debug.c直接入口及trace observer→should_stop的间接路径。现有两测试分别验证比较/算式成功和数组字面量的分配capability拒绝。未运行小/满容量、NaN/全类型、UTF-8、嵌套模板、故障GC或非局部清理，也不把声明或函数名当作这些覆盖。

本批在同一实际checkout、物理E盘复用B和相同独占E/tests cwd运行前后验证。两个阶段build均退出0，完整56例每阶段55PASS/1FAIL/0IGNORE；本批condition/logpoint两例均PASS，有序原始结果和Unity footer一致，stdout/stderr全字节相同。不能宣称完整套件通过。既有失败为test_debug_evaluation_effect_policy_marks_resolved_property_getter：stdout报告test_debug_expression_diagnostics.c:484，真实断言位于其收录的test_debug_evaluation_effect_policy_cases.h:484，policy.hasCanonicalFacts为假。之后isPure与PROPERTY_GETTER断言未到达；本批不归因、不修复。旧Debug45执行记录只作历史对照，本批事实来自新before/after收据。

原CTest runner默认tests/cmake cwd，注册未设内部超时。本批直接运行无参数EXE，使用独立物理E cwd观察初末状态，build900秒/direct120秒仅为任务watchdog。实际两次初末cwd均空，无生成夹具残留；没有新建测试或复制源码树。

旧调试器使用文档docs/cli-and-tooling/zr-debugger-v1-launch-workflow.md的372–401行仍符合已核实的正式纯求值合同，本批无需更正。混合叶742行中只迁移所选9行，外733行和表头逐字节保留；inventory仅更新四源的哈希/状态/说明，外来49行86格保留。全局check既有stale前后同为1，整叶检查的既有外围问题原样记录；本批逐行有效性与语义复核单独验收，不宣称全仓结束。

本批外部配对证据：debug-breakpoint-expressions-current-native-pair-r1.json，SHA256 ea4f3be5547f656d3411d400c32f522199ab65ccc624a8a1432401376286a7c3；源码应用：debug-breakpoint-expressions-current-source-application-r1.json，SHA256 82c7f3e17819a8a0e842f719a779ff42b34a1e2761f79d116093fa5dc7a1a507。
