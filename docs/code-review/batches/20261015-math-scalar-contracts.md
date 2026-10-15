---
related_code:
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_core/src/zr_vm_core/function/function.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/scalar.h
  - zr_vm_lib_math/include/zr_vm_lib_math/scalar_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar_registry.c
tests:
  - tests/module/test_module_system.c
  - tests/rust_binding/test_rust_binding_api.c
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr
  - tests/cmake/run_projects_suite.cmake
doc_type: code-review-batch
---

# Scalar callback 契约审查

## 范围与定义

四个 Scalar C/H 文件共94个唯一单元：4个文件说明、2个 include guard、2个参数化回调宏、25个 callback 声明和25个实现、2个 getter 声明和2个实现、2张静态表、25个函数 descriptor、5个 hint。实现包括21个宏展开与4个显式 callback；不重复创建普通局部块单元。相关测试和人工样例对照既有测试台账与当前定义，只计入其真实调用及断言边界，不给未执行目标信用。

## 调用与生命周期

全部27个函数符号和2个宏在首方仓库中作无扩展名限制的候选搜索，C/H 中去除注释和字符串后保留原行核查。查到27个声明、6个显式定义（4 callbacks +2 getters）、21个宏生成定义、25个 descriptor 函数指针、2个聚合宏 getter 参数；未发现直接 C 调用单个 callback。两个生成宏的21个展开点全部记录，不因没有直接调用就断言死代码；动态模块/跨语言入口另按协议名核查。完整审计 math-scalar-full-caller-evidence-audit-r1.json，SHA256 5c998ae4d6424e0e4969506c5b44762acb0cb80e478033b8398fb27835b6545c。

module.c:112 将 GetFunctions 作为聚合宏参数，:92 调 getter、:93 追加；module.c:137 将 GetHints 作为提示宏参数，:98 调 getter、:99 追加。native_binding_registry_plugin.c:971-972 遍历聚合后的函数表；dispatch_lanes.h:199 从 descriptor 取 callback，:277 在 native GC 域内间接调用。project.c:363 仅是整个 Math 模块的注册入口，未列作各 callback 的直接调用者。

scalar.h 的直接 include 者是 scalar.c:5 与 scalar_registry.h:8；scalar_registry.h 的直接 include 者是 scalar_registry.c:5 与 module.c:12。两个 getter 无 VM 分配，返回函数局部 static const 表，并可选写可写 count 槽；表、字面量和函数地址借用 provider image，使用期间须保持映像驻留，调用者不释放或改写。并发只读静态表不赋予共享 count 槽并发写权限。hints 经 metadata.c:927/428 导出，也由 LSP provider.c:776 按 symbolName 匹配；它们不执行 callback 或校验运行时返回标签。

所有 Scalar callback 只借用本次 context、state、参数和 result；ReadFloat 接受各整数及浮点 VM 标签，转为 TZrFloat64，较大整数可能丢精度。arity 由 descriptor 在派发前检查；缺参由 RaiseArityError、错标签由 RaiseTypeError 非正常返回，不能记成普通 false。合法数值入口的算术 callback 完成时写 float/bool 并返回 true，不额外筛除非有限输入。宏中的 helper-false 分支保留防御性传播，不把它当作已证的缺参/错类型返回路径。

## BUG 与 TODO 证据

BUG: invokeCallback 的 descriptor/hint 声明 float，而 callback 把结果槽直接交给 CallValue；dispatch.c:1931-1942 在成功状态原样复制被调 callable 的 VM 值，未按 returnTypeName 校验。合法 callable 成功返回 bool 或字符串即可交付另一种 VM 标签；四个对应声明、实现、descriptor、hint 单元均记录同一缺陷，不作四种故障计数。本批无专门返回类型执行复现。

TODO: ReadFunction（dispatch.c:1255-1272）还接受 NATIVE_POINTER，core function.c:4273-4275 将其 cast 为 FZrNativeFunction 并派发；C API SetNativePointer（native_binding.h:630）不做 ABI 验证。须核对由哪个宿主入口保证非空、函数 ABI 相容及代码映像有效期；这不是已证的非法指针调用 BUG。CallValue 普通失败状态可返回 false，VM 异常路径与其分别记录，嵌套调用不支持 yield。

TODO: clamp 保留先下界后上界的比较顺序；low>high 的公开政策须沿 callback 和 API 文档确认。lerp 使用 start+(end-start)*factor，可外推；有限 -1e308/1e308、factor .5 的中间差溢出，但稳定化算法政策尚待明确，记录 TODO。almostEqual 是绝对误差 helper，省略 epsilon 使用 ZR_MATH_EPSILON；跨数量级的相对误差政策仍 TODO。所有疑点均有源码和具体入口，未修改数值策略。

