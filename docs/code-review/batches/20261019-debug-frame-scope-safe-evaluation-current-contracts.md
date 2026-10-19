---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c
tests:
  - tests/debug/test_debug_snapshot_contracts.c
  - tests/debug/test_debug_truncation.c
  - tests/debug/test_debug_agent.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cmake/debug-runtime-targets.cmake
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# 调试栈帧、作用域与安全求值合同

本批完成debug_snapshot.c剩余39个既有单元：33函数、1宏及5个有独立旧身份的语句块。EOF追加34段中文Doxygen，覆盖职责、调用时机、必要约束、资源与暂停生命周期以及References；150个方向参数、33个非void返回说明；本批无void函数。语句块随所属函数解释非显然的投影、分派和失败顺序，保留逐块台账。正式结论35commented、4个既有BUG镜像，0新单元、0新问题家族。结合已提交7c5fb24b的42项，本文件72函数、3宏、6块共81个旧单元均完成审查，inventory据此标为reviewed；支持文件不获得整模块信用。

栈帧读取经Core introspection与调试层投影协作，VM帧、闭包、prototype和局部值均借用当前VM状态。不存在由快照自动产生的GC root或跨resume稳定身份。上层须保持暂停数据与VM存储有效；函数各自的NULL、空结果、普通FALSE、分配失败和非局部Throw出口不能互相替代。平面C数组由调用者ZrDebug_Free释放，不代管其浅借用的VM对象。Union相关calloc正常失败分支发生在输出建立前；不能据此虚构分配成功后所有失败分支均有清理的保证。

作用域既包括活动Core帧的槽位筛选，也包括异常保存帧及动态变量句柄。帧编号、局部槽、接收者、Lexical/Global或对象子项必须按真实分支解释。编译期来源元数据用于选择展示路径，并不代替VM当前存储或字段有效性检查。闭包查询返回借用的VM闭包或NULL，没有调用者释放的输出对象。固定文本容量、终止符及部分输出规则以各接口合同为准，TRUE不能外推完整枚举或所有分支统一分页。

求值辅助路径按当前策略解析名称、字段及可允许的表达式；不因一个兼容入口可返回成功，就推导正式capability已经开放，或所有调用都会内部统一确认暂停。合法性分类、值读取与允许的操作由上下层分别承担；调用者必须遵守相应停点和策略前提。普通拒绝和运行时Throw分别记录，不能声称所有求值都不分配、无副作用或自动回滚。本批仅补注释和证据，不改变策略或公开签名。

两个既有BUG家族由完整静态路径支持，保留函数/块各两镜像。第一类：暂停异常有非空保存stacks时，协议scopes请求给出大于保存帧数的非零frameId；当前只检查frameCount>0，仍返回Exception scope。第二类：真实活动frameId>=100时，算术scopeId进入动态句柄基数1000以上；ReadVariables先按句柄分派，无匹配时FALSE，碰巧匹配其他句柄时可能展开错误值。证据连接Core实际栈、scope编码、协议入口与分派分支，不依目录名或猜测；本轮未做越界帧/深栈运行复现，也不修复行为。

独立复核对原F提出13项严格typed修订：移除多余EOF空行并更新所有派生针；34段合同和39行内容保持。Root另记录原I两处摘要更正：闭包查询不是out参数；Union分配后没有所称的FALSE清理分支。原F/I保持不可变，effectiveF与Root修订单独存证，避免把审查摘要误写为源码事实。

实际应用在已提交Snapshot42的203894B4312LF后追加59642B841LF，结果263536B5153LF，前一批41段Dox逐字保留。19751程序atom及237字符串/字符字面量的拼写、绝对UTF8偏移、物理行和UTF8字节列均相同，删除后缀精确恢复批前raw。这是有限正则投影，不是编译器lexer/AST或运行时等价证明。35个有限whole支持输入、315个原始引用锚与3个已提交Agent EOF注释桥均逐字校核；另有本文件自身前一批后缀组合，只有受控注释差异被接受。

复用实际checkout的E盘clang-cl构建目录，前后各构建一次，直接无参数运行snapshot_contracts3、truncation5、agent17及variable_child_shape3，每阶段28PASS/0FAIL/0IGNORE，十个native子进程均退出0。四套stdout/stderr逐字节相同，无归一化；共28个不同用例、56次PASS实例。当前FileAPI守卫18个递归target、974源文件、930首方include头文件；只对本批已验证后缀逆投影，其余输入逐字保持。900秒build/600秒test为本任务watchdog，均未触发，不是现有CTest显式TIMEOUT。

snapshot_contracts3只链接Core并读取实际源码文字；其余25例链接真实调试库，提供现有TCP栈/变量、安全求值、闭包和Union子项等回归证据。它们不覆盖本批33函数所有分支，也不构成OOM/Throw注入、Unity长跳转清理、竞态、GC、两个BUG运行复现、WSL或全仓通过。历史diagnostics56的一处getter失败保留历史证据，本批未重跑，不能宣称已修复。

独占测试cwd及已登记debug_agent_binary_fixture.zro在两阶段结束时均为空；正常尾部remove、join、close和VM销毁不证明早期失败或Throw必然清理。未建立源码树副本或新增测试。提交后仅核对并移除本批自有临时索引及空cwd，保留共享构建、缓存、其他会话产物和必要证据。

本批将叶台账选中39个同实体旧flat路径键一一迁移为inspect路径，其他703行raw保持，其中已交付Snapshot42行逐字保留。39+42互斥并覆盖该文件全部81个旧单元；没有新增审查身份。全仓510叶43979个完整唯一键总数不变，inventory为1800 reviewed/2461 pending，48个外来inventory行85格和4个外来暂存路径保持。该文件完成不代表全仓验收。

实际配对证据：debug-frame-scope-safe-evaluation-current-native-pair-r1.json，SHA256 e1e8068ec470495b2176d630b9257dfb6225de2b646e74edcfce5745557f1b37；源码应用：debug-frame-scope-safe-evaluation-current-source-application-r1.json，SHA256 4aa970e9c504be7006c5991727204bdac4f8c91c926ffd7bf47ce16c702e8c58。
