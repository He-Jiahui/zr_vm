---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_runtime_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/env.h
  - zr_vm_lib_system/include/zr_vm_lib_system/env_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/env/env.c
  - zr_vm_lib_system/src/zr_vm_lib_system/env/env_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-13 System Env contract review assignment
tests:
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
  - tests/cmake/compiler-project-targets.cmake
doc_type: workflow-detail
---

# System Env 契约审查

## 范围与单元清点

本批仅覆盖环境变量叶模块的四个源码文件，并新增逐单元台账 [zr_vm_lib_system_env.tsv](../coverage/zr_vm_lib_system_env.tsv)。台账共 15 项：

- `env.h`：include guard 和公开 callback 声明，共 2 项。
- `env_registry.h`：include guard 和公开 descriptor getter，共 2 项。
- `env.c`：callback 实现、参数读取错误块、环境值交付块、GC 恢复期间的宿主指针借用 TODO、长值缓存依赖 TODO，共 5 项。
- `env_registry.c`：数组计数宏、descriptor getter、函数表、类型提示表、提示 JSON、模块描述符，共 6 项。

`module.c:85` 在系统模块注册表中调用 `ZrSystem_EnvRegistry_GetModule`；getter 返回包含静态 `kModule` 的借用指针，注册插件按 `functionCount` 注册 `kFunctions`（`native_binding_registry_plugin.c:971-972`），并把函数 descriptor 的 callback 经 native dispatch lane 调用（`native_binding_dispatch_lanes.h:277`）。描述符、嵌套表、字符串与 callback 指针都借用 provider image；该 image 必须驻留至注册器不再使用这些引用，浅拷贝 descriptor 不延长 image 寿命。用户侧示例在 `system-api.md:73-74` 导入模块并调用 `getVariable("HOME")`。这些是注册和调用链证据；测试没有直接运行该示例。

## 已确认的调用契约

描述符将 `getVariable` 的元数固定为一个（`env_registry.c:16`）。通用 dispatch 在构造 context 后先以 `native_binding_auto_check_arity` 消费该描述符，并调用 `CheckArity`；数量不符由 `RaiseArityError` 抛 VM 异常，callback 不会运行（`native_binding_dispatch.c:34-52,364,846-858,877-899`）。正常注册入口进入 callback 时已有一个参数；`ReadString` 此时负责检查字符串类型，非字符串由 `RaiseTypeError` 抛异常（`:1204-1217,862-875`）。`ReadString` 自身还保留缺参时调用 `RaiseArityError` 的保护，但注册入口的元数错误已先由 dispatch 拦截。callback 普通 `ZR_FALSE` 只对应 context/result 无效或防御性 `nameString` 为空。

`getenv` 在调用时读取宿主进程环境。返回指针归宿主 CRT 管理，只用于当前同步查询与 `ZrLib_Value_SetString` 复制；脚本取得的字符串是 VM 管理对象。变量不存在是正常结果，写入 VM null。变量名经 NUL 结尾 C 字符串传入；若 VM 字符串含嵌入 NUL，`getenv` 只查询此前缀，需完整匹配时调用方应传入无 NUL 的名称。该 callback 不锁宿主环境；宿主调用方需要按目标 CRT 的规则协调并发环境修改，并保证 `getenv` 返回值在复制完成前有效。MSVC CRT 文档说明其环境表指针不可由调用方修改，且 `getenv`/`putenv` 需要同步；POSIX.1-2024 对并发修改环境时的 `getenv` 不提供线程安全保证：[Microsoft getenv/wgetenv](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-wgetenv?view=msvc-170)、[POSIX.1-2024 General Information](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)。因此本批按宿主同步约束记录，不将所有并发读取一概判为不安全，也不标并发 BUG。

