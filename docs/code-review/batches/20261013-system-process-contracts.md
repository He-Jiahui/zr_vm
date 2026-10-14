# System Process 宿主回调契约审查

本批审查并补齐 `zr_vm_lib_system` 的 process 叶模块文档，覆盖四个声明/实现文件、全部 29 个源码单元及 4 个只读测试单元（两个测试函数、一个匿名结构体配置、一个 Process 断言块）。台账位于 `docs/code-review/coverage/zr_vm_lib_system_process.tsv`。工作树记录的 HEAD 为 `f9869ed6d330c2492789ef3b054622b5e04309b2`；本批没有改变函数签名、实现表达式或运行行为。Root 已完成严格非注释 token 与行首标记等价性检查。

## 构建与实际调用链

`CMakeLists.txt:141` 将 `zr_vm_lib_system` 纳入产品构建，`zr_vm_lib_system/CMakeLists.txt:5` 通过 `zr_declare_module` 建立模块目标，`zr_vm_common/CommonMacros.cmake:9` 递归纳入模块源文件；因此不能按 `process` 目录名推定代码停用。`module.c:86` 取得 `ZrSystem_ProcessRegistry_GetModule` 的静态 descriptor，聚合器在 `module.c:98` 开始遍历叶模块并在 `:100` 注册。`process_registry.c:56` 定义静态函数表，`:57`、`:58` 分别放入 Sleep/Exit 函数指针及固定一个参数的元数；注册插件读取表项见 `native_binding_registry_plugin.c:971`，调用派发进入 `native_binding_dispatch_lanes.h:277`。描述符 `dispatchFlags=0` 走默认 GC-aware 派发路径（`native_binding_dispatch_lanes.h:245`、`:274`、`:278`）。

参数 helper `ZrLib_CallContext_ReadInt` 在 `native_binding_dispatch.c:1121` 对有符号、无符号及浮点类型分类；成功分支分别在 `:1129`、`:1137`、`:1145` 返回 `ZR_TRUE`。缺参在 `:1118` 调用 `RaiseArityError`，不匹配类型在 `:1147` 调用 `RaiseTypeError`，这两个错误入口在 `native_binding.h:593`、`:589` 声明为 `ZR_NO_RETURN`。因此 Sleep 的普通 `ZR_FALSE` 只来自 `context/result` 空指针检查；Exit 只来自空 `context` 检查。缺参和不匹配类型是 VM 异常，不是 callback 的普通 false 返回。函数 hint 中的 `int` 文本不是运行时类型门禁；实际值类别由共享 `ReadInt` helper 决定。浮点窄化的共享风险仍归属 helper，不在此叶模块重复升级为 BUG。

Windows 的 `Sleep(DWORD)` 与 POSIX 的 `nanosleep` 在调用 OS 线程上阻塞。此时 native callback 仍处于 GC-aware mutator 状态（`gc_domain_mutator.c:683`、`:687`）；stop-the-world blocker scan 不跳过活动记录（`:171`、`:181`、`:188`、`:806`、`:817`），所以 Windows/POSIX 长等待可能延迟全局暂停，具体是否应改用 detached 派发仍是产品策略 TODO。Emscripten `emscripten_sleep` 是伪同步调用：C 续行在 sleep 完成后继续，但实现让出浏览器 event loop，等待期间事件可以执行并重入编译代码，不能套用 OS 线程阻塞或同一 GC 延迟结论。

## 完整源码单元清单

下表列出本批 29 个源码台账单元；每项的调用者、职责、约束、证据及结论均见 coverage TSV 对应行。

| 文件 | 已审单元 |
| --- | --- |
| `zr_vm_lib_system/include/zr_vm_lib_system/process.h` | 文件说明 `@file:2`；include guard `:9`；`ZrSystem_Process_SleepMilliseconds` 声明 `:44`；`ZrSystem_Process_Exit` 声明 `:71` |
| `zr_vm_lib_system/include/zr_vm_lib_system/process_registry.h` | 文件说明 `@file:2`；include guard `:11`；`ZrSystem_ProcessRegistry_GetModule` 声明 `:33` |
| `zr_vm_lib_system/src/zr_vm_lib_system/process/process.c` | 文件说明 `@file:2`；`_POSIX_C_SOURCE` 特性宏条件/定义 `:10`、`:12`；Sleep 实现 `:53`；Sleep 空指针/参数读取块 `:56`、`:60`；Windows 分支 `:64`、`:66`；Emscripten 分支 `:67`、`:70`、`:71`、`:72`；Emscripten 异步配置与借用期 TODO `:71`、`:45`、`:46`、`:47`；POSIX 分支 `:74`、`:78`、`:81`、`:82`；POSIX `time_t` 转换 TODO `:78`、`:79`、`:80`；等待后置结果块 `:86`、`:87`；Exit 实现 `:112`；Exit 参数/结果块 `:117`、`:121`；`TZrInt64` 转 `int` 待核 `:106` |
| `zr_vm_lib_system/src/zr_vm_lib_system/process/process_registry.c` | 文件说明 `@file:2`；`ZR_ARRAY_COUNT` 编译期回退宏定义 `:31`；`ZrSystem_ProcessRegistry_GetModule` 实现 `:49`；静态 `kConstants` `:51`；回调表 `kFunctions` 与两条登记项 `:56`、`:57`、`:58`；hint 表 `kHints` `:63`、`:65`、`:66`；`kHintsJson` 静态字面量 `:69`；聚合 `kModule` descriptor `:75`、`:80`、`:84`、`:92`；返回静态地址块 `:95` |