## 测试证据边界

本批实际共享 module suite 中 test_system_vm_call_module_export_executes_nested_native_export（test_module_system.c:2932；注册:8118）为 PASS。其 :2954 直接调 sqrt，:2955 仅断言非 null；:2945 的脚本经 :2961 执行，:2962 断言非 null、:2963 比较标签，没有 sqrt 数值断言。该信用不覆盖其他24个 callback、极值或 ABI/返回标签故障。

Rust binding 的既有 C 测试 :932 经 Project_CallModuleExport 调 sqrt(9)，:941 检查 float kind、:946 断言3.0；本批 BUILD_RUST_BINDING=OFF，未运行该目标，只记录现有静态数值断言。native_math_export_probe/main.zr:5 按字符串调用 sqrt(4)，:6 还取 Matrix4x4 scale，:8 合并；run_projects_suite.cmake:503 为聚合输出用例，本批未跑项目 runner。

callbacks.zr:17 与 probe_callbacks.zr:20 都走 math.invokeCallback；前者后续按名调用 summarizeCallback 并在 :31 合并 checksum，runner:507 的214断言不能隔离 callback 返回类型；后者是阶段打印探针。本批均未执行。runner:75/82/85 会改写输入、bytecode和固定输出目录，本批只读核查。其他算术 callback 当前未发现对应单项数值断言，台账明确保留这一边界。

## 冻结与实际验证

最终四个源码 SHA256：

- zr_vm_lib_math/include/zr_vm_lib_math/scalar.h：65b31fd67198800521a19253ce5b755b28aa7c39a68ecc3010ddbae42f3ec2ae。
- zr_vm_lib_math/include/zr_vm_lib_math/scalar_registry.h：0c65f55b06ec812b0cee64f23c41b789678e793983a48c24724eef29cecf618f。
- zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c：261022a5c1d503aae9a5e1a916c5268822d3103aa3574f0aa1f8b2ae3222583a。
- zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar_registry.c：75ee681f2971a6324ad3e4258813f43c2d13cbe5a35c8da305d4a87f63dd20cb。

严格 math-scalar-final-r2-comment-equivalence.json（SHA256 3438913ba73cbf34d2f45a75f0db5feb16217547dba2579759adf931314d50dc）比较改前 main c384d0f2 与最终四源的1639个非注释 token及lexer flags，完全相等；原C程序token物理行不变，8个lexer children全等待退出。只修改中文Doxygen/意图注释，API签名与程序语句不变。

实际 checkout 复用 clang-cl-products-debug：scalar-stream-vector-final-native-r2 构建0/10.375秒，module70/78 same8FAIL、provider9/9、Fs7/8 sameC1035；3216个输入及24个冻结源全程稳定，全部case行与历史稳定基线相同。报告 SHA256 4f3dc77668e03b98b3e5d44ef286459801db0133a6e8bd89fb608edca1dc2efb，comparison fd41f0d6dd2572ef25f3e6835ad24ea07327ff66d370a413f7b1d5c2d40de6af。Scalar四源与该轮实际冻结值逐一吻合。

native-r1 因 FileStream EOF GC引用运行中更正而 guard失败，原记录保留，不计作稳定验证。更早改前全构建因外部Parser宏缺失被阻塞，外部Parser/测试输入后来变化，因此不宣称隔离的全仓行为等价。没有Scalar专项NaN/溢出、返回标签、NativePointer ABI、OOM、GC/并发或WSL/Linux执行信用；现有module/provider汇总不能替代这些验证。

## 台账与产物

94行均有真实定义、已搜索的调用方/间接入口、契约与测试边界，56已注释/26无需独立注释/8TODO/4BUG。descriptor/hint 单项在表/公开函数契约充分覆盖时不另作逐行复述。Root把旧Math中四源的31个条目逐一映射到此94单元后移除旧行（旧表61变30），更新inventory四行并追加模块Wiki；其他单元/混合改动原始字节保留。此批交付94个唯一单元，累计1372。

未创建源码副本或新增测试；构建、共享缓存与alternate index复用。自有构建/测试已退出；本轮释放0字节，Fs28个生成文件91字节因此前自动审批 blocked by policy 拒绝精确清理而保留，未绕过或重复尝试，清理尚未关闭。

## 补充静态入口

