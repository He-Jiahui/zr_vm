---
related_code:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/ffi_fixture.c
  - tests/harness/runtime_support.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
implementation_files:
  - tests/ffi/test_ffi_module.c
tests:
  - tests/ffi/test_ffi_module.c
doc_type: test-contract
---

# FFI 模块测试的调用与资源契约

## 调用链与判据

每例创建独立 global/mainThreadState，注册 container、math、system、ffi，再用同一 state 编译或执行内嵌 ZR 源码。CMake 的 zr_vm_ffi_test 依赖动态库 fixture 与生成的绝对路径头；Unity main 逐项调用 30 个测试。测试不共享 VM state，普通返回路径由该用例释放 global。

编译返回 NULL 只说明编译失败，不能单独证明具体诊断；四个此类样本没有执行原生函数。元数据用例检查类型和签名，不代表方法已经执行。动态符号的 signature 由 ZR 调用方提供，需与 fixture 的 C ABI、按值结构体布局、可写缓冲长度及平台约定匹配。

标量和精度用例、Point/BufferHandle、varargs、stdcall/system、native extern delegate、pointer/handle_id wrapper、pack/offset overlay 分别验证各自断言。underlying(string) 的负向模板只声明、不调用 fixture 的 i32 函数；普通函数的负向 wrapper 样本不能证明所有 extern lowering 或诊断路径。

## 执行状态与借用

capture helper 用 TryRun 捕获异常，同时检查 threadStatus 和 callCompleted。未完成但没有捕获异常时合成 RUNTIME_ERROR；它与真实捕获异常不是同一种证明。仅在成功路径复制返回值；错误字符串先复制到宿主缓冲，再 ResetThread，避免借用已重置的 VM 异常对象。CallAndRestoreAnchor 恢复栈位置，不接管函数、global 或返回对象。

CallbackHandle 负责 trampoline 的存活；fixture 保存的函数指针没有 GC 根或所有权。foreign-thread 样本只检查执行失败，尚不能凭现有断言确定是哪一个线程策略诊断。关闭和幂等样本验证其实际状态及返回判据，不证明所有回调、buffer 或库卸载组合。

## 已知失败链与待核实边界

BUG: Unity 硬断言使用 longjmp。state 属于各测试局部变量，tearDown 为空；在创建 state 后的硬断言失败会跳过普通路径上的 destroy_test_state。账本中的 31 条 BUG 是这一共享静态失败链的落点，不代表已复现 31 个独立泄漏。

TODO: allocator 的低地址 void* 比较、注册失败/非局部异常协议、路径转义截断和控制字符、capture body 的函数 GC 根及 IgnoreObject 平衡、ResetThread 状态改写，均需沿对应实现及调用者继续核查。正常样本通过不能代替失败路径证明。

TODO: tests/CMakeLists.txt 创建并连接 zr_vm_ffi_test，但当前 language_pipeline 的 suite 列表没有该目标。需要核查预期的自动执行入口；拥有可执行目标不等于 CTest 已运行这些样本。

## 代码索引

- 测试入口和逐单元证据见 [tests_ffi 账本](../code-review/coverage/tests_ffi.tsv)。
- [FFI API](../wiki/03-modules/ffi-api.md) 记录 handle、signature 和原生调用表面。
- [source-level extern](../parser-and-semantics/ffi-extern-declarations.md) 记录声明与 lowering 边界。