常规启动时，`GlobalState_New` 同步运行 `MainThreadLaunch`；其中 `StringTable_Init` 的首个跟踪调用会缓存 `ZR_VM_TRACE_CORE_BOOTSTRAP`（`global.c:481`、`state.c:144`、`string.c:683,927-935`）。这排除了已完成正常启动后，Env 值复制期间再次首次读取该 string trace 开关。它不能覆盖另一条恢复窗口：`SetString` 创建短串时进入 `string_object_create` 和 `RawObject_New`；若首次 raw allocation 失败，`GcMalloc` 在全局已初始化且非即时回收时会调用完整 GC（`native_binding_dispatch.c:1303-1307`、`string.c:724-735`、`gc_object.c:1069-1075`、`memory.c:13-18,39-46`）。完整回收的函数图槽重写会进入 `garbage_collector_trace_rewrite_function_slot`；该跟踪 helper 首次检查 `ZR_VM_TRACE_GC_REWRITE_SLOTS` 时会再次 `getenv`（`gc_cycle.c:15-24,262-270,1710-1718`）。仓内启动路径没有证明这个独立开关必定已初始化，因此实现和台账为这个精确借用窗口登记 TODO：按受支持 CRT 核实后续 `getenv` 对旧 value 指针的规则，并以受控 GC/分配失败路径验证。没有据此判 BUG，也没有把启动期另一个开关的预热外推到此路径。

