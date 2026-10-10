---
related_code:
  - tests/ffi/ffi_fixture.c
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/ffi/debug_ffi_close_probe.c
  - tests/ffi/ffi_source_extern_add_manual.zr
  - tests/system/test_system_fs_module.c
  - tests/CMakeLists.txt
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
implementation_files:
  - tests/ffi/ffi_fixture.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/system/test_system_fs_module.c
doc_type: test-contract
---

# 原生 FFI fixture 的 ABI 与调用生命周期

## 使用目的和入口

`ffi_fixture.c` 构建为测试共享库，给动态符号、源码 native extern、工程 AOT、ref/out、callback 和 FileStream wrapper 等消费者提供可观察的原生端结果。小值哨兵核对特定 ABI 路径；只取得符号、检查元数据或编译成功的消费者不能自动获得实际执行信用。

测试通过生成头得到 fixture 路径；此共享库独立于 VM 数据结构。导出宏在 Windows 提供 DLL export，POSIX 分支为空；stdcall 宏同样按平台提供声明，调用方仍须选择受支持的 ABI。

## 数据与原生参数的约束

| 形状 | 对调用者承担的职责 | 限制 |
| --- | --- | --- |
| `Point` 的 `x/y` | 两个有序 i32 的按值参数、返回和 out 写回见证 | 对齐、字段顺序和宽度对应宿主；字段和可表示；返回不是堆句柄 |
| 两个 union | mixed integer/double 与不同宽度 float/double 的按值 ABI 见证 | 共用存储，调用方明确活跃成员；fixture 不保存或验证活跃成员标签 |
| `Mode` | 为 wrapper handleId 解包后的 i32 提供 OFF/ON 哨兵 | 传入 i32 而非 C enum 对象；任何非 ON 值均返回 ON，不提供枚举值校验 |
| 原生指针和缓冲 | 借用本次调用期间的可读字符串或可写 Point/i32/字节存储 | 不接管、保存或释放地址；调用方保证容量、对齐和直到 NUL 的可读性 |
| 可变参数 | 以总和核对显式尾参数展开 | count 不超过实际参数数目，遵守 C 默认提升及本宿主取值 ABI，整数和须可表示 |

u64 精度样本使用 `2^53+1`；负 i64 样本是 `-(2^53-1)`，仍可由 binary64 精确表示，只见证有符号整数往返。小值相加、字段相加和 increment 不提供有符号溢出保护，调用者须使结果可表示。

TODO: `strlen` 返回宿主 `size_t`，现有签名使用 u64，需要沿实际 32 位目标核对 ABI。字节填充和文件偏移的 i32 返回还需核对大长度或大偏移的窄化合同；现有小值样本不覆盖这些范围。

## Callback 与线程的边界

同步 Apply 借用 `double(double)` 入口，由 trampoline 和调用策略决定 VM 回入、异常默认值及错误记录。Twice 在同一次 native 调用中借用两次入口，嵌套 Apply 用例核对外层策略恢复；fixture 不延长句柄生命周期。

保存槽只保留裸 trampoline 地址，不取得 VM 句柄或 GC 根。晚到调用确实从 C 执行该地址；当 closure 存储仍存在时，失效的 call 生命周期可在 trampoline 内阻止进入 VM。释放后的 closure 地址不再可调用。

TODO: 保存槽的清空、并发和句柄/库析构顺序需沿 store/invoke 与 callback finalizer 核实。原有测试负例不构成任意异步保留裸回调地址都安全的保证。

foreign-thread 入口把 callback、value 和 result 放入 C 调用线程栈上的局部 `data`，传 `&data` 给新 OS 线程。成功等待完成后才能读取结果并离开作用域；线程入口借用而不释放这块存储。

TODO: 当前 `WaitForSingleObject`/`pthread_join` 未检查结果，需沿等待返回值证明线程已经结束。当前 VM 用例只观察执行返回 FALSE，没有核对 callback-thread 错误类别或消息；不能据此排除其他执行失败满足同一判据。

## 宿主错误和库句柄

errno 入口写库侧当前线程的 CRT 状态并返回 -1；消费者必须在同线程、同 CRT 中读取。Windows 静态 CRT 跨 DLL 的用例会跳过，不能由此推导所有宿主能看到该 errno。returnCode 入口只返回给定 code，成功/失败含义及清理政策由 native extern 调用者解释。

版本导出返回库内静态 NUL 文本；`LibraryHandle.getVersion` 用直接 C 函数指针调用它。地址只在动态库保持装载时有效，调用方不得 free。

BUG: `getVersion` 在逻辑 closeRequested 后，如果 SymbolHandle 仍保持物理库装载，只检查非空物理句柄仍可调用版本导出；与 getSymbol/call 的关闭准入不同。当前 fixture 注释记录调用方缺口，不承诺入口已检查逻辑关闭，也不修复运行逻辑。

## 本批验证和后续扫描

候选需逐项覆盖 exports 声明/定义、类型、成员、宏、全局状态和两平台线程单元，台账区分调用、注册与定义的身份。原代码、字符串、旧注释和 EOL 保持精确；应用后须用翻译单元的前后编译检查注释兼容性，实际结果另按本批收据记录。

本阶段没有运行整个 FFI/native extern/FS suite，也没有运行故障注入或跨平台线程烟测。静态 BUG/TODO 和现有测试的源码判据不能替代运行复现或所有消费者完成。

[fixture 与消费者台账](../code-review/coverage/tests_ffi.tsv)记录各单元的真实地址和调用限制；未选定消费者继续保留原范围。
