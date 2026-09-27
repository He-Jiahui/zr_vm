---
related_code:
  - scripts/code_review_inventory.py
implementation_files:
  - scripts/code_review_inventory.py
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
tests: []
doc_type: category-index
---

# 代码审查与注释

本目录记录首方代码的调用意图审查。审查先于注释：每个函数、数据结构、接口及其有意义的控制流都要核对实际调用方、资源契约和失败路径；源码只保留有长期价值的解释。

- [注释与审查规范](comment-standard.md)：范围、证据要求、注释格式、问题标签和提交门槛。
- `inventory.tsv`：当前首方代码候选文件及文件级审查状态；由 `scripts/code_review_inventory.py` 对照工作树更新和核验。
- `coverage/`：按功能模块保存 TSV 审查台账，包含已审查但无需注释的单元。

台账中的调用方和证据是审查时点的事实，不代替源码与构建配置。后续代码变更应重新核对受影响的单元。

## 已验收批次

| 功能边界 | 逐单元台账 | 验证 |
| --- | --- | --- |
| ZRM 容器公开接口与实现 | [53 项](coverage/zr_vm_library_zrm.tsv) | GCC、Clang 的容器测试各 9/9；独立审查发现 2 类已证实缺陷和 3 类待核实边界。 |
| `zr.iteration` 协议 provider | [20 项](coverage/zr_vm_lib_iteration.tsv) | GCC、Clang、MSVC 语法检查通过；GCC 两个模块源文件编译通过，完整测试目标链接因共享构建并发未完成。 |
| CLI 二进制 ZRP 元数据检查 | [24 项](coverage/zr_vm_cli_metadata_dump.tsv) | GCC 定向 CTest 2/2；元数据测试在 GCC、Clang、MSVC 均通过。 |
| `zr.testing` 测试阶段 provider | [68 项](coverage/zr_vm_lib_testing.tsv) | GCC、Clang 语法检查通过；独立 GCC Debug 构建成功，断言测试 12/12；独立审查保留 UTF-8 截断 BUG 与异常快照 TODO。 |
| Wiki 源文件校验器及定向测试 | [31 项](coverage/wiki_source_validation.tsv) | 独立调用链复核通过；Python 单测 5/5，真实 wiki 扫描通过；引用式链接尚未纳入预检，已标 TODO。 |
| 审查清单扫描器 | [8 项](coverage/review_tooling.tsv) | 独立复核临时副本过滤；4 项分类检查通过，两个已跟踪工作副本移出审查范围，正式脚本仍保留。 |
| `zr.math` 数值 provider | [262 项](coverage/zr_vm_lib_math.tsv) | 42 个文件经独立复核，GCC 共享库构建、Clang 20 个 C 文件语法检查与动态插件 smoke 通过；CLI 集成测试未运行。 |
| `zr.container` 集合、视图和池 | [池 128](coverage/zr_vm_lib_container.tsv)、[视图 80](coverage/zr_vm_lib_container_views.tsv)、[模块 249](coverage/zr_vm_lib_container_module.tsv)、[构建 1](coverage/zr_vm_lib_container_build.tsv) | 合计 458 项覆盖 14 文件；GCC Debug 构建与定向测试 55 项通过；另 7 项失败在注释前基线复现，视图测试因共享文件系统配置阻塞未运行。 |
| 历史 `zr_vm_lib_task` 实现 | [225 项](coverage/zr_vm_lib_task.tsv) | 10 个文件经独立复核；顶层当前不构建此目录，GCC 单文件语法检查发现旧代码引用已删除字段（原始版本同样失败），不能视为当前 CLI task provider 的测试结果。 |
| `zr.thread` 调度器与共享封装 | [309 项](coverage/zr_vm_lib_thread.tsv) | 11 个文件独立复核；GCC/Clang 语法检查通过；运行时目标因 VerifyGlobs 卡顿未执行，BUG/TODO 为静态证据。 |
| Wiki 渲染与 ZR 词法着色 | [33 项](coverage/wiki_rendering.tsv) | 8 个配置、插件和测试文件独立复核；注释之外的 Python AST 与配置保持一致，定向测试 7/7 通过。 |
| Wiki 路由清单与主题样式 | [13 项](coverage/wiki_manifest_theme.tsv) | 2 个文件独立复核；JSON 路由由校验器检查，CSS 仅改注释；Wiki 校验和主题测试 4/4 通过。 |
| `zr.system` 宿主能力 provider | [386 项](coverage/zr_vm_lib_system.tsv) | 39 个文件独立复核，非注释 token 未变；GCC 构建、Clang 语法检查通过。assembly 2/2、GC 67/67、provider convergence 9/9 通过；FS 8 项中 2 项、module system 78 项中 6 项、exceptions 8 项中 2 项失败。 |
| `zr.network` TCP/UDP provider | [197 项](coverage/zr_vm_lib_network.tsv) | 13 个文件独立复核，79 个 C 定义均登记；GCC/Clang 语法检查和完整 TCP 帧烟测通过，半帧 EOF 与零字节 UDP 的既有缺陷在烟测中复现。 |
| 根 CMake 与跨平台 CI | [30 项](coverage/root_build_ci.tsv) | 2 个入口文件独立复核，去注释后内容未变；YAML 矩阵解析与 `diff --check` 通过，记录扩展 WASM 构建目标缺失和未消费的依赖开关。 |
| `zr.ffi` 动态调用与 ABI provider | [287 项](coverage/zr_vm_lib_ffi.tsv) | 15 个文件独立复核，100 个 C 定义均登记；GCC shared 构建和 Clang 8 个 C 文件语法检查通过。四个既有测试目标仍有失败（8/30、3/30、1/2、另一个断言中止），未取得同基准对照结果。 |
| 工作区维护脚本（剩余批次） | [392 项](coverage/scripts_remaining.tsv) | 21 个文件完成调用链审查；Python AST、Shell/PowerShell 语法与注释纯度复核通过；魔术常量审计现存 199 项中 7 项失败，已保留风险记录。 |
| 维护脚本测试（剩余批次） | [31 项](coverage/tests_scripts_remaining.tsv) | 3 个文件完成审查；两个 Python 测试 AST 与 PowerShell 非注释文本不变，状态测试 4/4、PowerShell 语法通过；迁移测试 11 项中 1 项因五处 Wiki 旧语法片段失败。 |
| 公共基础类型、宏与 SSA 平台契约 | [632 项](coverage/zr_vm_common.tsv) | 33 个文件完成调用链审查；GCC/Clang 严格 C11 语法与 SSA matrix 测试通过；CMake 签名组合缺陷经隔离复现，另保留 6 个 BUG 与 17 个 TODO。 |
| CLI 回归测试与 REPL smoke | [443 项](coverage/tests_cli.tsv) | 30 个文件完成调用链审查；19 个 JS 语法检查通过，18 个 REPL smoke 为 17 通过、1 个 assignment 失败，语法迁移 golden 另有漂移；两处 BUG 已记录，未取得同基准旧版运行结果。 |
| zr.debug 调试、coverage 与 profile provider | [771 项](coverage/zr_vm_lib_debug.tsv) | 32 个文件完成调用链审查，465 个 C 定义、68 个内部声明与 35 个宏均登记；作者分组 GCC/Clang 语法检查通过，独立整批重跑受共享 WSL I/O 阻塞未完成；Wiki API 契约与 metadata 已校正。 |
| CLI 构建与程序入口 | [27 项](coverage/zr_vm_cli_entry.tsv) | 6 个文件的 11 个宏、4 个 C 定义、2 个接口及分发块完成审查；差异仅新增注释，现有 MSVC Debug CLI 的版本和空优化记录查询均正常退出，未重新构建。 |
| 项目清单、导入与依赖锁 | [275 项](coverage/zr_vm_library_project.tsv) | 16 个文件完成调用链审查，193 个 C 定义及公开声明均登记；GCC/Clang 语法检查通过，定向测试目标构建停在 Ninja glob 复查，运行测试未执行。 |
| 可选 JIT 后端与状态映射 | [122 项](coverage/zr_vm_jit.tsv) | 4 个文件完成调用链审查；GCC/Clang 严格语法和直接链接的可选 JIT 测试通过，JIT ON 配置通过；完整目标构建停在 glob 复查，CTest 未执行。 |
| coverage 与 profile 回归用例 | [15 项](coverage/tests_profile.tsv) | 2 个测试文件完成调用链审查；Clang C11 语法和非注释 token 核对通过，GCC 检查受共享 WSL I/O 阻塞而中断，测试目标未构建；固定行数组容量疑问已标 TODO。 |
| C/Rust binding 与安全封装 | [826 项](coverage/zr_vm_rust_binding.tsv) | 28 文件独立复核，23 个改动文件仅增注释；Rust fmt 与 sys crate 离线检查通过；workspace 检查仍有两处已标 BUG 的 E0283。C 独立目标构建未完成；Wiki 的 session、buffer、错误对象及整数读取边界已校正。 |
| 测试 manifest 二进制往返与 IoSource 生命周期 | [17 项](coverage/tests_artifact.tsv) | 2 文件独立复核、纯注释及 Clang C11 语法检查通过；已标读取失败原生泄漏和断言跳过分配器恢复两处 BUG。Unity 目标构建停在 CMake 重新配置，运行测试未执行。 |
| `zr.testing` 断言、并行 runner 与角色绑定回归 | [53 项](coverage/tests_testing.tsv) | 3 文件独立复核，源码只增注释、Clang C11 语法检查通过；并行 probe 共享计数器数据竞争已标 BUG，失败清理与快照覆盖疑问已标 TODO。四个 Unity 目标构建停在 CMake 重新配置，运行测试未执行。 |
| AOT runtime、typed call 与生命周期边界 | [624 项](coverage/zr_vm_library_aot.tsv) | 11 文件独立复核，非注释词法流不变，Clang C11 语法和 MSVC Debug 共享库构建通过；生成器未引用的 shim 风险记为 TODO，已证实的调用/清理问题记为 BUG。WSL GCC 检查因 I/O 阻塞中断，未计通过。 |
| library 基础状态、批处理、文件与任务运行时 | [312 项](coverage/zr_vm_library.tsv) | 14 文件独立复核，9 个改动源码文件的非注释内容不变；补准 Map 原位布局、global 初始化、ReadAll 释放后访问及整数比较精度问题的契约与 BUG。作者此前 GCC/Clang 语法自检通过；独立编译受本机头文件和 WSL I/O 限制，未计通过。 |
| 异常处理编译与运行时回归 | [31 项](coverage/tests_exceptions.tsv) | 2 文件独立复核，仅改注释；Clang C11 语法检查通过。Unity 断言中断后的 VM 状态清理缺口已标 BUG，两个覆盖疑问及孤立的子目录 CMake 配方已标 TODO；测试目标未完成构建或运行。 |
| 迁移、reference、decorator 与项目调试小边界 | [24 项](coverage/tests_small_contracts.tsv) | 5 文件独立复核，3 个 C 文件仅加注释且 Clang C11 语法通过；迁移缓冲区、reference 文本及 decorator 测试失败后的资源泄漏和过期 GDB 断点已标 BUG。生成 suite 清单只登记来源，未改动；运行测试未执行。 |
| FFI fixture、动态调用与 native extern 回归 | [183 项](coverage/tests_ffi.tsv) | 16 文件独立复核，15 个改动文件仅增注释；WSL GCC/Clang fixture 语法及 GCC SSA ABI 语法检查通过，未执行运行时测试。源码定位测试仍读取旧文件、ZRO 用例遗漏 IoSource 释放及 Release 构建 assert 跳过被测调用，均已标 BUG；9 处待核实边界已标 TODO。 |
| GC 回归、工具与调试探针 | [121 项](coverage/tests_gc.tsv) | 21 文件独立复核，67 个测试函数与 RUN_TEST 一一对应，源码非注释内容等价；7 个 BUG 和 7 个 TODO 均有现行证据。缺少 GC 测试二进制及多数 GDB 输入，未运行测试或探针。 |
| 项目 fixture：基准到 decorator 导入 | [229 项](coverage/tests_fixtures_projects_early.tsv) | 96 文件独立复核，6 个 `.zr` 仅增 8 处中文注释；29 个项目清单可解析。三处现有 CLI 失败均在 HEAD 原样 fixture 复现并标 TODO；四个基准副本仅有手工入口。 |
| iterator、yield 与 task 回归 | [176 项](coverage/tests_iterator_task.tsv) | 10 文件独立复核，72 个测试函数均由入口注册，495 个调用与定义证据复核有效；源码仅改注释。SSA frame budget 已注册到 CTest，其 Release 构建在 `NDEBUG` 下跳过初始化和断言的缺陷已标 BUG；本批未完成构建或运行测试。 |
| 项目 fixture：network、语法参考与 using | [140 项](coverage/tests_fixtures_projects_late.tsv) | 53 文件独立复核，21 个 `.zr` 仅增注释；现有 MSVC CLI 七个项目入口及 testing reference 脚本通过。network loopback 在旧二进制失败待新构建定位，失效的 VS Code 调试路径已标 BUG；WSL 语法参考 C 测试未完成。 |
| 项目 fixture：导入、GC 与 Native 流程 | [108 项](coverage/tests_fixtures_projects_middle.tsv) | 53 文件独立复核，18 个 `.zr` 仅增注释，17 个 JSON 配置可解析；12 个入口及一次 `zr test` 运行通过。现有 CLI 下四个入口的 Semantic IR 失败在去注释副本复现，GC 校验和注释已按真实断言收紧。 |
| reference、parser 与语法迁移 fixture | [168 项](coverage/tests_fixtures_reference_parser.tsv) | 73 文件独立复核，6 个 `.zr` 仅增 7 行注释；迁移输入与 golden 未改。JS smoke 复现旧 `%module` golden 偏差并标 BUG；Python 迁移测试本轮 9/11，另两项分别因 Wiki 旧语法及并行工作树两次扫描不一致失败。 |
| Native binding 注册、分派与元数据 | [664 项](coverage/zr_vm_library_native.tsv) | 23 文件独立复核，非注释 token 不变；GCC、Clang 对 16 个 C 文件语法检查通过，未运行时测试。插件描述符生存期、分配失败路径和公开 API 限制已逐项记录，三个 GC 保活窗口保留 TODO。 |
| 测试 harness、fixture reader 与崩溃保护 | [195 项](coverage/tests_harness.tsv) | 14 文件独立复核，487 个路径行号有效，源码仅改注释；堆/栈 reader 的关闭所有权、Unity 崩溃后的 teardown 跳过和公共 helper 调用者已核准。长 JSON 字段截断假阴性等 2 处 BUG 已标记；构建与运行测试未执行。 |
| 元数据 API 回归与目标注册 | [68 项](coverage/tests_meta.tsv) | 2 文件独立复核，211 个证据锚点有效；GCC C 语法检查通过。重复注册、成功路径假阳性和断言中止后的清理缺口已标 BUG，孤立子目录 CMake 与分配器边界已标 TODO；运行测试未执行。 |
| 容器、代际池与临时值根回归 | [266 项](coverage/tests_container.tsv) | 15 文件独立复核，547 个证据锚点有效；源码只新增 162 行注释，CMake 配置通过，未完成编译或运行测试。池扫描回调、线程入口和跨目录公共 helper 的调用方已核准，7 处待查边界标为 TODO。 |
| 性能采样、报告与 persistent 协议回归 | [191 项](coverage/tests_performance.tsv) | 14 文件独立复核，320 个证据锚点有效；源码仅改注释，本机 GCC Windows 分支语法检查通过。退出码、内存采样、const 写入与 OOM 中位数等 7 处 BUG 和 5 处 TODO 已有静态证据；完整构建和运行测试未执行。 |
| 编译期、函数、指令、系统与测试入口余项 | [541 项](coverage/tests_small_remaining.tsv) | 33 文件独立复核，源码仅改注释；Clang 对 12 个 C 文件及 GCC 对 test_runner 的语法检查通过。`test_runner` 的退出码截断和参数未转义已在 WSL 复现；Unity 失败后清理跳过、文件写入错误漏报及停用的子目录 CMake 等风险已标 BUG/TODO。system_fs 直接语法检查缺生成头文件；未运行完整套件。 |
| Benchmark 计时口径、持久命令、采样策略与环境证据 | [21 项](coverage/tests_cmake_benchmark_contracts.tsv) | 4 个 CMake helper 独立复核，源码仅改注释；四个脚本解析以及 Task 3、Task 4 定向契约脚本通过。Linux 环境报告路径含双引号导致最终 JSON 无效的可达缺陷已标 BUG，未修改行为。 |
| 测试清单、宿主库搜索路径与 fixture 生成入口 | [16 项](coverage/tests_cmake_harness.tsv) | 6 个 CMake 脚本独立复核，源码仅改注释；通用注册器和宿主环境桥接可单独解析，未运行聚合套件。七个直接包含点与多配置 DLL 回退已核准；Linux 构建路径含双引号使生成的 FFI 头文件无效，已标 BUG。 |
| 二进制元数据、导入所有权与调用绑定测试注册 | [13 项](coverage/tests_cmake_registration.tsv) | 6 个 CMake 脚本独立复核，81 个证据锚点有效且源码只增注释；CTest 回归目标与仅供手工计时的辅助目标已区分。未重构建或运行聚合套件。 |
| CLI REPL、运行时报告与测试执行 | [217 项](coverage/zr_vm_cli_repl_runtime.tsv) | 17 文件独立复核，241 个证据锚点有效，源码仅增注释；GCC/Clang 语法检查通过，未运行完整 CLI 测试。堆摘要、profile、coverage 的写入失败和参数生命周期等保留 9 个 BUG、6 个 TODO。 |
| 模块元数据、反射与动态泛型回归 | [841 项](coverage/tests_module.tsv) | 29 文件独立复核，2886 个本地证据锚点有效，源码差异仅注释/空行；7 个 TODO 和 2 个 BUG 均按现行调用链登记。GC 对象地址判断经 generational major 路径复核后未误标 BUG；未编译或运行本批测试。 |
| library 项目、绑定、SSA 与 ZRM 回归 | [287 项](coverage/tests_library.tsv) | 20 文件独立复核，569 个证据锚点有效，源码只改注释；WSL GCC 对 19 个 C 文件及头文件语法检查通过，未运行测试。ZRM CTest 注册缺口等 17 个 BUG 和 20 个 TODO 留证；SSA 失败输出因缺少调用契约已降为 TODO。 |
| 调试协议、线程、快照与诊断回归 | [423 项](coverage/tests_debug.tsv) | 30 文件独立复核，源码仅增注释/空行；GCC/Clang 对 16 个 C 翻译单元语法检查通过，未运行 CTest。23 个 BUG 和 14 个 TODO 均有当前证据；Unity 失败时跳过线程和 VM 清理的路径已按 join 前后分开说明。 |
| CLI 命令解析、编译、迁移与项目处理 | [280 项](coverage/zr_vm_cli_front.tsv) | 16 文件独立复核，192 个 C 定义和 48 个公开声明均入账；1322 处首方引用反向核对无缺，源码只改注释。WSL 8 个 C 文件语法检查通过，未运行 CLI 套件；记录 `recordSlot` 扩容悬空等 19 个 BUG 与 14 个 TODO。 |
| Benchmark 性能套件编排 | [41 项](coverage/tests_cmake_performance_suite.tsv) | 1 个 CMake 脚本独立复核，18 个函数及 145 个证据锚点有效，源码仅增注释；脚本在预期缺少 CLI_EXE 前置条件处退出，未运行完整基准。旧 profile 误收、GC 配对门控及未筛选工具链预构建等 5 个 BUG 和 2 个 TODO 留证。 |
| Benchmark 注册与 Task 3 环境门控 | [49 项](coverage/tests_cmake_benchmark_registration.tsv) | 8 个 CMake 脚本独立复核，173 个证据锚点有效且只改注释；registry、Task 3 和 Task 4 三项可直跑契约测试通过，缺 runner/fixture 二进制的两项仅检查脚本入口。Task 3 环境失格仍保留原始 gate 和比值的两处 BUG 已用最小夹具复现。 |
| AOT ZRP 元数据裁剪与发布 | [341 项](coverage/zr_vm_aot_metadata.tsv) | 26 个 C/H 文件独立复核，924 个证据锚点有效，源码仅增注释；GCC/Clang 对 13 个实现文件的语法检查通过，未运行完整构建。空成员映射、签名重写借用期及发布失败边界已校正，保留 12 个 TODO。 |
| core 调用绑定、持久化契约与导入重定位 | [111 项](coverage/zr_vm_core_call_binding.tsv) | 16 个 C/H 文件独立复核，源码仅改注释；GCC/Clang 对 12 个实现文件的语法检查通过，未运行完整测试。artifact 读端错误分类和链接分配失败的错误报告已标 2 个 BUG，另有 3 个 TODO。 |
| CLI 脚本套件编排 | [53 项](coverage/tests_cmake_cli_suite.tsv) | 1 个 CMake 脚本独立复核，覆盖 13 个函数及 35 个案例，源码仅增注释；未知 `TIER` 空跑成功退出已标 BUG，另外保留 3 个 TODO。脚本定向检查通过，未运行完整 CLI 套件。 |
| 项目 fixture 脚本套件编排 | [67 项](coverage/tests_cmake_projects_suite.tsv) | 1 个 CMake 脚本独立复核，覆盖 44 个项目案例与 9 个函数；源码仅增注释，脚本解析及未知 `TIER` 空跑复现通过。未知档位无案例仍成功退出已标 BUG，删除目标与 binary 回退保留 2 个 TODO。 |
| 归档 AOT runtime、ABI 与测试脚本 | [875 项](coverage/zr_vm_aot_tests_runtime.tsv) | 33 文件独立复核，2351 个证据锚点有效，源码差异仅注释；根构建使用现役 runtime，归档 runtime 重接时的生成 helper 接口缺口已按非穷举集合标 BUG。累计保留 45 个 BUG、44 个 TODO；未运行完整构建或测试。 |
| 多语言 benchmark runner、案例与注册检查 | [1015 项](coverage/tests_benchmarks.tsv) | 172 文件两轮独立复核，2477 个本地证据及 1830 个带行号 caller 引用有效；源码差异仅注释。8 个 BUG 和 1 个 TODO 留证；Node 缩放输入定向检查通过，未运行完整构建或 CTest。 |
| core GDB 调试脚本 | [38 项](coverage/tests_core_gdb.tsv) | 23 个脚本两路独立复核，98 个非空证据锚点有效，差异仅新增注释；断点命令内 `finish` 后的采样失效等 7 个 BUG 和 8 个 TODO 留证。旧调试二进制不可用，未运行脚本。 |
| core 调用与执行路径回归 | [696 项](coverage/tests_core_execution.tsv) | 24 个 C/头文件独立复核，2188 个证据锚点有效；源码仅改注释，GCC/Clang 对实现文件的语法检查通过。13 个 BUG 与 15 个 TODO 留证；未运行完整测试套件。 |
| core GC、资源转移与字符串回归 | [229 项](coverage/tests_core_gc_resource.tsv) | 14 个 C/头文件独立复核，838 个证据锚点有效；源码仅改注释，GCC/Clang 对 12 个实现文件的语法检查通过。33 个 BUG 与 9 个 TODO 留证，其中 8 个未注册测试目标待核查；未运行 CTest。 |
| core UTF-8 验证、解码与编码边界 | [12 项](coverage/zr_vm_core_utf8.tsv) | 2 个 C/H 文件独立复核，57 个证据锚点有效，源码仅新增注释；GCC C11 语法检查通过。编码代理区码点生成无效 UTF-8 已标 BUG，码点偏移仅验证前缀的契约已写明；未运行完整测试。 |
| core 热更新与迭代器帧 | [63 项](coverage/zr_vm_core_hotpatch_iterator.tsv) | 9 个 C 文件独立复核，263 个证据锚点有效，源码仅新增注释；GCC/Clang 语法检查通过。跨 manager 句柄、并发代际读取、迭代值根与异常退出等现存缺陷已标 BUG；未运行完整测试。 |
| core 模块加载、导入签名与反射分派 | [188 项](coverage/zr_vm_core_module.tsv) | 14 个 C/H 文件独立复核，241 个证据锚点有效，150 个 C 函数定义均入账；源码只改注释，GCC/Clang C11 语法检查通过。8 个 BUG、15 个 TODO 涉及导入、契约与所有权边界；未运行完整测试。 |
| parser GDB 调试脚本 | [70 项](coverage/tests_parser_gdb.tsv) | 30 个非 SSA 脚本独立复核，183 个证据锚点有效，命令内容未变；断点、栈帧和观察位置的现有缺口保留 26 个 BUG 与 10 个 TODO。GDB 12.1 最小实验验证嵌套命令内注释不改行为；预设调试二进制缺失，未运行完整脚本。 |
| AOT C lowering 的值、控制流与通用类型转换 | [338 项](coverage/zr_vm_aot_c_lowering.tsv) | 22 个 C 文件三组及整批独立复核，855 个证据锚点有效，源码仅改注释；GCC C11 语法检查通过。27 个 BUG 与 26 个 TODO 覆盖转换可达性、所有权、CFG 活性等边界；未运行完整 AOT 测试。 |
| AOT LLVM 文本生成与 lowering | [244 项](coverage/zr_vm_aot_llvm.tsv) | 42 个 C/H 文件独立复核，1228 个调用与证据锚点有效，源码仅改注释且去注释 token 与原版一致；未运行完整构建。最终写入错误漏报、模块字符串未转义已标 BUG；主构建边界和整型溢出契约保留 TODO。 |
| parser 诊断构建、复制与消息目录 | [78 项](coverage/zr_vm_parser_diagnostics.tsv) | 8 个 C 文件独立复核，195 个证据锚点有效，源码仅新增注释；GCC C11 语法检查通过。94 个描述符与 71 对消息的缺口、测试中固定的 71 项预期均按现行代码标 BUG；另有 2 个 TODO，未运行诊断测试。 |
| parser 旧语法迁移与非 SSA writer | [112 项](coverage/zr_vm_parser_migration_writer.tsv) | 10 个 C/H 文件独立复核，284 个证据锚点有效，源码仅改注释；GCC/Clang 对 7 个实现文件的语法检查通过。迁移词法边界和三个 writer 关闭失败路径已标 BUG，其他疑点保留 TODO；未运行可执行测试。 |
| AOT IR 适配、可达性与链接档位 | [258 项](coverage/zr_vm_aot_ir_adapter.tsv) | 28 个 C/H 文件独立复核，529 个本地证据锚点有效，27 个改动文件仅改注释；GCC/Clang 对 15 个实现文件语法检查通过。自引用函数常量预扫描递归与禁止桥接时清零 descriptorOnly 已标 BUG，另保留 2 个 TODO；未运行完整 AOT 测试。 |
| AOT C 写入器、帧与调用边界 | [579 项](coverage/zr_vm_aot_c_writer.tsv) | 29 个 C/H 文件独立复核，1378 个本地证据锚点有效，源码仅改注释；GCC 对 16 个实现文件语法检查通过。未转义的选项及 manifest 文本进入生成的 C 字符串/注释已标 BUG，短链 CFG 前驱等疑点保留 TODO；未运行完整 AOT 测试。 |
| AOT C 标量、值布局与泛型共享 | [557 项](coverage/zr_vm_aot_c_scalar_layout.tsv) | 34 个 C/H 文件独立复核，1357 个证据锚点有效，22 个改动文件仅增注释；GCC/Clang 对 18 个实现文件语法检查通过。用户自定义值类型泛型实参的文本分类仍待核，已标 TODO；未运行完整 AOT 测试。 |
| core 热更新与迭代器公开接口 | [84 项](coverage/zr_vm_core_hotpatch_iterator_api.tsv) | 9 个头文件独立复核，353 个证据锚点有效，83 个声明/类型/宏及兼容包含块均入账；GCC/Clang C11 语法检查通过。跨 manager 句柄、并发 Resolve、迭代器异常清理等已证实缺陷与受限状态枚举疑点均按契约标注；未运行完整测试。 |
| core AOT 装箱与拆箱 bridge | [5 项](coverage/zr_vm_core_bridge.tsv) | 2 个 C/H 文件独立复核，19 个当前调用与实现锚点有效，差异仅新增注释；GCC/Clang C11 语法检查通过。公开拆箱入口的源原型匹配要求尚不明确，已标 TODO；未运行 AOT 测试。 |
| AOT C 共享库与本地执行 smoke | [684 项](coverage/tests_parser_aot_smoke.tsv) | 27 个测试文件独立复核，2664 个证据锚点有效，源码仅增 97 行注释；GCC 对 27 文件语法检查通过。13 个目标属于 CTest 聚合，14 个有手工验收入口但注册意图待核，已标 TODO；完整构建停在旧 WSL 树 VerifyGlobs，未运行套件。 |
| AOT C 早期调用、控制、泛型与代码裁剪契约测试 | [514 项](coverage/tests_parser_aot_contracts_early.tsv) | 27 个 C/H 文件独立复核，1727 个证据锚点有效，源码仅增 390 行注释；GCC 对 22 个实现文件语法检查通过。61 个 BUG 行含已证实的 code-stripping 正常路径泄漏与 Unity 失败清理，另有 9 个 TODO；完整 CMake/CTest 未完成。 |
| AOT C 元数据裁剪、发布与可达性测试 | [226 项](coverage/tests_parser_aot_metadata_reachability.tsv) | 11 个 C 文件独立复核，1058 个证据锚点有效，源码仅增 191 行注释；GCC C11 语法检查通过。11 个 BUG 行记录 Unity 断言失败后的清理缺口，TypeDef 目标未注册 CTest 的意图保留 TODO；完整 CMake/CTest 因 WSL 挂载盘 I/O 未完成。 |
| AOT C 后期类型调用与值契约测试 | [232 项](coverage/tests_parser_aot_contracts_late.tsv) | 19 个 C/H 文件独立复核，484 个证据锚点有效，源码仅增 255 行注释/空行；GCC 对 17 个实现文件及头文件包含入口语法检查通过。20 个 BUG 与 5 个 TODO 按实际调用链登记；未运行完整 AOT 测试。 |
| core 哈希盐、稳定身份与节点契约 | [19 项](coverage/zr_vm_core_hash.tsv) | 2 个 C/H 文件独立复核，84 个证据锚点有效，源码仅改注释；GCC 语法检查通过。种子缓冲未定义行为及哈希失败被后续折叠均标 BUG，未使用的比较回调保留 TODO；未运行运行时测试。 |
| AOT C 反射调用与类型化 thunk | [459 项](coverage/zr_vm_aot_c_thunks.tsv) | 44 个 C/H 文件独立复核，1524 个证据锚点及直接/间接调用者已核实，源码仅增 144 行注释/空行；GCC 对 22 个实现文件语法检查通过。9 个 BUG 行包括 i64 有符号运算边界及非有限 f64 常量生成无效 C；未运行完整 AOT 测试。 |
| AOT C 测试支持头与类型调用辅助 | [75 项](coverage/tests_parser_aot_support_headers.tsv) | 10 个头文件独立复核，239 个证据锚点有效，源码仅增 66 行注释；GCC 对 13 个包含它们的宿主翻译单元语法检查通过。12 个 BUG 行记录 Unity 断言失败跳过清理的路径；未运行完整测试。 |
| 编译 fixture、BufferPool FFI 与 W2 quickening 回归 | [125 项](coverage/tests_parser_compile_fixtures_quickening.tsv) | 7 个 C/H 文件独立复核，400 个证据锚点有效，源码仅增 51 行注释；GCC 对 5 个实现文件语法检查通过。4 个 BUG 行覆盖计数断言方向和融合调用扫描漏检，4 个 TODO 行保留延迟写失败与 CTest 注册疑点；未运行运行时测试。 |
| AOT 源码契约与跨模块绑定投影辅助 | [30 项](coverage/tests_parser_aot_binding_support.tsv) | 3 个 C/H 文件独立复核，182 个证据锚点有效，源码仅改注释；GCC/Clang 对 3 个宿主翻译单元语法检查通过。5 个 BUG 行记录 Unity 断言失败跳过局部资源清理；未运行完整 AOT 测试。 |
| parser 调用参数、后缀调用与所有权语法 | [21 项](coverage/zr_vm_parser_syntax_calls_ownership.tsv) | 5 个 C 文件独立复核，96 个证据锚点有效，源码仅增 30 行注释；GCC/Clang 语法检查通过。3 个 BUG 行记录分配失败后的对象释放缺口，另有 2 个 TODO；共享 WSL I/O 阻塞定向 CMake/CTest。 |
| parser 工件、元数据与逃逸流水线测试 | [117 项](coverage/tests_parser_artifact_metadata.tsv) | 8 个 C/H 文件独立复核，490 个证据锚点有效，源码仅增 123 行注释；GCC 对 7 个实现文件及头文件包含入口语法检查通过。22 个 BUG 行含失败断言跳过清理，另有 2 个 TODO；未运行可执行测试。 |
| parser 语句、循环、switch 与 yield 语法 | [82 项](coverage/zr_vm_parser_syntax_statements.tsv) | 4 个 C 文件独立复核，205 个证据锚点有效，源码仅改注释；GCC C11 语法检查通过。30 个 BUG 行记录错误退出清理与重复 switch default 等可达问题，另有 5 个 TODO；既有构建未登记可运行 parser CTest。 |

表中只列已独立复核并完成文件级状态登记的功能边界；其余文件仍以 `inventory.tsv` 中的 `pending` 为准。共享 `main` 工作树中的其它会话曾把部分尚在审查的注释收入广域 checkpoint，因此验收状态以台账、内容哈希和对应功能验证为准，不以单个 checkpoint 提交名推断完成。