descriptor、嵌套表及静态字符串只在 provider image 存续期内借用；注册方必须保持该映像驻留至全部借用结束。

三个公开 C 函数注释均按适用情况补齐 Markdown `Summary`、`Parameters`、`Returns`、`Constraints and lifecycle`、`References`，并区分返回状态、VM 异常、借用参数及副作用。无参数 getter 的参数小节说明为空。`kConstants` 进一步说明 descriptor 中的静态声明与 VM 物化后的数组所有权不同；CLI 的 project/inline/REPL 入口在运行前写入参数导出（由台账记录其实际调用证据）。`kHints` 是独立的可读元数据表，不替代函数表元数或 helper 转换；parser hint 消费范围及是否需要 function 参数推断仍按源码 TODO 待核。

## 相关测试审查

`test_module_system.c:3128` 的 `test_system_leaf_modules_expose_new_api_and_owned_types` 在测试 main 中由 `:8130` 注册。函数通过 `:3236` 导入 `zr.system.process`，`:3245` 确认模块存在；`:3297` 取得 `arguments`、`:3298` 检查非空、`:3299` 确认其为数组，`:3300`、`:3301` 分别确认 `sleepMilliseconds` 和 `exit` 导出存在；静态列表 `:3204` 与断言 `:3302`、`:3303` 确认旧别名 `args`、`sleepMs` 不存在。它不检查 arguments 内容、hint 实际消费、callback 参数错误、等待时长、平台分支、GC 延迟、Emscripten 重入或进程退出。

另有 `test_system_root_aggregates_leaf_modules_and_reuses_cached_instances`（定义 `:3034`，注册 `:8124`）：预期表在 `:3040` 定义、process 项位于 `:3045`，导入循环从 `:3064` 开始并在 `:3076` 比较 root 导出与单独导入的对象身份。此项覆盖聚合/缓存边界，不执行 Sleep 或 Exit。当前测试搜索未发现直接调用两个 callback 符号或 `sleepMilliseconds`/`exit` 导出并触发它们的用例；不存在对数值时长、VM 异常、OS signal、Asyncify/JSPI 或进程终止语义的执行信用，本批没有修改测试源码。