另有一个相邻 Core 缓存依赖。Env 的任意长度宿主值通过 `SetString`、`native_binding_create_string` 到达 `ZrCore_String_CreateTryHitCache`（`env.c:35`、`native_binding_dispatch.c:1307`、`native_binding_support.c:320-324`、`string.c:958-974`）。该缓存按输入指针地址选桶并扫描桶项；扫描时将缓存的 `SZrString` 传给 `ZR_CAST_STRING_TO_NATIVE`，而宏契约只允许短串，长串布局需通过 `longString` 访问数据（`conversion.h:73-76`、`string.h:35-37`）。Core 现有台账已把长串经该宏读取标成 Core BUG；Core 字符串缓存行进一步限定了已证明影响，长串场景只证明缓存失效，没有声称返回错误文本（[conversion 台账](../coverage/zr_vm_core_math_conversion.tsv#L39)、[字符串台账](../coverage/zr_vm_core_string.tsv#L71)）。本批另列 Env 依赖 TODO：核环境长值后续同桶比较、`longString` 布局和受控重复 `getVariable` 读取，确定 Env 用户是否可观察到错误结果或无效读取。当前没有 Env 长值同桶复测，故不新增 Env BUG，也不声称任意长度复制路径始终安全。

## 已证实缺陷

`kFunctions` 的 `returnTypeName` 是非空 `string`，而 callback 对未定义变量返回 null，`kHints` 签名与文档均为可空 `string?`/`string/null`。runtime typed metadata 使用 descriptor 的 `returnTypeName` 构造导出签名（`native_binding_runtime_metadata.c:358,375`）；类型引用初始化先清零并默认 object 非 nullable（`:263-280`），parser 随后从 typed export 转回 `returnTypeName`（`type_inference_import_metadata.c:881`）。LSP native declaration projection 虽用 hint 格式化参数，却直接用 descriptor 的 `returnTypeName` 格式化返回类型（`lsp_native_declaration_projection.c:470-486`）。这是可观察的 nullable 契约分歧，完整证据见台账 `kFunctions` 行；本批不修描述符值或行为。

另一个完整静态失败链位于返回值构造。一次合法的已定义变量查询可取得非驻留短字符串；当短串哈希表达到扩容阈值时，`ZrCore_HashSet_AddRawObject` 调用 `Rehash`，其宿主 allocator 失败会使 `Rehash` 返回 false，而此分支不设置 VM 异常（`hash_set.h:525-537`、`hash_set.c:145-180`、`memory.h:19-25`）。`string_create_short` 随后返回 null（`string.c:802-807`），`ZrLib_Value_SetString` 没有检查创建结果并只在字符串对象非 null 时写槽（`native_binding_dispatch.c:1303-1317`）；dispatch 已把结果槽初始化为 null，并在 callback 返回 true 且线程状态仍正常时交付该槽（`:362,615-664`）。所以已有环境值在这条 OOM 路径会被成功返回成 null。台账和源码标注为 BUG；没有运行故障注入，也没有将该结论描述为复现测试。

## 测试及构建证据边界

`tests/module/test_module_system.c:3235,3293-3294` 导入 `zr.system.env` 并断言 `getVariable` 导出存在；`test_system_root_aggregates_leaf_modules_and_reuses_cached_instances` 与 `test_system_root_native_module_info_exposes_module_links` 覆盖根模块导入和 module link（`:3034-3065,3472-3482`）。测试源码搜索没有发现直接执行 `getVariable`、检查未定义变量、验证类型错误、注入字符串分配失败或覆盖长值同桶重复读取的 Env callback 用例。该 Unity target 在 `tests/cmake/compiler-project-targets.cmake:211-223` 注册。`test_official_provider_convergence.c:50` 只列出 `zr.system.env` 的官方 provider 归属，不验证 callback 或 nullable metadata。

修改前的 Debug 静态库构建及 official-provider 9/9 结果由 Root 提供，记录于 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/leaf-libraries-final-r2-results.json`；producer 未重新构建或运行测试。本批只新增中文注释与文档台账，不改变签名或行为。修改后 source SHA、台账锚点检查及独立语义复核仍须分别验收。

## 台账归属与本批验收

旧 System 总表中的本叶 11 项已移出，完整重新审查后的 15 项只由 `zr_vm_lib_system_env.tsv` 维护；其余 360 行保持原始字节及顺序。共享的 Process 测试函数审查继续由 Process 台账维护，本批不重复复制已审查单元。

Root 已比较四个 Env 源码相对任务起点 `901f9898d2326d7b2694a89a77c14255003632d9` 的所有非注释 Clang raw-token kind/spelling 与预处理 StartOfLine 标志，完全一致；源码及调用证据依赖在验证前后保持同一 SHA。证据为 `system-env-process-r1-comment-equivalence.json`（SHA256 `d0e2b9ff4a5d2be3214f6a6803b2816e257cc778dd641e3cdf5e812bfff3e213`），与 Process 更正共用该轮检查，未保存源码副本或 token 正文。

实际 checkout 的既有 MSVC Debug/static 构建由 Root 调用 `zr_vm_lib_system_static` 和 `zr_vm_official_provider_convergence_test`：构建退出 0（15.438s），现有 provider 测试 9/9（0.218s）且逐用例结果与修改前一致，见 `system-env-process-r1-results.json`。provider 目标的传递依赖还编译了当时进行中的 Vector2 与现有 parser 单元；本批不授予 Vector2 最终源码验收，相关 Math 批次仍须冻结后单独核验。该轮存在一条 C4100 警告，不声称无警告。module System 测试仅只读核查，未执行；没有环境查询、nullable metadata、长值碰撞、GC 恢复或 OOM 故障注入的行为信用。

源码冻结 SHA256：

| 文件 | SHA256 |
| --- | --- |
| `zr_vm_lib_system/include/zr_vm_lib_system/env.h` | `f3d8666d0c1624d2618e5880d6ba59189c918c2d94608222b6c4d180933f83e3` |
| `zr_vm_lib_system/include/zr_vm_lib_system/env_registry.h` | `c9dbc21d8ebd2e321efe81639158a2fc1766a1a982dd5ad6cc3fbe69e42cb65e` |
| `zr_vm_lib_system/src/zr_vm_lib_system/env/env.c` | `9660a00e05533afe7172ce4b442ac782495d2870b284d246f28d1906408f7d95` |
| `zr_vm_lib_system/src/zr_vm_lib_system/env/env_registry.c` | `210da89e9e5388e7e69be20dd0c36e545ccc42d8616e52d3d37c126fcc0d3879` |
