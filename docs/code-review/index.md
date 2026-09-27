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
| 审查清单扫描器 | [10 项](coverage/review_tooling.tsv) | 独立复核清单分类及新增单批锚点检查；10 行台账已迁移为具体证据行号。Windows/WSL 正例通过，缺列、多列、非法路径、字段内换行、坏编码及超长行号负例均按预期失败；当前全仓 `check` 因并发工作树变更仍报告 stale。 |
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
| core 调用绑定、持久化契约与导入重定位 | [111 项](coverage/zr_vm_core_call_binding.tsv) | 16 个 C/H 文件独立复核，源码仅改注释；GCC/Clang 对 12 个实现文件的语法检查通过，未运行完整测试。artifact 读端错误分类和链接分配失败的错误报告已标 2 个 BUG，另有 3 个 TODO。旧台账 15 个失效锚点和其他漂移已按当前调用语义重锚，296 个证据锚点通过独立复核与 `check-batch`。 |
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
| parser 类、接口、结构、联合与属性迁移语法 | [71 项](coverage/zr_vm_parser_syntax_declarations.tsv) | 7 个 C/H 文件独立复核，245 个证据锚点有效，源码仅改注释；GCC/Clang 对 6 个实现文件语法检查通过。26 个 BUG 行含 malformed where 失败清理与属性迁移分配失败路径，另有 2 个 TODO；未运行完整 CMake/CTest。 |
| parser 状态、诊断与 AST 释放契约 | [294 项](coverage/zr_vm_parser_syntax_state.tsv) | 4 个 C/H 文件独立复核，319 个证据条目（其中 3 个以函数名锚定并发变动的调用点），源码仅改注释；GCC 对 4 个翻译单元语法检查通过。5 个 BUG 行涵盖位置换算与临时错误文本寿命，另有 2 个 TODO；未运行运行时测试。 |
| parser 调用展开、闭包捕获与值类型运行测试 | [65 项](coverage/tests_parser_call_value_runtime.tsv) | 3 个 C 文件独立复核，201 个证据锚点有效，源码仅增 70 行注释；GCC 语法检查通过。6 个 BUG 行记录 Unity 失败后清理跳过，另有 3 个覆盖 TODO；未运行运行时测试。 |
| parser 字面量、语法与旧式迁移测试 | [362 项](coverage/tests_parser_syntax_literals_migration.tsv) | 10 个 C 文件独立复核，1,195 个证据锚点有效，源码仅增 37 行注释；GCC 语法检查 10/10 通过。1 个 BUG 行记录未注册 Unity 用例，5 个 TODO 行归于 3 类覆盖疑点；定向构建停在共享 CMake 再生成，未运行测试。 |
| parser 项目导入、规范键与反射压力测试 | [142 项](coverage/tests_parser_project_reflection_runtime.tsv) | 3 个 C 文件独立复核，356 个证据锚点有效，源码仅改注释；GCC/Clang 语法检查通过。16 个 BUG 行含正常路径原生数组与源树泄漏，另有 7 个 TODO；定向构建停在共享 CMake 再生成，未运行测试。 |
| parser 类型、函数、泛型与 extern 语法 | [80 项](coverage/zr_vm_parser_syntax_types_functions.tsv) | 5 个 C 文件独立复核，154 个证据锚点有效，源码仅改注释；GCC/Clang 语法检查通过。26 个 BUG 行记录可达清理缺陷与异步函数修饰符问题，另有 19 个 TODO；定向构建停在共享 CMake 再生成，未运行测试。 |
| parser 错误恢复与 AST 清理测试 | [23 项](coverage/tests_parser_recovery_ownership.tsv) | 1 个 C 文件独立复核，85 个证据锚点有效，源码仅增注释；GCC 语法检查通过。2 个 BUG 行记录装饰器和可变形参 fixture 没有触及声称的路径；未运行运行时测试。 |
| parser 表达式、字面量与插值语法 | [111 项](coverage/zr_vm_parser_syntax_expressions.tsv) | 5 个 C 文件独立复核，567 个证据锚点有效，源码仅增 114 行注释；GCC/Clang 语法检查通过。37 个 BUG 行含计算式对象键空指针解引用及失败路径子树泄漏，另有 4 个 TODO；未运行运行时测试。 |
| parser 引用语法与表达式片段测试 | [49 项](coverage/tests_parser_reference_expression_fragment.tsv) | 2 个 C 文件独立复核，152 个证据锚点有效，源码仅增 23 行注释；GCC/Clang 语法检查 4/4 通过。2 个 BUG 行标记同一处准确 token/范围断言缺口，另有 2 个自动测试归属 TODO；未运行测试二进制。 |
| core 调试、日志与原生调用契约 | [312 项](coverage/zr_vm_core_debug_native_log.tsv) | 16 个 C/H 文件独立复核，956 个证据锚点有效，15 个文件仅改注释；GCC/Clang 对 11 个 C 文件语法检查通过。5 个 BUG 行记录扩栈悬挂指针、物化失败误报、钩子非局部退出、回调计数遗漏和 packed 写回拒绝，另有 3 个 TODO；缺少既有测试二进制，未运行 CTest。 |
| parser 诊断目录公开接口与既有记录勘误 | [13 项](coverage/zr_vm_parser_diagnostic_catalog.tsv) | 2 个公开头文件新增逐单元记录，3 个已审 C 文件注释及原有 [78 项台账](coverage/zr_vm_parser_diagnostics.tsv)同步勘误；两表无重复，GCC 对 5 个源码文件语法检查通过，独立复核通过。旧表原有 2 个 BUG 与 2 个 TODO 保留。 |
| parser 位置、词法器与公开解析入口 | [82 项](coverage/zr_vm_parser_public_lexer_entry.tsv) | 6 个 C/H 文件独立复核，204 个非空证据锚点有效，源码仅改注释；GCC 对 3 个 C 文件语法检查通过。16 个 BUG 行记录词法边界、错误恢复和失败路径缺陷，另有 2 个 Unicode 契约 TODO；未运行运行时测试。 |
| core 原生数组公开接口 | [18 项](coverage/zr_vm_core_array_api.tsv) | 1 个头文件独立复核，9 个函数和 9 个风险块、108 个非空证据锚点，源码仅增 36 行注释；GCC/Clang 语法检查通过。6 个 BUG 行标记已证实的溢出、分配失败和清理状态问题，3 个 TODO 保留可达性与别名契约疑点；未运行运行时测试。 |
| core/parser 模块 API 可见性别名 | [2 项](coverage/zr_vm_core_parser_api_conf.tsv) | 两个配置头文件逐宏核对函数、TLS 数据和内部声明/定义用途，14 个非空证据锚点，源码各仅增一行注释；独立复核通过。实际 Windows 导出疑点沿用通用 API 配置的既有 TODO。 |
| core IO、回调与异常跨层契约 | [338 项](coverage/zr_vm_core_io_callback_exception.tsv) | 8 个 C/H 文件独立复核，源码仅改注释；GCC/Clang 对 5 个 C 文件语法检查通过。36 处源码 BUG/TODO 标签均有独立审查块；跨批复核将线程释放回调风险降为 TODO，因待释放队列生产者未发现。定向测试目标构建停在 CMake VerifyGlobs 预检查，未运行测试。 |
| core 原型元数据布局与常量引用路径 | [26 项](coverage/zr_vm_core_constant_reference.tsv) | 2 个 C/H 文件独立复核，127 个非空证据锚点，源码仅改注释；GCC/Clang 语法检查通过。原型布局有编译/运行消费方，路径解析 API 目前无外部调用；8 个 TODO 记录未来接入前需确认的边界，未标未证实的 BUG。 |
| parser CFG union、switch 与 throw 测试 | [56 项](coverage/tests_parser_cfg_switch_union_throw.tsv) | 3 个 C 文件独立复核，262 个非空证据锚点，12 个 Unity 用例均已登记，源码仅增 55 行注释；GCC/Clang 语法检查 6/6 通过。3 个 BUG 行记录断言非局部退出跳过原生资源释放；未运行完整 CTest。 |
| parser 泛型约束与实例化测试 | [19 项](coverage/tests_parser_generic_contracts.tsv) | 2 个 C 文件独立复核，148 个 evidence 路径锚点有效，源码仅增 15 行注释；GCC/Clang 语法检查 4/4 通过。2 个 BUG 行记录失败断言跳过清理，1 个 TODO 留待真实命名结构体 AOT 用例核对；定向构建停在 CMake 重新生成，未运行 CTest。 |
| core 哈希集合与字符串构建器 | [75 项](coverage/zr_vm_core_hash_set_string_builder.tsv) | 4 个 C/H 文件独立复核，259 个非空证据锚点，源码仅改注释；GCC/Clang 对两个 C 文件语法检查通过。11 个 TODO 保留所有权、极端容量、GC 地址稳定和自追加契约疑点；未运行运行时测试。 |
| parser cast 操作数与逻辑表达式事实测试 | [33 项](coverage/tests_parser_expression_fact_focus.tsv) | 2 个 C 文件独立复核，141 个非空证据锚点、8 个 Unity 用例，源码仅增 33 行注释；GCC/Clang 语法检查 4/4 通过。2 个 BUG 行记录失败退出后的悬挂状态或原生资源泄漏，1 个 TODO 为 logical 目标自动套件归属；未运行完整 CTest。 |
| core 调用帧与缓存链 | [23 项](coverage/zr_vm_core_call_info.tsv) | 2 个 C/H 文件独立复核，157 个非空证据锚点，源码仅增 35 行注释；GCC/Clang 定向语法检查通过。4 个 TODO 留待确认预留状态位、续体、yield 联合体与入口重复清零契约；未运行运行时测试。 |
| parser extern decorator 诊断查询测试 | [41 项](coverage/tests_parser_extern_decorator_diagnostics.tsv) | 3 个 C 文件独立复核，206 个 evidence 与 67 个 callers 锚点有效，源码仅增 48 行注释；GCC/Clang 语法检查 6/6 通过。3 个 BUG 行记录失败断言跳过 AST/编译状态清理，3 个 TODO 记录独立目标未注册 CTest；未运行动态测试。 |
| core 字符串驻留、拼接与格式化 | [77 项](coverage/zr_vm_core_string.tsv) | 2 个 C/H 文件独立复核，169 个 evidence 与 60 个 callers 锚点有效，源码仅改注释；GCC/Clang 定向语法检查通过。8 个 BUG 记录创建失败后解引用、格式串越界及数组插入假成功等可达问题，1 个 TODO 待明确对象占位符契约；未运行运行时测试。 |
| core canonical artifact 消费与类型投影 | [41 项](coverage/zr_vm_core_canonical_consumer.tsv) | 2 个 C/H 文件独立复核，172 个非空证据锚点有效，源码仅增 58 行注释；GCC/Clang 定向语法检查通过。类型、布局与调度器合同的借用期和错误语义已按当前调用链核准；未发现可证实的新 BUG/TODO，未运行运行时测试。 |
| core 会话 checkpoint 的捕获与回滚 | [36 项](coverage/zr_vm_core_session_checkpoint.tsv) | 2 个 C/H 文件独立复核，211 个 evidence 与 128 个 callers 锚点有效，非注释 token 未变；GCC/Clang 定向语法检查通过。4 个 BUG 记录常量恢复、map 失败回滚、扩容所有权及异常退出时 GC 暂停清理，1 个 TODO 待核模块重挂路径；未运行运行时测试。 |
| core 数值幂与值转换宏 | [50 项](coverage/zr_vm_core_math_conversion.tsv) | 2 个头文件独立复核，125 个非空证据锚点有效，源码只改注释；GCC/Clang 普通及 Debug 语法检查通过。7 个 BUG 行涉及零底数幂、整数溢出和长串指针槽误读，1 个 TODO 待核原始函数指针转换 ABI；未运行运行时测试。 |
| parser 编译期导入所有权与调用绑定测试 | [64 项](coverage/tests_parser_import_call_binding.tsv) | 2 个 C 文件独立复核，314 个非空证据锚点与 20 个 Unity 注册有效，源码仅增 49 行注释；GCC/Clang 语法检查 4/4 通过。2 个 BUG 记录断言失败后的旧状态或编译器资源清理缺口，1 个 TODO 待核首个缓存项是否为目标调用；未运行 CTest。 |
| parser FFI wrapper 与 variance 诊断测试 | [26 项](coverage/tests_parser_ffi_variance_diagnostics.tsv) | 2 个 C 文件独立复核，183 个 callers/evidence 锚点有效，源码仅增 36 行注释；GCC/Clang 语法检查 4/4 通过。2 个 BUG 记录断言失败跳过 AST 与编译状态清理，3 个 TODO 记录 CTest 注册缺口及 variance 诊断位置覆盖疑问；未运行动态测试。 |
| core 优化记录存储与发布接口 | [64 项](coverage/zr_vm_core_optimization_remark.tsv) | 2 个 C/H 文件独立复核，280 个非空证据锚点有效，源码仅改注释；GCC/Clang 严格 C11 语法检查通过。1 个 BUG 记录满容量时 `Append` 自别名指针在扩容后失效，3 个 TODO 留待核对发布链、原始 ABI 与跨版本丢弃统计；未运行动态测试。 |
| core 连续视图的切片与索引边界 | [25 项](coverage/zr_vm_core_contiguous_view.tsv) | 2 个 C/H 文件独立复核，86 个 evidence 与 47 个 callers 锚点有效，源码仅增注释；GCC/Clang 语法及定向数组切片测试通过。5 个 BUG 行归纳为索引误报溢出、空尾切片误报边界和成功返回不可验证视图三类，4 个 TODO 保留 GC、布局及代数契约疑问。 |
| core 原生内存、GC 债务与预算分配 | [30 项](coverage/zr_vm_core_memory.tsv) | 3 个 C/H 文件独立复核，145 个 evidence 与 61 个 callers 锚点有效，源码非注释 token 未变；GCC/Clang 定向语法检查通过。3 类 BUG 涉及非原子债务竞争、有符号溢出和并发峰值低报，2 个 TODO 留待核对上游扩容失败与未见仓内调用的债务路径；未运行动态测试。 |
| core 类型入口与全局静态初始化 | [13 项](coverage/zr_vm_core_type_meta_entry.tsv) | 4 个 C/H 文件独立复核，73 个非空证据锚点有效，源码仅增 19 行注释；GCC/Clang 定向语法检查通过。1 个 BUG 标出短串驻留桶扩容失败后 GlobalStaticsInit 将空结果传给永久标记函数的可达路径；未运行运行时测试。 |
| parser typed 布尔跳转、逻辑取反与数值取负测试 | [35 项](coverage/tests_parser_typed_bool_neg.tsv) | 3 个 C 文件独立复核，210 个非空锚点与 4 个 Unity 注册有效，源码仅增 25 行注释；GCC/Clang 语法检查 6/6 通过。3 个 BUG 记录断言失败跳过局部函数和状态清理，3 个 TODO 保留目标是否应注册 CTest 的疑问；未运行动态测试。 |
| core 对象布局映射与代际有效性 | [13 项](coverage/zr_vm_core_object_layout_map.tsv) | 2 个 C/H 文件独立复核，108 个非空证据锚点有效，源码仅增注释；GCC/Clang 定向语法检查通过。4 个 TODO 行归为 shape/布局身份绑定、以及上游 publicLayout/聚合准入证明两类；未发现可证实的新 BUG，现有构建未注册定向运行测试。 |
| core 函数调用展开与解释器/AOT 参数交接 | [15 项](coverage/zr_vm_core_function_call_spread.tsv) | 3 个 C/H 文件独立复核，源码仅增注释；GCC/Clang 定向语法检查通过。2 个 TODO 分别保留继承属性与数组稠密索引的语义边界、以及实参数组固定前分配触发 GC 的疑问；未运行动态测试。 |
| parser const 赋值与接口查询生产者测试 | [22 项](coverage/tests_parser_const_query_producers.tsv) | 2 个 C 文件独立复核，233 个有效锚点，源码仅增 43 行注释；GCC/Clang 语法检查 4/4 通过。2 个 BUG 记录 Unity 断言退出时跳过资源清理，5 个 TODO 保留查询覆盖和 CTest 注册疑问；未运行动态测试。 |
| core 闭包捕获与函数定义身份 | [83 项](coverage/zr_vm_core_closure_identity.tsv) | 5 个 C/H 文件独立复核，281 个有效锚点，源码仅增注释；GCC/Clang 定向语法检查通过。2 个 TODO 保留 AOT shim 投影时 GC 根及共享缓冲身份快速路径的疑问；未运行动态测试。 |
| core 属性引用的创建、装载与写回 | [39 项](coverage/zr_vm_core_property_reference.tsv) | 2 个 C/H 文件独立复核，265 个有效锚点，源码仅增 63 行注释；GCC/Clang 定向语法检查通过。2 个 BUG 标出对象桶初始化或字段插入失败被虚报成功的同一失败链，3 个 TODO 保留 GC 局部根、普通对象保留字段及帧活性疑问；未运行动态测试。 |
| core 批处理形状、视图校验与别名判断 | [30 项](coverage/zr_vm_core_batch_contract.tsv) | 2 个 C/H 文件独立复核，215 个有效锚点，源码仅改注释；GCC/Clang 语法及现有 batch 测试通过，Clang UBSan 复现 `INT64_MIN` 取负溢出。7 个 BUG 行涉及溢出、单元素误拒和负步长重叠漏报，2 个 TODO 保留负步长范围与诊断契约疑问；library 测试台账的反向证据锚点已校正。 |
| parser Span 核心与 GC 视图测试 | [53 项](coverage/tests_parser_span_gc.tsv) | 3 个 C/H 文件独立复核，235 个有效锚点及 16 个 Unity 注册，源码仅增 46 行注释；GCC/Clang 语法检查通过。1 个 BUG 记录断言失败跳过资源清理，2 个 TODO 保留 GC 根/压缩覆盖和 CTest 注册意图疑问；反向证据锚点已校正，未运行动态测试。 |
| core map/string 存储候选契约 | [113 项](coverage/zr_vm_core_container_storage_contract.tsv) | 2 个 C/H 文件独立复核，源码仅改注释；GCC/Clang 语法及现有 Core、parser 合约测试通过。4 个 BUG 行归为布局 stride 对齐遗漏和 rope 自定义相等性准入/复验冲突两类，3 个 TODO 保留执行路径接入与标志位语义疑问；模块文档的 CTest/parser 状态已校正。 |
| parser 数值循环赋值数据流测试 | [26 项](coverage/tests_parser_numeric_assignment_dataflow.tsv) | 2 个 C 文件独立复核，源码各增 16 行注释；GCC/Clang 定向语法检查通过，`language_pipeline` CTest 聚合归属已核。2 个 BUG 记录 Unity 断言失败跳过编译器、AST 和类型资源清理；未运行动态测试。 |
| core 执行预算、异步帧与等待/编译状态 | [104 项](coverage/zr_vm_core_execution_budget.tsv) | 3 个 C/H 文件独立复核，265 个有效锚点，源码仅改注释；GCC/Clang 定向语法及独立 SSA 异步帧测试通过。1 个 TODO 待明确跨线程取消的同步契约，未发现可证实的新 BUG；两份依赖台账的行号证据已校正。 |
| core GC 公开契约、主调度与显式根 | [132 项](coverage/zr_vm_core_gc_main.tsv) | 2 个 C/H 文件独立复核，源码非注释 token 不变；GCC/Clang 定向语法检查通过。37 个 BUG 行记录构造/区段 OOM、异常清理、并发根和遥测、AOT 根帧及 native pin 失败链，8 个 TODO 行保留未接入宏和重叠保活契约；8 份依赖台账的旧 GC 锚点已复位，legacy 台账仍待格式迁移，未运行动态测试。 |
| core 元数据 token 与 ZRP 表格式 | [44 项](coverage/zr_vm_core_metadata_format_headers.tsv) | 2 个头文件独立复核，283 个有效锚点，源码仅改注释；GCC/Clang 定向语法检查通过。1 个 BUG 标出合法但未对齐的 TypeSpec section 被强转读取，3 个 TODO 保留 RID 截断、AOT token 指针序列化及跨 ABI 行格式疑问；未运行动态测试。 |
| core artifact 小端编码与公开身份 | [24 项](coverage/zr_vm_core_artifact_encoding_identity.tsv) | 2 个 C 文件独立复核，源码仅增注释；GCC/Clang 严格语法检查通过。公开 `StatusName` 对五个合法状态返回 `unknown` 已由最小 C 调用复现并标 BUG；4 个 TODO 行保留 token 诊断和跨 ABI 原始结构哈希等边界，关联 artifact 行台账锚点已校正。 |
| core artifact 签名校验与文本往返 | [38 项](coverage/zr_vm_core_artifact_signature_text.tsv) | 2 个 C 文件独立复核，406 个有效锚点，源码仅增 33 行注释；GCC/Clang 定向语法检查通过。2 个 TODO 保留 WriteText/ReadText 输入输出缓冲区重叠契约，未运行完整测试。 |
| core artifact 定长行编解码 | [28 项](coverage/zr_vm_core_artifact_rows.tsv) | 1 个 C 文件独立复核，24 个函数与 4 个风险块；GCC/Clang 严格语法检查通过。公开单行读取缺少节类型、数据范围校验且成功后保留旧诊断状态，最小 C 调用已复现并标 BUG；CallBinding 写入返回值与 DomainTransfer 哈希边界保留 TODO。关联 encoding 与 call binding 台账锚点已校正。 |
| parser 编译器套件入口与调用降级入口 | [147 项](coverage/tests_parser_compiler_suite_mains.tsv) | 2 个 C 文件独立复核，732 个有效锚点；源码仅将空行替换为注释，GCC/Clang 定向语法检查通过。专项目标手工运行，integration 入口属于 language_pipeline CTest；1 个 BUG 记录 W2 pair 分类测试的断言比较方向，未运行完整测试。 |
| parser 编译器回归用例主体 | [115 项](coverage/tests_parser_compiler_regressions.tsv) | 1 个大型 C 测试文件独立复核，源码只把原有空行替换为注释且行号保持；GCC/Clang 定向语法检查通过。48 个 BUG 记录 Unity 硬断言失败跳过尾部资源清理的可达路径，1 个 TODO 保留空守卫语义，未运行完整测试。关联现代台账锚点有效，旧夹具与通用台账仍需迁移格式。 |
| parser 数值 foreach 基数与符号系数读取 | [30 项](coverage/tests_parser_numeric_cardinality_symbolic.tsv) | 2 个 C 测试文件独立复核，214 个证据锚点，源码仅增 35 行注释；GCC/Clang 定向语法检查通过。3 个 BUG 记录 Unity 断言后跳过资源清理及 Array 初始化失败后的 Push 风险，未运行动态测试。 |
| core GC 分片预算与宿主累计 | [56 项](coverage/zr_vm_core_gc_budget.tsv) | 3 个 C/H 文件独立复核，181 个证据锚点，GCC/Clang 定向语法与两项既有预算测试通过。10 个 BUG 记录未初始化预算字段、无效输入后复制结果及阶段诊断等可达缺陷；6 个 TODO 保留位宽、并发和累计语义边界。旧 GC 台账中预算与 ownership 的行号已校正。 |
| core 所有权控制块与 GC 交接 | [73 项](coverage/zr_vm_core_ownership_core.tsv) | 2 个 C/H 文件独立复核，255 个证据锚点；GCC/Clang 对源和头文件的定向语法检查通过。2 个 BUG 记录 SharePlain 后 Unique 重置 strong 及 Value_Copy 后 ReturnToGc 的半提交，1 个 TODO 保留外部 ignore 根契约；7 份关联旧台账的 ownership 行号已重锚，部分旧台账仍有不属于本批的格式/漂移问题。 |
| core 类型布局、复制与初始化 | [119 项](coverage/zr_vm_core_type_layout.tsv) | 3 个 C/H 文件独立复核，546 个证据锚点；GCC/Clang 定向语法、inline_copy 40/40 与两个 CTest 通过。27 条 BUG 台账行归并为 7 组可达缺陷，涵盖显式 GC 表、布局对齐、union tag、嵌套复制和 DROP_NONE 子字段释放；2 条 TODO 保留联合默认初始化疑点。关联 core 测试台账的 24 行布局入口锚点已校正。 |
| core 函数图平坦索引解析 | [7 项](coverage/zr_vm_core_function_graph.tsv) | 1 个 C 文件审查 AOT 展平生产者、运行时元数据和模块绑定调用链，源码仅增宏观注释；GCC/Clang 严格语法检查通过。1 个 BUG 记录暂存分配失败后绑定应用路径直接解引用空结果，关联函数身份台账锚点已校正。 |
| parser artifact 调用绑定与元数据投影 | [15 项](coverage/zr_vm_parser_artifact_projections.tsv) | 2 个 C 文件独立复核，93 个证据锚点，源码仅增 35 行注释；GCC/Clang 定向语法、6 项 call binding 与 3 项 metadata graph 测试通过。2 类 BUG 记录原生属性计数与物化门槛不一致、属性数失配诊断报告相等成员数；3 个 TODO 保留默认原型、声明槽位及同名属性契约。关联身份哈希台账锚点已校正。 |
| core artifact ExecIR 视图与内部接口 | [46 项](coverage/zr_vm_core_artifact_exec_ir_internal.tsv) | 2 个 C/H 文件独立复核，432 个证据锚点，源码只增宏观注释；GCC/Clang 定向语法检查通过。7 条 BUG 台账行归并为 4 类缺陷：失败清零诊断 token、非零行数零步长、失败后残留视图和 codeOffset 越节；1 个 TODO 保留输入输出缓冲区重叠契约。旧 tests_library 台账的证据与失败视图分级已校正。 |
| core 元数据运行时查询与导出 | [20 项](coverage/zr_vm_core_metadata_runtime_queries.tsv) | 3 个 C 文件独立复核，177 个证据锚点，源码仅增注释；GCC/Clang 六项定向语法检查与两项对应 CTest 通过。3 个 TODO 保留 RID 非零、导出标志组合以及跨模块 TypeRef 签名首匹配契约；无反向旧锚点漂移。 |
| parser SemIR 动态退优化测试 | [94 项](coverage/tests_parser_semir_dynamic_deopt.tsv) | 6 个 C 测试文件独立复核，554 个证据锚点，源码仅增 129 行注释；GCC/Clang 定向语法检查通过。12 个 BUG 行记录六处跨分配对象指针顺序比较及六处 Unity 中止后的资源清理缺口；8 个 TODO 保留测试覆盖疑问。目标可执行文件尚未构建，未运行对应 CTest。 |
| core GC 预算、跨域共享、遥测与扫尾 | [28 项](coverage/zr_vm_core_gc_auxiliary.tsv) | 4 个 C 文件独立复核，170 个调用与证据锚点，源码仅增注释；GCC/Clang 语法检查、隔离构建的域桥接 5/5、并发 major 10/10、跨域资源转移 24/24，以及预算单文件测试通过。BUG 标记扫尾计数上限与异常中断后的清理/债务缺口；TODO 保留枚举、计数、拒绝状态和 ShareValue 契约疑问。5 份旧台账共 47 次反向锚点引用已按源码等价行校正；其中 2 份仍有其他历史格式诊断。 |
| parser BZMS 计数范围变体测试 | [90 项](coverage/tests_parser_bzms_range_variants.tsv) | 9 个 C 测试文件独立复核，1,251 个证据锚点、117 个调用条目；原行号保持不变，GCC/Clang 各 9 文件语法检查通过。9 个 BUG 行指向同一共用 helper 在 Unity 断言失败后跳过 compiler state 释放的缺陷；测试可执行文件未构建，未运行目标测试。 |
| language server stdio 帧与 JSON-RPC 信封 | [26 项](coverage/zr_vm_language_server_stdio_frame_envelope.tsv) | 4 个 C/H 文件独立复核，90 个证据锚点、43 个调用条目，源码仅增注释；GCC/Clang 对两份 C 文件的严格语法检查通过。1 个 TODO 保留 Content-Type 媒体类型判定范围疑问；目标生命周期测试未构建运行。 |
| language server stdio 生命周期状态机 | [20 项](coverage/zr_vm_language_server_stdio_lifecycle.tsv) | 2 个 C/H 文件独立复核，接口、状态和通知字段均有调用证据，源码仅增注释；GCC/Clang 严格语法检查通过。1 个 TODO 记录 initialized 通知标志目前仅由测试读取、是否需保留外部可观测状态的疑问。同步校正前一批 stdio 台账的 3 处调用锚点；目标生命周期测试未构建运行。 |
| core GC 域公共契约 | [49 项](coverage/zr_vm_core_gc_domain_public_contracts.tsv) | 2 个 H 文件独立复核，216 个证据锚点，源码仅增注释；GCC/Clang C11 头文件语法检查通过。4 个 TODO 保留公开证明、调用时序和所有权约束疑问；GC 辅助台账的 2 处反向锚点已按当前声明及证明字段校正。 |
| parser CFG 数值条件测试 | [61 项](coverage/tests_parser_cfg_numeric_conditions.tsv) | 2 个 C 测试文件独立复核，385 个证据锚点，覆盖 17 个 Unity 注册；源码仅增 62 行注释，GCC/Clang 四项定向语法检查通过。13 个 BUG 行记录失败断言后跳过资源释放的可达路径，1 个 TODO 保留边界测试缺口。目标测试构建触发大量不相关重编译并已停止，未运行对应可执行测试。 |
| core artifact 公共格式与 ExecIR 接口 | [155 项](coverage/zr_vm_core_artifact_public_contracts.tsv) | 2 个 H 文件逐定义复核，353 个证据锚点，源码仅增注释；GCC/Clang 严格头文件语法检查通过。17 条 BUG 台账行包含公共行读取器共同的节边界缺口、ExecIR 失败状态与诊断问题；5 条 TODO 保留输入重叠、跨模块证明及文本转换契约。5 份旧台账的 44 次行号引用已映射到等价源码行。 |
| language server stdio 请求传输 | [40 项](coverage/zr_vm_language_server_stdio_transport.tsv) | 1 个 C 文件独立复核，覆盖 26 个函数定义与 247 个证据锚点；源码仅增 41 行注释，GCC/Clang C11 严格语法检查通过。1 个 BUG 标明入队内存分配失败后请求 ID 保留，3 个 TODO 记录队列容量、JSON 解析失败分类和停止时阻塞读取的待确认问题。同步修正 2 份旧台账中随注释增行移动的引用；目标运行测试未执行。 |
| language server stdio 生命周期测试 | [38 项](coverage/tests_language_server_stdio_lifecycle.tsv) | 1 个 C 测试文件独立复核，覆盖 10 个函数及状态与关键分支，533 个证据锚点；仅将空行换成注释，保持全部源码行号，GCC/Clang C11 严格语法检查通过。1 个 BUG 标明 JSON 解析分配失败后读取已释放参数，6 个 TODO 记录短写、输出清零、exit 路径观测与故障注入证明缺口；目标测试未构建运行。 |
| parser CFG finally 回归测试 | [36 项](coverage/tests_parser_cfg_finally_abrupt.tsv) | 1 个 C 测试文件独立复核，覆盖 32 个函数、全局状态和 3 个关键块；源码仅增 37 行注释，GCC/Clang C11 定向语法检查通过。1 个 BUG 标明断言失败跳过原生 AST/CFG/context 释放，5 个 TODO 保留合成源长度、break 目标、throw 经 finally、克隆出口归属与 AST 范围的核查入口；目标测试未运行。 |
| language server stdio 输出测试 | [34 项](coverage/tests_language_server_stdio_transport_output.tsv) | 1 个 C 测试文件独立复核，覆盖 16 个函数及输出关键块；源码仅改注释且行号不变，439 个证据锚点，GCC/Clang C11 严格语法检查通过。1 个 BUG 标明断言失败跳过 JSON 树释放，5 个 TODO 记录输出恢复、夹具分配、null 断言和写失败路径的核查缺口；目标测试未运行。 |
| VS Code 扩展客户端请求与虚拟文档 | [24 项](coverage/zr_vm_language_server_extension_client_virtual_documents.tsv) | 4 个 TS/JS 文件独立复核，覆盖桌面和浏览器入口、视图请求及 7 个现有测试；源码仅增注释，TypeScript `--noEmit` 与 7 个 Node 测试通过。1 个 BUG 标明注销虚拟文档提供器后订阅仍持有旧对象；2 个 TODO 保留监听器异常隔离与提供器生命周期测试缺口。 |
| parser CFG 常量条件测试 | [44 项](coverage/tests_parser_cfg_constant_conditions.tsv) | 1 个 C 测试文件独立复核，覆盖 42 个函数、全局状态和断言失败块，278 个证据锚点及 22 个 Unity 注册；源码仅增 46 行注释，GCC/Clang C11 严格语法检查通过。1 个 BUG 标明断言失败跳过原生资源释放，1 个 TODO 保留 if 范围覆盖疑问；目标测试未运行。 |
| language server stdio JSON 解析与内存辅助 | [33 项](coverage/zr_vm_language_server_stdio_parse_memory.tsv) | 3 个生产 C 文件和 1 个测试文件独立复核，覆盖全部 33 个函数及 88 个证据锚点；源码仅增注释，GCC/Clang C11 严格语法检查通过。1 个 BUG 标明测试中 JSON 数值构造分配失败后可能解引用空指针，1 个 TODO 保留大于 2^53 的数值转换精度契约；目标运行测试未执行。 |
| parser CFG 可达性测试 | [64 项](coverage/tests_parser_cfg_reachability.tsv) | 1 个 C 测试文件独立复核，覆盖 58 个函数、全局状态与 5 个关键块，29 个 Unity 注册；源码仅增 30 行注释，GCC/Clang 严格语法检查通过。1 个 BUG 标明断言失败跳过 AST 释放，5 个 TODO 记录分支原因、浮点常量边界及未知布尔选择器的覆盖疑问；同步校正文档将常量 true 用例误作布尔全集穷尽证明的表述，目标测试未运行。 |
| language server stdio 初始化 | [50 项](coverage/zr_vm_language_server_stdio_initialize_flow.tsv) | 2 个生产 C 文件和 1 个测试文件独立复核，覆盖全部函数、宏和关键状态；源码仅增注释，GCC/Clang C11 严格语法检查通过。2 条 BUG 台账行记录初始化响应发送失败后选中项目可能遗留到后续重试的同一缺陷；相关测试经 CMake 动态注册，目标测试未运行。 |
| language server stdio 请求注册与进度 | [71 项](coverage/zr_vm_language_server_stdio_request_coordination.tsv) | 2 个 C/H 模块和 1 个测试文件独立复核，源码仅增注释，GCC/Clang C11 严格语法检查通过。4 个 BUG 记录 NUL 请求 ID 错配、进度 token 截断、end 通知失败后仍回复及 UTF-8 partial 位置未转换。两批共迁移四份旧台账中 37 个不同源码行号引用，并更正输出和初始化测试的注册状态；目标运行测试未执行。 |

表中只列已独立复核并完成文件级状态登记的功能边界；其余文件仍以 `inventory.tsv` 中的 `pending` 为准。共享 `main` 工作树中的其它会话曾把部分尚在审查的注释收入广域 checkpoint，因此验收状态以台账、内容哈希和对应功能验证为准，不以单个 checkpoint 提交名推断完成。
