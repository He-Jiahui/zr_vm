---
related_code:
  - tests/library/test_native_registry_oom.c
  - tests/cmake/native-registry-oom-tests.cmake
  - tests/cmake/cli-fixture-targets.cmake
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/CMakeLists.txt
  - zr_vm_core/src/zr_vm_core/runtime/global.c
  - zr_vm_core/src/zr_vm_core/execution/execution_memory.c
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
implementation_files:
  - tests/library/test_native_registry_oom.c
test_files:
  - tests/library/test_native_registry_oom.c
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/cmake/native-registry-oom-tests.cmake
plan_sources:
  - "user: 全量逐单元审查、按需中文注释、保留程序行为并按功能模块提交。"
  - docs/code-review/comment-standard.md
tests:
  - tests/library/test_native_registry_oom.c
  - tests/cmake/native-registry-oom-tests.cmake
doc_type: review-batch
---

# Native registry OOM 场景与所有权契约

## 范围与逐单元结论

本批只交付 tests/library/test_native_registry_oom.c 的当前合同：10个函数、1个包含8字段的类型、1个文件接口和3个有意义块，共15个独立键。类型成员归于 RegistryOomAllocator；三个块分别记录预载与基线顺序、故障窗口与观察、Windows错误报告设置。所有15行均已逐定义、调用者、入口、实际断言、失败状态、所有权与生命周期复核，结论为 commented；逐项证据和理由见[九列台账](../coverage/tests_library_native_registry_oom_current.tsv)。

15个键此前没有物化到覆盖叶。旧报告中的11个定义候选不等于已交付信用；本批是0旧键迁移、15个首次物化键。按Root本次独立单元口径，交付完成后累计由3308变为3323。这里只新增注释和审查记录，不新增函数、类型、测试或行为修复；没有新BUG/TODO家族。只有源文件完成实际提交后才登记交付信用。

## 入口与普通进程协议

实际CMake建立 zr_vm_native_registry_oom_test，并依赖 zr_vm_descriptor_plugin_fixture_int。前者通过Parser/Core/Library链接助手接入当前生产路径，后者来自 tests/language_server/descriptor_plugin_fixture_int.c；夹具是只读加载输入，不归本批修改。main 使用精确argv形式选择九个场景，已识别场景的返回值原样透传；参数数目拒绝返回64，未知selector返回65。这两个拒绝分支没有作为本批新增运行用例。

| selector | 实际场景入口 |
| --- | --- |
| attach_array_1 | test_attach_initial_array_oom(1u) |
| attach_array_2 | test_attach_initial_array_oom(2u) |
| attach_array_3 | test_attach_initial_array_oom(3u) |
| module_record_growth | test_module_record_growth_oom() |
| binding_entry_growth | test_binding_entry_growth_oom() |
| plugin_handle_growth | test_plugin_transaction_oom(1,argv[2]) |
| plugin_string_allocation | test_plugin_transaction_oom(2,argv[2]) |
| plugin_record_growth | test_plugin_transaction_oom(3,argv[2]) |
| plugin_reload_allocation | test_plugin_transaction_oom(4,argv[2]) |

前五个场景只传selector；后四个还传FileAPI识别的实际夹具DLL路径。使用实际B/tests为cwd，九个direct各沿既有CTest的30秒限时；构建900秒是本任务外层上限。任务给每个selector独占的TMP/TEMP/TMPDIR，不把旧CTest共享临时目录当可清理资源。本文件没有Unity，也不输出逐例PASS或总计；stdout/stderr原始分流保留，成功只指实际退出0与所执行的源码断言。

## 分配器、借用和失败状态

RegistryOomAllocator位于场景栈上，必须覆盖global的完整使用和销毁期间。Core复制callbacks的值，并持续借用tracker地址。helper不会自动Attach、停止GC或建立恢复点。分配器只统计ARRAY/GLOBAL类别的块数，不等于字节数、GC对象或OS句柄统计；状态无内部同步，本测试按独立进程串行运行。

非零请求的注入和宿主realloc失败都保留原块所有权；零大小释放不受注入。ARRAY一基序号与增容模式分别控制目标窗口，非零序号优先。GLOBAL一基序号只针对新块请求。准备基线时不arm；注入后撤销窗口、检查各自明确的指针、长度或块数，并按实际普通返回路径释放global。Attach首次失败分支允许在arm状态销毁，释放请求本身不会被拒绝。

fill_array仅将本文件三类已初始化记录数组的空槽清零，并把length推到capacity，不分配、不给槽构造有效模块或绑定。descriptor helper清零输出并浅借用名称和表，不释放先前资源、不校验任意输入合法性；借用输入须保持到registry销毁。占位native callback不读参数、不写result，测试只注册和携带它，没有派发，因此固定TRUE不是已验证的业务返回。

