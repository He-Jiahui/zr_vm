---
related_code:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/include/zr_vm_lib_math/module.h
plan_sources:
  - docs/code-review/comment-standard.md
  - E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/math-module-source-leases.json
---

# Math 模块聚合与注册入口

仅审查 module.c/module.h：7 个函数、静态存储、容量、两个追加宏、非显然聚合顺序及共享库条件入口均单独入账。台账为 ../coverage/zr_vm_lib_math_module.tsv；相关测试只读取证据，不修改或重复授予测试代码审查信用。

## E1：定义、调用与入口

Register 的当前直接调用包括 CLI project 注册、LSP VM 创建、测试宿主与 parser 构建仍纳入的 AOT 测试。实际 checkout 的直接调用共 41 处；检索排除第三方、生成产物及 .claude 遗留源码副本。全部可检索 C/Rust/TypeScript 名称引用已检索，未发现额外跨语言包装或函数指针赋值；不能据此断言不存在仓外宿主调用，也不以 AOT 目录名判停用。

zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6882；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7197；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7233；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7271；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7315；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7355；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7403；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7440；zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:7502；zr_vm_cli/src/zr_vm_cli/project/project.c:363；tests/compileTime/test_comptime_definition_environment.c:754；zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c:117；tests/language_server/test_union_pattern_diagnostics.c:860；tests/container/container_test_common.c:61；tests/ffi/test_native_extern_contract.c:196；tests/ffi/test_ffi_module.c:124；tests/ffi/debug_ffi_close_probe.c:45；tests/language_server/test_semantic_analyzer.c:4637；tests/language_server/test_ownership_diagnostics.c:895；tests/module/test_module_system.c:985；tests/module/test_module_system.c:2951；tests/module/test_module_system.c:2997；tests/parser/test_compiler_regressions.c:2546；tests/parser/test_compiler_regressions.c:2595；tests/parser/test_compiler_regressions.c:2645；tests/parser/test_compiler_regressions.c:2695；tests/parser/test_compiler_features.c:124；tests/parser/test_project_import_canonicalization.c:2945；tests/parser/test_project_import_canonicalization.c:2981；tests/parser/test_project_import_canonicalization.c:3051；tests/parser/test_semantic_query_relations.c:1315；tests/parser/test_semantic_query_relations.c:1423；tests/parser/test_semantic_query_relations.c:1496；tests/parser/test_semantic_query_symbols.c:1691；tests/parser/test_semantic_query_symbols.c:1804；tests/parser/test_semantic_query_symbols.c:2202；tests/parser/test_syntax_hygiene_alias_scope.c:284；tests/parser/test_syntax_hygiene_alias_scope.c:294；tests/parser/test_syntax_hygiene_alias_scope.c:305；tests/parser/test_syntax_hygiene_import_aliases.c:33；tests/parser/test_type_inference.c:363。

GetModuleDescriptor 在当前仓内由本文件共享入口调用；公开 ABI 也允许仓外宿主查询。共享 ZrVm_GetNativeModule_v1 经 native_binding_registry_plugin.c:370 字符串查符号和 :383 函数指针调用进入，加载器随后在 :396/:410 校验 ABI/请求模块名；静态构建不覆盖该分支执行。

## E2：容量、不变量与提示

逐个 getter 核对为 25 个模块函数、8 个类型、13 条结构化提示：Scalar 函数表 25 项，其余八个类型的 GetFunctions 都写 count=0 并返回 NULL；Scalar hints 5 项，加八个类型各一项。容量分别为 48/8/32，目前未越界，类型表恰满。

offset 是 initialize 的栈游标，函数/类型/提示三段之间分别清零；宏依赖同作用域 count/offset，空 getter 也必须写 count。浅拷贝不取得 descriptor 中字符串、字段表、方法表或 callback 的所有权，原 registry 静态存储须继续有效。

**TODO:** 三个 append 没有容量检查与失败返回；新增第九类型或扩充函数/提示时，沿所有 getter 重新计数并核对上界。当前数量不构成已发生越界 BUG，核查入口是容量枚举、append 与三组聚合调用。