修改前记录的 MSVC Debug 基线来自 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/leaf-libraries-before-results.json`，当时 HEAD 为 `4cf9fa362e2fea3789efcea0edb100f02e40ce1e`，四个 Process 源文件哈希固定在同目录 `leaf-libraries-before-pins.json`。`zr_vm_lib_math_static`、`zr_vm_lib_system_static` 和 `zr_vm_official_provider_convergence_test` 构建退出码为 0；官方 provider 测试 9/9 通过。该测试覆盖注册/provider 契约，不覆盖 Process Sleep/Exit；它也不能证明 Emscripten 分支。

## BUG 与 TODO 证据

- **BUG — Windows 等待值窄化：** 经 `process_registry.c:57` 注册、native callback 派发路径可达。`process.c:66` 把非负 `TZrInt64` 窄化为 `DWORD`；`0xFFFFFFFF` 成为 `INFINITE`，可能永久等待，`0x100000000` 成为零。未调用无限等待作验证。平台语义见 [Microsoft Sleep API](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep)。
- **BUG — Emscripten 等待值窄化：** 宏启用时可从相同 descriptor/callback 路径到达。`process.c:70`、`:72` 将毫秒窄化为 `unsigned int` 后传给 sleep；32 位 `unsigned int` 下 `0x100000000` 变为零。MSVC 基线不覆盖此分支；没有运行 Emscripten 构建。
- **BUG — POSIX 中断后仍报告完成：** `process.c:81`、`:82` 忽略 `nanosleep` 错误和剩余时长；收到信号而以 `EINTR` 提前返回后，`:86`、`:87` 仍写 null 并返回 `ZR_TRUE`，没有续睡。错误与剩余时长约定见 [POSIX nanosleep](https://pubs.opengroup.org/onlinepubs/9799919799/functions/nanosleep.html)。
- **TODO — Windows/POSIX GC 延迟策略：** dispatchFlags 默认 GC-aware 且回调在系统调用期间保持活动；是否把长睡眠改成 detached 尚缺少 safepoint/rooting 与产品策略依据，核查入口是 `process_registry.c:54`、`:56`、`:57`、`:58` 和 core mutator 路径 `gc_domain_mutator.c:683`、`:687`、`:171`、`:181`、`:188`、`:806`、`:817`。
- **TODO — Emscripten 异步配置与重入：** 当前仓库 CMake/CI 搜索没有发现 Asyncify/JSPI 配置或实际 Emscripten 目标；`process.c:71` 与 `:72` 是当前异步分支入口，挂起期间 event loop 可运行，事件处理器可能重入 compiled code。需在真实 Emscripten build/test 入口确认使用 `-sASYNCIFY` 或 `-sJSPI`，并检查 dispatcher 的同状态重入行为、挂起恢复时借用 `context/result` 的有效性。当前只记录待核，不断言这些指针必然失效或安全。
- **TODO — POSIX `time_t` 宽度：** `process.c:78`、`:79`、`:80` 把长秒数直接转换为 `time_t`，未校验支持 ABI 的可表示范围；按实际支持 POSIX 目标核实。
- **TODO — Exit 状态码窄化：** `process.c:125` 把 `TZrInt64` 转为 C `int`，公开合法退出码范围尚未找到；不因转换语句单独推定所有超范围值都可由合法调用触发。
- **TODO — function hint 消费：** `process_registry.c:60`、`:63`、`:65`、`:66` 提供 `int` 签名文本，但参数描述符为空；需由 `type_inference_native.c:543`、`:560`、`:3728` 核对 parser 是否预期利用 function hint。该 hint 不是运行时类型门禁。

Emscripten 官方文档将 `emscripten_sleep(unsigned int)` 列为伪同步 API：C 代码在等待完成后续行执行，但底层会让出 event loop，等待期间可能处理其他异步事件，见 [emscripten.h sleeping](https://emscripten.org/docs/api_reference/emscripten.h.html)；Asyncify 章节说明可用 `-sASYNCIFY` 或 `-sJSPI` 支持，并明确列出等待期事件重入的风险，见 [Sleeping / yielding to the event loop](https://emscripten.org/docs/porting/asyncify.html#sleeping-yielding-to-the-event-loop)。这些资料解释平台机制，不证明本仓构建已启用任一配置。

两测试函数、root 的 `kExpectedModules` 匿名结构体/映射配置与 Process 导出断言块已单列台账，
均为 `no-comment`：现有函数名、字段名、期望表和明确断言已表达具体回归场景；本批详细记录
其对象借用至 state 销毁的边界及未覆盖项，不把台账记录声称为新增源码文档。该测试文件由
其他活跃会话修改，本轮只读审查并固定哈希，没有执行这个 module target。

## 验证与边界

台账33项结论：19已注释、6无需注释、3BUG、5TODO，无未审查项。源码新增 BUG/TODO
均对应上述具体完整路径或核查入口。旧 System 总台账只将本四叶源的17项替换为29项，
其他行字节及相对顺序保持；既有失效锚点仍登记，整张 System 表尚未认证。

Root 最后修订后在同一实际 checkout 和同一 MSVC Debug/static 产品目录构建 Math、System
及官方 provider test；构建24.984秒退出0，provider9/9 PASS（1.469秒），各case与修改前
基线完全相同。17源码/测试 pins 和15共享证据 pins 在本次验证前后稳定。九 cases覆盖
provider清单/注册契约，不调用 Process getter 或 Sleep/Exit，不证明各平台等待行为。
本批无 Linux/Emscripten、信号注入或进程退出执行信用；WSL E_ACCESSDENIED 已登记。

Clang19 将 Git 原始 blob 和当前四叶源在内存中做 raw-token 严格比较：非注释的
kind/spelling/StartOfLine 完全一致，同轮 Quaternion四源也相同。未持久化源码副本或token
输出，没有新增测试或修改行为。证据目录是
`E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`，证据文件为
`leaf-libraries-before-pins.json`、`leaf-libraries-before-results.json`、
`leaf-libraries-final-r2-comment-equivalence.json`、`leaf-libraries-final-r2-results.json`。
check-batch 仅机械验证九列、路径与非空行；独立 reviewer 另核对 caller 方向和契约语义。

## 冻结内容身份

最终 raw-lexer 证据 SHA-256：`f3b9c8b267da4e09d3455799976af307c9c43dd7353783353d455fc5655d1dae`。

| 文件 | SHA-256 |
| --- | --- |
| `zr_vm_lib_system/src/zr_vm_lib_system/process/process.c` | `d493ae7a8be64a8939454832219c1767a52f05066c9fbcf733c831211743dd22` |
| `zr_vm_lib_system/src/zr_vm_lib_system/process/process_registry.c` | `a52f1d644a0e1ff7e3d83a3bf709d71292e11b2b8572865d07461ea82022897a` |
| `zr_vm_lib_system/include/zr_vm_lib_system/process.h` | `69653984ab477499e15673582206822b3b373ed61c379bdd24d081bf44da73d1` |
| `zr_vm_lib_system/include/zr_vm_lib_system/process_registry.h` | `8192251570cc14a7cba74503c77cca299731de7dcf340f810188296029f4a7a7` |

只读测试 `tests/module/test_module_system.c` SHA-256：`b2026e11836fc7d57979fd15b253f6621e3aec5be4223af047013843fab427b0`。

台账 SHA-256：`fbad8d57050965eb810ca0b5a5b72ab7d0ac47059e7f61e40dbca0c977530b43`。
