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

# 调试值预览、变量句柄与子项快照合同

本批逐项审查debug_snapshot.c的42个既有单元：39函数、2宏和1个字节切片语句块；EOF追加41段中文Doxygen，39函数及2宏的职责、使用时机、必要约束、生命周期及References均有文档，含130个方向参数、32个非void返回和7个void Returns小节。字节切片块随所属分页函数审查，其窗口、借用与失败约束由同一合同说明。正式九列表结论40commented、2BUG，0新增单元或问题家族，签名和程序行为不变。Frame/Scope与安全求值的另外39项分批交付；本文件此时只完成42/81，inventory保持pending。

展示名称遵循实际值标签与原型协作：OBJECT/ARRAY且有prototype->name时使用其借用字符串，否则才按标签回退静态名称；缺少state/value则回退value。名称、函数、原型与VM存储没有因预览而获得新的GC root，调用方必须保持相应VM及停点状态有效。不能把浅值快照或C字符串指针当作持久身份。

固定预览缓冲区按字节计长、保留NUL并依容量决定截断。文本分片使用原字符串的字节窗口，宏、切片指针与长度的使用受原字符串存活约束。现有UTF-8 byte_slice问题沿完整调用路径确认：64字节窗口可能截断多字节编码，协议cJSON保留原字节，客户端Buffer转文本时可出现替换字符；两个既有BUG行是同一问题家族的镜像。本批只记录，现有ASCII截断用例没有提供多字节边界的运行复现。

变量句柄由Agent持有的原生表登记，描述对象、prototype、固定字段或索引窗口的浅借用。登记INDEX_WINDOW时保存value/prototype/start/count并分配handle_id，此时尚未裁剪窗口；读取时才使用调用方已传入的storage裁剪并物化预览。普通calloc失败返回FALSE，后续Core物化或预览构造仍可能Throw；没有统一的非局部退出清理承诺。返回的平面C预览数组由调用方ZrDebug_Free释放；释放数组不延长也不回收被借用的VM对象。

对象、prototype、全局与索引子项的named/indexed数量按实际分支和元数据计算。传入的过滤、start/count并非所有分支都执行同一种分页，不能从总数或TRUE推导完整枚举、稳定顺序或跨resume有效。当前停止帧和VM存储必须由上层保持稳定；句柄在resume后失效的现有测试只证明其具体场景，不构成并发、GC或所有失效路径证明。

独立复核形成99个精确typed修订，原始F/I保留：修正原型类型名职责与355–356真实声明、NULL预览与默认value的差别、只读agent参数方向；补齐39函数及2宏的必要约束/生命周期标题、7个void Returns和42行纯path:decimal证据；移除多余EOF空行并重算所有依赖针。Root另记录3处I摘要更正：prototype名称优先、INDEX_WINDOW浅登记不先裁剪、读取窗口使用调用者storage且预览构造可能Throw；原I不重写，不增加源码改动或审查信用。

源码只追加59907字节943LF，原143987字节3369LF逐字保留，结果203894字节4312LF。19751个程序atom及237个字符串/字符字面量的四元坐标保持：拼写、绝对UTF8偏移、物理行、UTF8字节列；逐字移除后缀恢复批前原件。这是有限正则投影，不能冒充完整编译器lexer/AST等价证明。20个有限支持whole和309个原始行锚用于静态证据，支持文件不获得本批整模块审查信用。

在实际checkout复用E盘clang-cl构建目录，未重新配置或添加测试。当前FileAPI递归18个target、974源文件及930个首方include头文件作有限输入守卫；本批源码按受控后缀逆投影，其余输入逐字不变。前后均构建成功，直接无参数依真实main顺序运行snapshot_contracts3、truncation5、agent17和variable_child_shape3，每阶段28PASS/0FAIL/0IGNORE，十个native子进程退出0；四套原始stdout/stderr逐字节相同、无归一化，共28个不同用例、56次PASS实例。

snapshot_contracts3只链接Core并读取实际checkout源码文字，不执行debug_snapshot.c；其余25例才链接含本文件的调试库。它们提供ASCII截断、长串窗口、TCP栈/变量、子对象/Union预览、闭包与语义摘要等既有回归证据，不能推导39函数所有分支、OOM/Throw注入、Unity长跳转清理、竞态、GC或多字节BUG已验证。900秒build与600秒test为本任务watchdog，CTest注册没有这些显式TIMEOUT，均未触发，不授WSL或全仓通过信用。

独占E盘测试cwd起初不存在，debug_agent_binary_fixture.zro提前登记；Agent既有binary用例正常尾部删除文件，两个阶段实际检查cwd均为空。早期断言/Throw可能跳过remove、join、close和VM销毁，因此没有将正常尾部当作所有失败清理保证；语言gauntlet读取实际project源码，没有创建源码副本。清理仅由Root在提交后核对自有索引及空cwd执行，共享B/cache及失败证据保留。

台账742行将选中42个同实体旧flat路径键一一迁移到inspect路径并替换完整合同，其他700行raw不变；这是路径整理，不新增函数或审查单元。inventory只更新本文件真实哈希和42/81部分范围，仍1799 reviewed/2462 pending；48个外来inventory行85格及4个外来暂存路径保留。510叶43979完整唯一键总数不变，未预记其余39项或全仓验收。

实际配对证据：debug-variable-handles-children-current-native-pair-r1.json，SHA256 3468fb0d9f734c55641eeb21c6f3f661e3d3178168e04beb04699ab8ad37cf25；源码应用：debug-variable-handles-children-current-source-application-r1.json，SHA256 97c7ae066a7c1716c27470540bbde4d2942161dec62efd3e85074ca6e9e3c6c1。