六个常量元素分别发布 PI、TAU、E、EPSILON、INF、NAN，均为 float descriptor，值来自 conf.h 或宿主 math.h。精简 JSON 只列 abs/invokeCallback 两个函数和八个类型；已查消费者是 NativeHints_GetModuleJson/WriteSidecar 和 native_binding_metadata 反射字段，不能只凭旧注释声称编译器将其用作完整类型源。

## E3：借用、失败与线程边界

描述符不依赖某个 VM，静态初始化不分配 VM 对象；描述符和嵌套表、字符串、callback 均依赖模块映像。静态链接通常覆盖进程，共享库指针不能越过卸载。GetModuleDescriptor 及共享发现入口不注册到 VM，也不拥有加载器句柄。

Register 初始化后委托 NativeRegistry_RegisterModule；下层 register_module_record 先拒绝 NULL，再 Attach、验证和添加/重注册。false 不表示 VM 完全没有变化，Attach 可能已完成；静态描述符不会因某个 VM 注册失败而回退。registry 借用 descriptor，VM 销毁不释放本模块静态数组；本层不额外承诺下层分配失败原子性。

**TODO:** g_math_initialized 非原子；表填好后才置 true 仍不提供跨线程同步。当前 CLI 启动注册是串行路径，未证明宿主允许跨线程同时首次调用，因此保留调用约束疑点，不宣称已复现并发 BUG。宿主须串行首次初始化、在后续跨线程读取前建立同步，也须协调同一 VM registry 写入。

## E4：测试与验证边界

现有 module 测试在 test_module_system.c:2951/:2954 注册 Math 并经 sqrt 导出调用比较结果非 NULL 与标签，不额外授予 sqrt 精度断言；:2997/:3021 的 Vector3 构造断言字段；:7431 owner refcount 用例通过 zr.math 导入检验模块引用计数。provider suite 的 zr.math 只是冻结清单项，9 项通过不能据此宣称断言了 Math 三组计数或动态插件分支。

修改前 Math/System 静态库 build0，见 support-common-libraries-baseline-r1-results.json。完整测试目标 pre-edit build1，foreign parser 的 compiler_quickening_null_state.c:147 和 compiler_quickening.c:9814 使用未定义 ZR_INSTRUCTION_USE_RET_OPERAND_EXTRA，三个 suite 未启动。历史 Complex 的 module70/78同八项失败/provider9/9，不能记作本批新执行。

两源严格证明 math-module-final-r3-comment-equivalence.json 已通过：894 个非注释 token 的种类、拼写、StartOfLine/LeadingSpace 相同，lexer 输出全部解析，module.c 每个 token 行号不变；4 个私有函数与 2 个可调用宏在原代码之后用 @fn/@def 提供完整 Markdown Doxygen，公开参数标明方向；没有源码副本。r1 宏续行 UnClean 标志解析假差异保留诊断，v2 按实际 EOL 处理多行标志，不规范化源码来规避检查。

最终实际 checkout 验证 support-common-final-native-r1-results.json（SHA256 d24a551e3944d6326b7c143b81826369f6f8cb4dfa053a30c7409afaee27214d）为 build0/19.110 秒；module70/78、exit8，与 Complex 最近基线全部78条case行一致；provider9/9、exit0，全部9条一致；Fs7/8、exit1，与其旧基线8条一致。3216个输入及13个冻结源码在构建/测试期间稳定；完整对比见 support-common-final-native-r1-comparison.json（SHA256 89f2317ebcab940a7e410366b664cac0ff6e16859337450f81224596ae9e67f1）。当前 foreign Parser 已变化并解除此前编译阻塞，不能把本次成功隔离归因于注释。精确暂存复核和提交回执由 Root 串行登记；无并发、越界或动态加载执行信用。没有新增实现镜像测试，共享构建保留；本轮测试又生成4个13 B夹具，现共16个52 B保留，本批释放0 B，既有自动清理拒绝未关闭。