三个Attach场景分别观察初始三数组之一失败的发布状态、两类块数及撤销故障后的重试，不推导所有注册钩子都具事务性。模块增容场景观察按名称查找及临时GLOBAL副本归还，不直接检查记录数组所有字段。绑定增容场景只要求返回假与长度不变，不读取未初始化value；中间closure归Core GC，不断言立即回收。

插件四模式冻结各自预载和容量基线，再让一次加载进入故障窗口。loaded非NULL先短路失败；只有NULL时才继续身份、长度与GLOBAL存量比较。global ordinal3对应已定位的handle.loadedPath字符串分配，不能泛称任意插件分配或re-registration提交。mode4必须先成功预载；previous只比较身份，不在global销毁后使用descriptor。这些断言没有验证注入命中计数，也没有覆盖所有OOM点。

## 源码注释与证据边界

实际应用为 E:\cargo-targets\zr_vm\reports\first-party-review-01a13463\native-registry-oom-tests-current-source-comment-application-r3.json，SHA256 cb4d0c12f1b8b5a9ceec9d1969ed4a8fd05cae387b4b3ed3e593094b50cb6182。只追加12段完整EOF Doxygen；原11273字节、277行程序前缀、字符串、行列和绝对字节位置保持，附录删除可精确恢复原文件。当前源SHA256为 c2e51d812b7cfb29469b76f8598f3686ce8ae99b20064d22dd6dfcb98f3e0842，28257字节、504LF。既有实体由适用file/struct/fn指令关联，三个有意义块复用所属函数文档的顺序和生命周期说明。

源计划的有限手写comment/string-aware atoms及原始位置证明属于静态机械证据，不冒称Clang lexer、AST、ABI或跨平台行为验证。F3和独立I3保持原件；I3唯一29→30修正仅澄清caller名称数量，没有修改15行前八列或源码计划。可检索入口和间接回调按冻结报告的有限范围审查，不把没有文本命中当未使用或不存在动态入口。

## 实际修改前后验证

同一当前配置修改前后各完成1次构建与9个既有普通进程场景，步骤均实际退出0；各4个只读查询均0，14个直接子进程全await且无超时；每场景沿原CTest 30秒，无Unity/PASS输出断言。BEFORE driver PID 11664；AFTER driver PID 12904。两阶段真实外层终态分别由N中的外部witness绑定，报告内部原null字段保持原样，不以controller退出码代替测试进程退出码。

唯一配对为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/native-registry-oom-tests-current-delivery-r1-native-pair.json，SHA256 eebf2841c72482589561ec076a5aea4c650b9de63a508fb363b7193153d835a0，结论 ACCEPT_CURRENT_NINE_CASES；独立复核为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/native-registry-oom-tests-current-delivery-r1-native-pair-independent-review.json，SHA256 c047ed06f1d84767ace3b36a47594e0b54167642989ba07bb5f8b25c82ef013c。源码应用、前后计划和独审、source-state、实际selected closure、results、process、outer及原始流的完整pin随交付报告保留。原准备P中的archive大小写别名按物理身份去重，旧P与两次fresh失败及字典键序逆向比较的写前失败不改写；该更正不是构建或运行证据。

当前执行只证明九个普通进程按原断言结束。没有故障命中次数、全部资源最终零存量、Throw/abort异常清理、全部插件事务或其他平台信用。四个插件场景通过生产加载器在独占cache生成影子DLL；名字与实际PID及有限counter绑定。只允许已批准输入及有限输出，未知文件、读取错误或重解析点会HOLD并保留。卸载和删除尽力而为，原测试不检查其成功；即使子进程退出0，也不能声称影子文件已全部清理。

## 台账、清单与提交边界

新增叶完整锁定15行九列，原定义、调用、入口、断言和约束不被完成理由覆盖。只将源文件一个inventory既有key的sha256/status/note三列刷新，模块、kind和其余全部原始行保持；导航只EOF增加本批一条链接。提交边界为一个源、一个新覆盖叶、本说明、inventory和导航共五路径。全仓pending或其他过期记录保留为各自批次事项，模块check-batch通过不能写作全仓通过。

保留静态F/I、actual app、两阶段原始证据、配对与独立复核、逐单元台账及精确暂存/提交记录。生命周期OPEN，复用当前E盘build/cache；未知或获准残留不自动清理、不重试，手工释放0字节。

首次 BEFORE 在构建成功后因冻结池漏收 `crtdbg.h`、`vcruntime_new_debug.h`、`vcruntime_new.h` 三个系统头而在输入守门退出 1，九个测试未执行；该失败证据保留，后续配对仅采纳补齐精确三针后的真实 retry2 BEFORE 与 AFTER 结果。
