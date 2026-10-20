---
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/profile.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c
tests:
  - tests/profile/test_profile_deterministic.c
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# Profiling 聚合、采样与 hook 生命周期合同

本批完整复核 profile.h 的19个和 profile.c 的35个既有单元，共54项：45项已注释、6项TODO、3个宏无需重复注释，无未审查项。54行沿用原有完整键，0新增信用；742行混合叶的外688行逐字节保留。原包含保护和两个文本容量别名由结构字段与 copy_text 的合同承接，不为它们重复增加Doxygen。

Profile 通过 Core 的 hook 接收 CALL/RETURN 及可选 COUNT。回调本身没有用户上下文，g_active_profiles 用 state 查找宿主持有的活动对象；正常情况下同一 state 只有一个活动 Profile。宿主应在 state 和被借用函数元数据仍有效时初始化、启动、停止并销毁对象。进程级活动表没有锁，当前接口不承诺并发启动、停止、读取或重入安全。

Init 面向新建或已经销毁的对象；在持有数组或处于活动状态时重复 Init 不能当作清理。Start 保留既有聚合结果，Stop 丢弃未闭合计时帧；Reset 清空逻辑记录并保留数组容量及当前 hook 活动状态。Destroy 释放本对象拥有的 entries、frames、samples 三个分配器数组，借用的 state、函数元数据不由它释放。

Start 在发布完整会话状态和 state 映射后安装包装回调，保留旧 callback、mask 和 COUNT 基础周期。Stop 恢复这些基础配置，但 Core_SetHook 会重置剩余 COUNT 计数，相位不等于开始前剩余值。活动期间外部改写 hook、旧回调非局部退出、回调内重新启动或销毁 Profile 都没有通用合并或安全恢复保证；COUNT 是基础指令间隔，不是墙钟采样频率。

函数聚合以借用函数指针身份作键，同名但不同身份分别计数；采样以函数与源码行组合为键。GetSampleCount 返回组合条目数，GetTotalSampleCount 返回成功记录的 COUNT 命中总数，GetSample 返回该组合的借用聚合项。FindByName 选择第一个匹配名称；截断后的文本和同名函数可能碰撞，名称并不等价于唯一身份。

entries、samples 内嵌名称和来源的固定容量文本快照，按原始字节截断，不承诺UTF8边界。GetEntry/GetSample 等返回的内部地址在相应数组扩容或 Destroy 后失效；Reset 使旧逻辑记录失效。函数指针不是新增GC根，调用方仍须维持函数和 state 生命周期，停止采集也不会把它们变成独立拥有的报告对象。

CALL 先累计调用次数再为计时帧预留容量，普通分配失败可能保留部分计数；RETURN 先累计返回次数再查找匹配帧，匹配较深帧时可以丢弃上方帧。因此异常、OOM、非局部退出后的调用/返回/耗时并非事务式完整记录。clock() tick 经换算得到纳秒形式，只用于同一计时源内的相对聚合，不承诺跨平台统一的单调墙钟、精确CPU时间或完整异常耗时。

6项TODO沿用4个既有家族：THREAD 的进程表/宿主串行限制；TIME 的 clock 语义与精度；CAPACITY 的三种数组增长边界；HOOK 的旧观察者/COUNT配置协作。各自疑点和具体核查入口留在对应Doxygen及台账，本轮未新增家族或修复行为，也未将风险升级为已复现缺陷。

51段中文Doxygen采用明确实体选择器追加于两个文件EOF，分别补充 Summary、适用参数、返回值、调用前提、失败状态、资源和生命周期及 References。原97+563共660行字节前缀完整保留，新增340+694行；逆操作移除精确后缀即可还原。有限正则验证3347个程序atom和10个字符串/字符字面量的拼写、原始行、UTF8字节列及绝对偏移相同；这是有限文本证据，不宣称编译器lexer或AST验证。

当前调用证据来自逐单位的符号检索、结构字段消费、hook函数指针、CLI报告和既有测试，并核对19个有限支持整文件、795个原始EOL锚点与189个引用范围。支持代码只提供具体调用合同，不凭链接或引用获得完整模块审查信用。

修改前后均在实际checkout复用物理E盘的既定构建目录，构建 zr_vm_profile_deterministic_test 后直接运行完整既有EXE。两个有序用例分别检查CALL/RETURN聚合与COUNT热点的存在性，均2PASS/0FAIL/0IGNORE、退出0，stdout/stderr完全相同且无归一化。它们不证明精确耗时、逐行覆盖、非零旧hook相位恢复、分配失败、异常或竞态分支。

该CTest注册没有显式目标超时；build900秒和direct120秒仅为本任务watchdog。未运行会写入编译期固定 profile.txt 的CLI导入夹具，不据本次2例声称CLI端到端验证。当前FileAPI13目标的源码及头文件形成有限输入身份围栏，不是编译器或SDK的完整include闭包。

两阶段均使用最初不存在的独占cwd，实际执行后无生成文件。未知介入文件不得清理；提交后只按实际哈希移除自有临时索引及空cwd，复用构建、缓存和验证证据保持OPEN。配套wiki仅把原有两行采样帧描述更正为函数/行聚合、COUNT基础周期和hook剩余相位边界，既有示例和历史执行事实保留。

两源已完整复核，inventory维持reviewed并更新本轮实际哈希和证据说明；外来48行85格及正常索引4个既有暂存路径保留。整叶与全局历史诊断按修改前后基线比较，只验收本批54行，不宣称全仓完成。

配对证据：debug-profile-current-native-pair-r1.json，SHA256 cf3226a01eb9425b60a3773dac7eec5647147d4d79c349223b774e708d241c5a；源码应用：debug-profile-current-source-application-r1.json，SHA256 eb88cc004b54548150927fc3c165e0151ca5ad67ba3b5a4c9a1b06af82fe7f52。
