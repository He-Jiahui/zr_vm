---
related_code:
  - tests/system/test_system_fs_module.c
  - tests/harness/runtime_support.c
  - tests/harness/path_support.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - tests/CMakeLists.txt
  - tests/cmake/run_executable_suite.cmake
  - tests/ffi/ffi_fixture.c
  - tests/harness/zr_test_log_macros.h
  - tests/third_party/zr_unity/Unity/src/unity.c
implementation_files:
  - tests/system/test_system_fs_module.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/system/test_system_fs_module.c
doc_type: test-contract
---

# 文件系统测试的调用与资源契约

## 入口与测试目的

CTest 的 system_fs 项通过 run_executable_suite 启动 zr_vm_system_fs_test；Unity main 登记八个用例并返回 Unity 失败计数。元数据用例创建独立 State，另外七例生成源码再编译；其中五例实际执行，普通调用负例和 tellFd probe 只检查编译结果。涉及 native 句柄的样本依赖 FFI fixture 动态库及生成的绝对路径头。

元数据用例从同一 descriptor 的两条物化路径交叉检查公开 types 条目与实际 FileStream 原型上的 handle_id、i32、owned、close 字段。接口名称存在不能证明所有方法已执行；元数据查询借用 VM 值或字符串，不能越过所属 State、对象存储改写或可影响视图的 GC 保存。

## 各样本的实际判据

- 路径对象样本将文件、目录和路径操作的失败编码为负整数，C 侧要求结果为 1；成功脚本才删除该根。
- 拷贝样本分别读取 exists 与 readText，避免逻辑短路遮蔽成员调用；正常尾部没有删除生成根。
- 流模式样本实际执行读写接口、位置长度、独占打开及 native 句柄调用。using 分支读取成功，没有直接观察离开作用域后的 closed 或句柄状态。
- 缺失路径样本要求 IOException catch 返回 1；没有异常则返回 0。未检查消息、错误码或其他路径错误。
- 关闭句柄样本先编译成功，再要求执行失败；没有异常类别或特定诊断断言，仍需排除不相关失败满足该判据。
- 普通调用负例只要求编译返回 NULL，不执行脚本；尚未核对具体类型诊断。
- tellFd probe 编译完整流模板，按源码第 22 行检查调用指令家族、一元 @call 及没有数字转换；不执行模板。历史指令偏移 112 和 SymbolHandle 原型、元方法查询只作打印诊断，后者没有非空或一致性断言。

## 生命周期与已知失败链

每例拥有自己的 State，普通源码用例在正常尾部释放 Function，再销毁关联 Global。setUp 和 tearDown 不建立或回收共享资源；VM 销毁不等于磁盘生成根已删除。元数据视图只借用，不能单独 free。

BUG: 成功取得 State 后，Unity 硬断言可通过 longjmp 跳过正常释放；空 tearDown 不兜底。九条相关 BUG 台账记录同一静态可达清理失败链，不代表已复现九个独立泄漏。

BUG: probe 的旧打印说明称模板在第一次 tellFd 后截断，实际两个解码模板逐字相同，均为 58 行、2011 字节。它会误导源码或指令偏移比对；模板和旧输出保留，附近注释标记这一已证明的不一致。

## 待核实边界

TODO: system、container、ffi 注册返回值未消费，不能从非空 State 推断所有导入前提已经建立。路径与源码格式化、反斜杠和双引号转义没有完整写入判据，也不编码其他控制字符，需核查配置路径的长度与允许字符。

TODO: 时间、clock 和进程内计数不是跨进程唯一的原子预留；已有同名根会被删除。需核查并行运行的命名与生成条目保留政策，当前没有观察到路径碰撞或截断。

TODO: using 关闭状态、关闭句柄的实际诊断、普通调用的类型诊断与 SymbolHandle 可调用性，均有对应源码注释列明后续可消费的验收入口。现有输出或编译成功不能扩大为这些边界已被强断言。

## 代码索引

- [测试单元台账](../code-review/coverage/tests_small_remaining.tsv)列出具体调用与源码证据。
- [System API](../wiki/03-modules/system-api.md)说明文件系统公开表面。
- [FFI API](../wiki/03-modules/ffi-api.md)说明 native 句柄与调用边界。
