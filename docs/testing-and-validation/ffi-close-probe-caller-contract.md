---
related_code:
  - tests/ffi/debug_ffi_close_probe.c
  - tests/ffi/ffi_fixture.c
  - tests/ffi/test_ffi_module.c
  - tests/ffi/gdb_ffi_load_failure_probe.gdb
  - tests/ffi/gdb_zr_vm_ffi_close_crash.gdb
  - tests/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
implementation_files:
  - tests/ffi/debug_ffi_close_probe.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/gdb_ffi_load_failure_probe.gdb
  - tests/ffi/gdb_zr_vm_ffi_close_crash.gdb
doc_type: test-contract
---

# FFI 手工诊断探针的调用契约

## 入口与观察目的

`tests/CMakeLists.txt:6574-6589` 将本文件作为 `zr_vm_ffi_probe` 支持目标注册，链接测试支持及所需模块，并依赖生成的 fixture 路径头与动态库。它供命令行和 GDB 手工诊断使用，当前不是 CTest 测试。`main` 选择最小源，创建状态，登记 parser 和 provider，编译后同步执行；函数先于状态释放。

调用形式为 `zr_vm_ffi_probe [mode [libraryPath]]`。缺省及未知 mode 使用 `all-no-lib`；`--verbose` 也会落入该场景。需要库的模式缺第二个参数时返回1。GDB close 脚本已保留该参数与预期 library close 场景不一致的 TODO，不能据脚本名字认定库句柄已经覆盖。

| 模式组 | 使用目的 | 当前观察限制 |
| --- | --- | --- |
| `library` / `library-bool` | 打开与两次 logical close 的三态组合 | 没有 SymbolHandle 延迟卸载场景；bool false 或整数0仍可退出0 |
| `primitive`、`primitive-add/mul/strlen/stdcall` | 隔离标量动态符号调用及对应签名 | 需匹配 fixture 导出和受支持 ABI；`size_t` 对 u64 的32位宽度疑点沿 fixture TODO 保留 |
| `primitive-numeric-fold` / `ternary` | 无库的数值或表达式基线 | 不取得 FFI close 或 native ABI 信用 |
| `all` | 组合 library、callback、buffer 的重复关闭 | 返回值只汇总 library 三态；没有 callback 派发、buffer 读写或 pin 断言 |
| `callback` / `buffer` / `all-no-lib` | 无库的创建与重复关闭观察 | 固定返回1不是资源不变量断言；三个注入 identity 函数都未被派发 |
| `load-failure` | 提取预期缺失库的错误文本 | 任意非 FINE 都退出0，没有核对错误类别；同名库也不保证永远不存在 |

## 同步交接与资源边界

`ZrProbeExecuteCaptureRequest` 由外层栈分配并清零，仅在一次 `TryRun` 和立即结果复制期间存活。它借用函数与结果栈槽，不取得其所有权；完成标志还必须与 TryRun 状态及非空结果锚一起判断。

回调创建 closure 后尝试登记临时 ignore，在栈准备和 callee 发布之后解除成功登记的 ignore，再通过可恢复锚调用。`CheckStackAndAnchor` 的名字本身不能证明实际触发 GC；当前实现先保存锚，检查/增长栈，再恢复位置。源与这些调用顺序本轮不变。

错误字符串来自 VM 内部借用指针，先复制至宿主容量受限的缓冲区，再 `ResetThread`。返回的是已保存的原失败状态；不能据此推断清理后的全部线程状态。正常回调早退而无完成/结果锚时合成 RUNTIME_ERROR，当前不执行相同的 ResetThread，原因和恢复契约保留具体 TODO。

可选 `result` 为 NULL 时丢弃返回值；非空时必须指向可写、已初始化的 `SZrTypeValue`。有效旧值的 ownership 可能被复制函数释放，全新输出槽可先用 `ZrCore_Value_ResetAsNull` 建立初态。成功时覆盖，失败时不写；现有源返回标量，不把调用的 TryRun 边界扩大为任意对象结果复制的保护。

## 已证实缺陷与后续核查

BUG: `main.result-destination` 声明全新自动 `result` 后直接传址。正常成功条件成立时，`execute_function_capture_status` 调用 `ZrCore_Value_Copy`；`value.h:218-236` 进入 `PrepareDestinationForOverwriteNoProfile`，在 `value.h:147-164` 先读取旧 ownershipKind/control/weakRef，可能进一步 ReleaseValue。该可达调用链证明未初始化读取；表现可能是错误所有权释放、诊断陷阱或崩溃，未声称每次运行必然崩溃。默认无库模式也可能到达该成功路径，load-failure 的空输出槽不走复制。建议修复入口为传址前 `ZrCore_Value_ResetAsNull(&result)`；本轮只加标签，不改行为。

TODO: 转义器仅处理反斜杠和双引号，容量不足静默截断；两个 `snprintf` 调用也未检查返回值。沿这些调用核查超长路径和控制字符的源有效性及诊断策略。

TODO: 对照共享 runtime helper，核查 Closure_New 分配前 function 的 GC 根，以及 IgnoreObject 登记失败或栈准备抛错时的临时保活与解除。当前零捕获 InitValue 不产生捕获单元分配，不能把一般捕获路径的分配风险误写为本例实际步骤。

TODO: 对 closure 创建失败导致的回调早退核查合成错误的线程恢复与原因报告；对照现有两个 ResetThread 失败分支。

TODO: 核查 load-failure 的错误分类，区分预期缺失库与其他运行失败；普通模式则需明确期望结果断言或纯观察性目标，当前只打印并按执行状态退出。

## 审查与验证范围

逐单元台账为 `docs/code-review/coverage/tests_ffi.tsv` 中本文件的39项：7函数、1类型、3字段、14数据模板、3注入函数、1结果状态和10重要块。注释位于 C 源的字符串外，注入源字节、全部原400物理行及其混合换行均保持不变（344 CRLF、56 LF）；新增39个完整注释块共349行，形成749行文件。

fixture 中四处指向本探针的旧行号也按同一原始语句迁移；fixture 原974行仅四处数字引用改变，不新增该文件单元或语义覆盖。

修改前已完成真实消费者 TU 编译，342个实际依赖，owned job 自然退出0并关闭全部句柄。后续修改后编译必须使用同一命令、同一非本批依赖并与该基线配对；台账检查和非注释投影只证明各自静态事实。未取得 fixture DLL 构建、链接、probe 运行、BUG运行复现或整个模块验收信用。
