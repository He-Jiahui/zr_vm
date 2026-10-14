---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/vm.h
  - zr_vm_lib_system/include/zr_vm_lib_system/vm_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm_registry.c
  - zr_vm_lib_system/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - tests/module/test_module_system.c
  - zr_vm_core/src/zr_vm_core/runtime/state.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/include/zr_vm_core/gc.h
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/vm.h
  - zr_vm_lib_system/include/zr_vm_lib_system/vm_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 System VM callback contract audit and failure-path review
tests:
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
  - tests/fixtures/reference/core_semantics/imports/native_root_member_chain_pass.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr
  - tests/cmake/run_projects_suite.cmake
  - tests/parser/test_type_inference.c
  - tests/language_server/test_lsp_interface.c
doc_type: workflow-detail
---

# System VM callback 契约审查

## 范围和逐单元清点

本批审查 System VM leaf 的两个声明头文件和两个实现文件，并建立唯一叶级台账 coverage/zr_vm_lib_system_vm.tsv。台账共 29 行：25 个 guard、接口、函数或静态 descriptor/config 单元，以及 4 个独立失败/时序块；结论为 12 commented、3 no-comment、10 TODO、4 BUG。

四个源码文件进入实际 System 静态库的路径为 zr_vm_lib_system/CMakeLists.txt:1-8 导入 CommonMacros.cmake，后者 :9-11 收集模块 src/*.c；System 根注册器在 zr_vm_lib_system/src/zr_vm_lib_system/module.c:89 调用 ZrSystem_VmRegistry_GetModule。本批覆盖该 VM leaf；Root 将旧 System 总表中的对应27行迁移到本叶台账（其余275行原样保留），更新 System API/概览，并修正三份既有测试台账的12个反向证据单元。共享 dispatcher、Core 与测试实现保持本批修改范围之外。三份测试台账只更正相关反向证据；未据此宣告并发占用的 C 测试源码已经完成文档级注释审查。

- vm.h 4 行：include guard 与三个公开 callback 声明。
- vm_registry.h 2 行：include guard 与 descriptor getter。
- vm.c 14 行：7 个私有函数、3 个 callback，以及 4 个独立语义块。
- vm_registry.c 9 行：条件备用宏、两个字段表、类型/函数/hint/JSON/module 静态表和 getter。

最终冻结 SHA-256 与行数为：vm.h（74 行）71D904C1611039F81B7904201637DFFFB6449F03CCA7FAB9F7666915FA3AE750；vm_registry.h（25 行）37FE44E11C41671E5AA27FA8D8F46BFDE5BB016D2EC278714464551DDE132897；vm.c（458 行）06083DB9201220F6840DA6163AE45D3FEA67234FB1721D1248B2DB4E981543CB；vm_registry.c（97 行）B424380AAB70D3BF89E533631BF78FAE0CC067547ED3FFA839FD1D67EC0FF6B6。

## 注册与调用链

vm_registry.c:76-94 的静态 kModule 将函数、类型、字段、hints、schema JSON、版本和 ABI 聚合为 zr.system.vm descriptor；:96 返回其地址，System root 在 module.c:89 注册它。dispatcher 从函数表取 callback 并在进入 callback 前检查注册元数：kFunctions 的 loadedModules、state 分别是零参，callModuleExport 固定三参；native_binding_dispatch.c:362-365 的 arity 检查先于 dispatch lane 的函数指针调用。注册元数错误不会到达 callback。ReadString 等参数读取对实际类型错误走 VM 异常路径；这与 callback 以 false 报告防御性 context/result 或普通分配失败不同。

kTypes 与字段表供 native metadata layer 建立 VM 原型；kHints 与 kHintsJson 提供静态分析/registry 元数据，不替代运行时类型检查。descriptor 的嵌套数组、字符串和 callback 都是静态借用指针。registry 对 descriptor 的浅保存不延长定义它们的 provider image 生命周期，动态 provider 必须在任何已注册指针仍可能被访问期间保持映像驻留。

loadedModules 从当前 state 所属 global 的 registry 遍历条目，按 NativeRegistry 元数据构造 SystemLoadedModuleInfo 并最终把数组放入 result；state 将 global collector 的模式/debt/threshold 与当前线程的 stack/frame 深度组合成 SystemVmState。两类 snapshot 读取和 VM 对象/字段物化不是一个原子操作。

callModuleExport 借出模块名和导出名字符串，先把参数数组元素浅拷贝到临时 native allocation，再委托 ZrLib_CallModuleExport。Core 查找目标 export 时可能触发 lazy import；本 callback 在 helper 返回后释放临时参数缓冲。导入时是否有足够的 VM root 覆盖浅拷贝 values 与借用名称、以及局部异常处理建立前发生非局部跳转时的清理边界仍记为 TODO。

## 已证实缺陷与未闭合契约

本批记录的四个 BUG 都限定在已闭合的合法调用路径和具体软失败，不代表做过故障注入。

system_vm_write_string_field 先以 ZrLib_Value_SetString 创建字符串，再把局部 fieldValue 交给 void ZrLib_Object_SetFieldCString。合法 sourceKind 短串（native 或 source）遇到 string cache miss 时，如果 HashSet bucket 扩容的 ZrCore_Memory_Allocate 返回 null，Core 返回 null 而不抛 VM 异常；SetString 因而没有写入 fieldValue，后续 void setter 仍继续读该值。该链可能使新 metadata 对象得到未定义/错误字段值。范围不包含会通过 GcMalloc 抛出并非局部跳转的失败。

system_vm_make_state 与 system_vm_make_loaded_module_info 均通过 void setter 逐字段构造新对象；返回非空只表示对象构造成功，不保证每个字段都已写入。各自已核实的窄风险有三类：对象首次 nodeMap 创建及 setter 内重试都遇到软 bucket 分配失败，setter 记录后返回而 factory 继续；初始容量 4、3/4 threshold 下，写到第四个总字段时后续 bucket Rehash 软失败，void 写入仍继续；ignored-root pin table 扩容软失败使对象/key 固定失败并由 void setter 吞掉。value 为 bool 不豁免 nodeMap 扩容；合法来源模块先写 name/sourceKind/sourcePath 且没有 registrationKind 时，hasTypeHints 正是第4字段，也可触发该 HASH_BUCKET rehash。源码 bool helper 的生命周期说明只是非穷举风险列举，没有承诺免扩桶。SystemVmState 的 native-field-layout 用例核验字段布局，不制造这些失败；SystemLoadedModuleInfo 的相关用例也只核验原型/字段信息。

LoadedModules 在 Array_PushValue 返回 bool 后没有检查结果。Core 在 ignored-root pin table 所需的 RawMallocWithType 软失败时可返回 false 且不设置 VM 异常；callback 忽略 false，最后仍把数组发布并返回 true，因此当前已加载模块列表可能静默少一项。

待核问题分四组；各接口/所属函数与独立语句块在台账分别记录。首先，遍历期间 registry、bucket pair、module/name/sourcePath 与 descriptor 被借用，后续 VM 字符串/对象分配是否可能搬迁或失效需沿实际 GC root 协议和 plugin image 生命周期确认。另须核同 global 多 mutator 的调用限制：State_New/State_MutatorLaunch 可建立另一执行者（runtime/state.c:61-70,173-182；静态测试入口 tests/thread/test_thread_runtime.c:652-658），Module_AddToCache 在 module/module.c:300-316 写共享 registry；gc_domain_mutator.c:316-383 的 MutationBegin 并非始终持有模块表专属锁。需追踪受支持的并发导入/快照是否由调用方串行化、以及现有同步协议；本批没有并发压力执行证据，未闭合前仅为 traversal TODO，不标 BUG。state() 的 count helper 也遍历该 registry，factory 又读取共享 gcMode/gcDebtSize/gcPauseThresholdPercent；需沿 gc.h:177,199,204 和 gc.c:658-671 的写路径核实 mutator 协议/调用方同步。不是同一瞬间的快照与读写是否受保护是两个问题，后者同样保留 TODO。其次，reviewer 发现此前单列的 traversal 指针 TODO 没覆盖局部 array 和条目 info：vm.c:321 创建 array，到 callback 末尾 :396 才写入 result；info 由 :285 创建，在每个字符串 setter 中 :291-308 先做字符串分配，后调用字段 setter 固定对象。当前证据没有证明受支持协议在这段窗口自动 root 两个对象；后续应沿 RawObject_New、callback roots、Object_SetFieldCString 的 pin 顺序和受控 GC 压力入口核实，暂不升级为 BUG。最后，CallModuleExport 在局部异常保护范围建立前可能查找/导入目标模块；需验证借用字符串、浅拷贝 VM values、GC 与 longjmp 的完整时序。

计数转换另保留 source vm.c:156 的 TODO：loaded_module_count 以 TZrSize 累加后在 :186 转为 TZrInt64；需结合实际类型宽度、registry 可达容量上界和超范围转换约定核实，尚未证明可触发，不标 BUG。

## 测试与验证边界

直接相关的现有测试范围如下：

- test_system_vm_call_module_export_executes_nested_native_export（tests/module/test_module_system.c:2932，runner :8118）先用 C API 调用 zr.math/sqrt，再执行脚本 system.vm.callModuleExport("zr.math", "sqrt", [4.0])（:2945）。断言 callback 返回非空且结果类型与先前直接调用相同（:2963），没有断言数值等于 2；脚本前已预热模块，所以不覆盖 lazy import、导入期 GC 或局部异常边界。
- test_system_leaf_modules_expose_new_api_and_owned_types（:3128，runner :8130）检查 loadedModules、state、callModuleExport 等导出名和 hints；它不运行 loadedModules() 或 state() callback。
- test_system_native_types_register_complete_struct_fields（:3357，runner :8133）核验 SystemVmState 和 SystemLoadedModuleInfo 原型字段布局，不覆盖 runtime snapshot 值或对象构造软失败。
- test_system_root_aggregates_leaf_modules_and_reuses_cached_instances（:3034，runner :8124）检查 root 聚合注册及缓存身份，不是 VM callback 行为测试。共享测试函数在其他叶台账的 canonical row 中登记，本批没有再建立重复 test-unit 行。
- official-provider suite 验证通用 provider/registry 行为，不给 VM callback、对象字段投影或 lazy import 生命周期增加测试信用。

仓内项目入口还包括 native_math_export_probe/src/main.zr:5、native_numeric_pipeline/src/callbacks.zr:18 与 probe_callbacks.zr:22 的按名称调用；callbacks.zr:19-20、probe_callbacks.zr:24,26 分别调用 loadedModules/state，callback :32 把 count 放入记录，probe :28 把 count 纳入返回 checksum。runner :503、:506-508 登记这些项目；主流程固定checksum214不消费模块计数，probe只要求 probe:vmState 标记，不固定返回checksum。本批未运行 project suite，不获得执行信用。

reference fixture tests/fixtures/reference/core_semantics/imports/native_root_member_chain_pass.zr:2 实际调用 state()，:4 比较 loadedModuleCount>0。test_reference_import_manifest_and_native_member_chain_fixture 在 test_module_system.c:7657 定义，:7682 读该文件，:7686 编译，:7688 执行，:7689 断言1，runner :8271；r1实际该项 PASS，因此提供单 state 正常计数正数投影的执行证据，不证明全部字段数值、GC root、同步或失败状态。Parser test_type_inference.c:7830/8030 和 LSP test_lsp_interface.c:1143/1144 则是编译/类型推断或语言服务字符串，不等于调用 VM callback。本批没有运行这些 Parser/LSP 专项。

此前 final-r1 使用实际 checkout 的 msvc-products-debug 配置运行验证，覆盖补充共享 global 并发 TODO 之前的同 token 源码。严格注释 token-equivalence proof SHA-256 为 a40489f90174ca0269ab4a559f66ac768d47cfe60c0797e8563e4a426e416b33。Debug/static build 命令构建 zr_vm_module_system_test 与 zr_vm_official_provider_convergence_test，exit 0，耗时 220.875 秒；4 个本批/Vec4 leaf object 被重建。构建存在 Parser compiler_function.c:665-666 两条 C4701 可能未初始化警告，另有 CMake test 配置 warning，因此不描述为无 warning。

zr_vm_module_system_test.exe exit 8；78 项中 70 PASS、8 FAIL，逐项状态和断言消息与最新 GC r3 比较完全一致。失败仍为 test_module_init_summary_cache_growth_keeps_current_source_summary_stable（:2440）、test_binary_roundtrip_preserves_module_init_callable_metadata（:2702）、test_builtin_wrapper_box_helpers_preserve_runtime_semantics（:4797）、test_typeof_local_callable_reflection_preserves_callable_shape_in_source_and_binary（:5912）、test_typeof_binary_module_reflection_omits_legacy_test_metadata（:6255）、test_typeof_binary_reflection_restores_advanced_oop_metadata（:7082）、test_reference_source_and_binary_import_forms_share_logical_identity（:7815）、test_reference_binary_module_metadata_roundtrip_fixture（:7931）。本批相关的 nested call、API/type exposure、native type field layout 和 GC controls（runner :8118、:8130、:8133、:8136）均 PASS。provider suite 为 9/9 PASS；该结果只表明通用 provider 测试通过。

与较早 r3 baseline 相比，build/test 输入有三项外部改动：zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c、zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c、zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_callable_member.c；所以 before/after 不是隔离比较，不能把结果差异归因给本批注释或这些外部改动。Root 记录 2950 个当前输入 pins 在 build 与两套测试前后稳定，本批冻结的 8 个源未变化。没有新增测试、源副本或临时 helper；报告显示临时文件前后均为空，净释放 0 字节，复用的构建目录保留。

完整过程记录位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/system-vm-vector4-final-r1-results.json，proof 位于同目录 system-vm-vector4-final-r1-comment-equivalence.json。本批没有执行 OOM/fault-injection、GC stress、lazy-import 异常或快照全部字段/并发/软失败专项测试；逐单元台账中的 TODO 保留这些未核边界。

最终并发约束注释后的 VM 四源严格证明为 system-vm-final-r2-comment-equivalence.json，SHA-256 d1dd3c80b14f44fdf26434e8025bf3112f5247b28cf19119264daad6b0a3f211；四源 token 数为 47/19/1735/487，kind/spelling/StartOfLine 和 LeadingSpace 全部相同，旧行到最终行映射另存。MSVC r2 曾因配置的 cl.exe 不存在而 CreateProcess 失败；工具后来重新可见，r4 又在其他会话的 Core/Parser 编译处遇到 C1902 PDB manager mismatch。两次均未运行测试，不记为源码验证通过；原 MSVC 树删除的两个 VM object 仍未重建。没有修改安装或终止外部 PDB 服务。

最终源码的实际验证为 system-vm-clang-final-r3e-results.json。沿用同一 checkout 和 E:/cargo-targets 下的 Clang 构建树，以 clang-cl 19.1.5、lld-link、原生 ml64、Debug/static 构建两个现有 target；cache guard 在构建前逐项确认编译器、汇编器、链接器及 static/shared/module 开关。configure exit 0/173.156 秒，build exit 0/159.579 秒，105 个 Ninja step 中包含 vm.c 与 vm_registry.c 的实际编译，新 object 的路径、哈希和时间另记。此前 llvm-ml 不支持 libffi 的 NEAR 语法、以及 CMake 清 cache 后误置默认 profile 的失败/取消记录保留；误置 profile 未授予测试信用。

r3e 模块测试 exit 8/19.750 秒，78 项为 70 PASS/8 FAIL，全部 78 条 case 行与上述 MSVC r1 精确一致；provider exit 0/4.188 秒，9 条 PASS 行也与 r1 精确一致。嵌套转发、root 缓存身份、导出及字段布局、reference member-chain 的 State/count 正常路径均 PASS；八项失败仍保留，不称 suite 全通过。构建记录含 291 条诊断行（包括重复 unused /experimental:c11atomics 和各模块 initializer 警告）；本批 vm_registry.c 有三条 genericParameters 与一条 onMaterialize 缺省 initializer 警告，token 证明显示相应 initializer 未改变，不声称无 warning。

r3e 期间 2950 个保守编译/配置输入及四个冻结源码哈希前后稳定，不表示这些文件全部实际被编译。State/count 实际引用样例另在 suite 前后核对单文件 SHA-256 25054105f7a31fb8f9e5dbf31fc1039ff833a9f0b9a6ca5ea54d7811fc64ae58；没有据此声称全部运行时样例、生成输入或外部 SDK/toolchain 均已 pin。相对 r1，除本批两份注释源码外，module_prototype.c 与 compile_expression_types.c 也有其他会话改动，且 MSVC/Clang 工具链不同，不能作隔离的 before/after 因果归属。完整对象、逐 case 比较和边界见 system-vm-clang-final-r3e-comparison.json；所有本次 native 子进程及工具会话已等待退出。

本批未增加行为测试或源码副本；没有执行 Linux、分配失败注入、并发、GC stress、project suite 或 lazy-import 异常专项。Clang 树保留供后续模块复用，临时验证 runner 的归属与后续清理登记在 artifacts.json；失败证据及未重建的 MSVC object 状态保留，不能把此前 r1 的净释放 0 字节当作当前整个任务的清理结论。
