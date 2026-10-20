---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
tests:
  - tests/debug/test_debug_threads.c
  - tests/debug/test_debug_agent_protocol.c
  - tests/cmake/debug-runtime-targets.cmake
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# 调试Agent、线程与连接生命周期合同

本批逐项审查32个既有单元：debug.c中11函数和4重要块共15项，debug_internal.h中7接口、2结构和1宏共10项，公开debug.h中4接口和3结构共7项。EOF追加20段完整中文Doxygen，涵盖48个方向参数、12个非void返回、职责、使用时机、约束、生命周期及References；内部声明关联同名C实现的合同，宏沿用明确值和首次登记引用。结论24commented、8no-comment，没有新增单元或BUG/TODO。本批不修改签名或行为。

Start初始化原生Agent、句柄表、连接及trace observer。配置按结构浅拷贝，其中auth_token仍借用调用者字符串，必须保持到Stop完成；并非Start返回后即可释放配置全部指向对象。普通NULL失败按现有路径释放已建立资源，不能据此推断所有Throw都事务回滚。默认线程登记依赖当前VM状态，引用的state、frame和prototype必须仍有效。

trace observer安装与注销受userdata身份约束。Stop只在仍属于当前Agent时移除自己的observer，普通注销恢复既有trace mask；并非通用observer栈，也不保证恢复被其他调用者替换的observer回调。轮询、暂停、临时线程上下文的begin/end在普通路径协作，内部对象和frame索引仅在对应VM停点及状态有效期内借用。Stop不会替调用者join工作线程或建立并发互斥，调用方应先协调使用者停止访问，再销毁Agent及VM。

临时上下文先保存当前state和线程号，再绑定目标线程以供栈、变量和求值层使用；普通end恢复配对状态。失败路径和输出须按各入口具体合同判断，非局部Throw不提供自动finally恢复承诺。线程枚举的快照和引用身份不延长VM对象生命周期，不保证其他线程并发修改时安全。

ZrDebug_ReadTestManifest返回原生平面快照数组；调用方用ZrDebug_Free释放，数组可以在Agent停止后仍独立持有，但其中语义不能自动投射为新的VM对象所有权。GetEndpoint的TRUE表示已进行格式化拷贝，极小buffer可发生截断，buffer大小为1时可得到空字符串；不能把TRUE等同于完整端点字符串。所有借用和输出有效期以具体接口Dox为准。

独立复核只修订4项审查metadata：内部zr_debug_agent_end_thread_access声明仅占404行，后续405–406属于data_breakpoint_info API，修正其末行/raw pin/长度；agent_protocol构建锚从同文件113更正为278，113仍是另一组agent17测试的有效入口。20段Dox和32正式单元未因此改变。内部头的实际应用基线已包含Union13段Dox：在45476字节前缀追加3604字节，仅此2段；没有使用较旧28984字节预测覆盖前序提交。

三个文件仅在EOF追加，原raw前缀及14482个程序atom、157个字符串/字符字面量的五元坐标保持：拼写、物理行、UTF8字节列、Unicode列和绝对偏移。逐字移除后缀可恢复各自实际批前raw；有限正则投影不冒充编译器AST/lexer证明。16个支持whole和32个原始span支持静态合同，借用文件不获得整模块信用。

在实际checkout复用E盘clang-cl构建目录。FileAPI递归14个target、964源文件及922个首方include头文件作有限守卫；批内原有输入逐字不变或按三个受控EOF后缀投影回基线，未重新配置、未添加测试。前后均构建成功，并直接无参数按既有main顺序运行threads完整1例和agent_protocol完整8例；每阶段9PASS/0FAIL/0IGNORE，六个native子进程退出0。两套原始stdout及stderr逐字节相同，无归一化，共9个不同用例、18次PASS实例。

既有用例覆盖线程路由、扩展能力、typed manifest、TCP暂停、求值权限、resume后子句柄失效、暂停断开、socket关闭及重连暂停。它们不覆盖所有11函数分支、所有配置失败/OOM/Throw注入、非局部退出资源清理、竞态或GC生命周期。独占cwd起初不存在，两个阶段均为空，源码样例在内存中，没有磁盘生成夹具。正常尾部清理不保证Unity长跳转仍执行；CTest无既有显式TIMEOUT，900秒build及600秒test是任务watchdog，两阶段未触发。协议测试耗时分别207.094和217.687秒，不授WSL或全仓通过信用。

台账742行中的选中32行逐项收口，其他710行raw不变；32个完整三元键均未迁移。inventory三个文件继续pending：debug.c累计61/90余29、internalH累计35/89余54、publicH累计21/41余20；仅更新真实哈希与部分范围。48个外来inventory行85格、4个外来暂存路径保留；510叶43979完整唯一键总数不变。

实际配对证据：debug-agent-thread-lifecycle-current-native-pair-r1.json，SHA256 7358f58eb7a6fa3ac777a826a9967779b8412ab6685da19c8b414a77c3811d88；源码应用：debug-agent-thread-lifecycle-current-source-application-r1.json，SHA256 33945b5df924e2a8fb06e2dfd9fae5b0ee592f65e8cbcb1e0ea9e825da372e6c。

暂存前覆盖计数按全部已交付台账重新闭合：Trace7（8b74437193e4d95168cc2e9125589d79626c8d72）、Debug18内5（49f4ce88512758195ec12b1128a97af5beaf9d85）及Union13（b87eb2e71b5db9c4eb80bb7701741808bb773367）共25个唯一内部头键，均逐字匹配各自已提交九列；与本批Agent10无交集，最终累计35/89余54。初步r1仅计Union、本轮得到23，r2加入Trace得到30，两者仍漏历史Debug18五项；r1/r2原件和旧暂存证据保留，r3只纠正说明，未新增单元或动态测试信用。