首方协议候选中还包括 tests/language_server/test_lsp_interface.c:1125 的 g_callbacks_fixture，:1141 嵌入 math.invokeCallback。test_lsp_function_symbol_range_trimmed_to_body（:3107，:3136 UpdateDocument）及 test_lsp_function_prepare_rename_uses_name_range（:3188，:3215 UpdateDocument）使用该字符串做文档解析和位置/重命名范围测试；它们不执行 callback，本批未运行 LSP 目标。该候选与两个 pipeline 动态源码入口分别记录，不给 callback 数值、返回标签或 ABI 执行信用。

## 完整协议候选与摘要投影

385个广义协议候选经完整分类后，除前述动态入口外还发现三类 sqrt 静态消费。tests/parser/test_type_inference.c:3410-3437 仅导入模块并检查 sqrt 成员 metadata/signature token/hash（登记:8835）；tests/language_server/test_lsp_cast_operand_facts.c:213-218 的源码含 sqrt(4.0) 与 cast，:227 登记，tests/cmake/zr_vm_cast_operand_tests.cmake:7-16 声明独立目标，检查外部身份/导航；tests/language_server/test_lsp_virtual_declaration_projection_cases.h:9/24 的共享夹具含 sqrt(math.PI)，test_lsp_semantic_query_parity.c:1564 include、:1605 以caseIndex调用，只核对虚拟声明投影。这些目标不在本批 module/provider/Fs 运行集，均只作静态且未执行的证据。

module.c:45/52 的精简 g_math_type_hints_json 同样把 invokeCallback 写为 float，经 native_binding.c:733 GetModuleJson 和 :738 WriteSidecar 借出/写出。它重复同一个返回标签 BUG，是额外 metadata surface，不是新 runtime caller，也不是已证编译器完整输入。Root只在原注释行补充该 BUG、同步原有 module 台账这一条及两个API页；不改 JSON 字面量或函数签名，不新增该既有单元计数。

这一模块摘要依赖源的最终SHA256为 1c54fa78de44099e994137458c36ced0c4aacd840a23a59dcc417fd0faf6fb6e。严格 math-scalar-module-json-final-r1-comment-equivalence.json（SHA256 72726687ed3f692aa562f569706990c7b315822ff80bfd8022843fdb069df2be）确认848个token/flags及原C行不变；2个lexer已退出。native-r2实际使用旧5a2efdca版本，该版与新源程序token/flags/原行相等的桥接报告为 math-scalar-module-json-native-r2-token-bridge.json；不宣称新注释字节已在r2编译。

此处的具名入口区分实际VM调用与C源码内供工具解析的脚本文本；后者包括下列abs样本，不视为callback执行。没有发现额外直接C调用或额外单项数值断言。callbacks/probe runner登记:506-508仍只作静态入口。完整分类不排除外部宿主或未来动态名称调用。

## abs 与其余静态文本候选

静态 abs 消费亦逐类核对：tests/parser/test_semantic_query_symbols.c:1791-1847 将 :1794 的 math.abs(-3.0) 文本交给解析/外部符号查询，不求值；tests/parser/test_compiler_features.c:4671-4672 只验证 plugin handle 的 callable 不能逃逸；tests/parser/test_project_import_canonicalization.c:2933-2991/3042-3067 核对 module-effect target hash及未加载异常，:2990 的实际执行只返回导入失败 fallback 77，不执行abs。tests/language_server/test_lsp_advanced_editor_features.c:908/985/1179/1226/1276 是注释生成或missing-import quick-fix源码，:1320-1322/:1381 则是注释/字符串中的反例文本；不作数值执行入口。扩展 src/structure/builtinModules.ts:124-135 与 test/builtinModules.test.js:22-27 只是名称/代表符号快照。以上parser/LSP/扩展目标本批未运行，也没有abs数值断言；其余zr.math导入、注册、类型、非Scalar数学样本保留对应分类，不外推某个callback被调用。

LSP stdio 候选亦核对：tests/language_server/stdio_smoke.js:2851/2990/3025 的 math.abs 是 textDocument/didOpen 的源码文本，分别用于 unused-alias import、missing-import quick-fix 与范围内缺失导入诊断；:3025 的外层 system.print 也没有在此执行。tests/language_server/CMakeLists.txt:217-224 仅在 Node 与 stdio target 同时存在时登记 language_server_stdio_smoke；本批 LSP 关闭、该目标未运行，三处均不授予 Scalar callback 执行或数值断言信用。全首方具名 math.* 重扫中 scripts/benchmark/benchmark_statistics.py、tests/benchmarks/test_benchmark_statistics.py 使用 Python math，tests/benchmarks/common/lua/benchmark_runner.lua 使用 Lua math，均非 zr.math callback；acceptance Markdown 只记录历史静态编译/符号预期，不新增本批执行证据。
