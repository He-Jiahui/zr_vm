---
related_code:
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_parser/include/zr_vm_parser/iteration_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_flow.c
implementation_files:
  - zr_vm_lib_iteration/CMakeLists.txt
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_parser/include/zr_vm_parser/iteration_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c
plan_sources:
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m1-enumerator-protocol-implementation-plan.md
  - docs/code-review/batches/20261013-iteration-contracts.md
tests:
  - tests/iterator/test_enumerator_protocol.c
  - tests/iterator/test_iterator_runtime.c
  - tests/iterator/test_iterator_gc_drop.c
  - tests/iterator/test_yield_syntax.c
  - tests/iterator/test_iterator_semantic_ir.c
  - tests/iterator/test_yield_semantic.c
  - tests/library/test_official_provider_convergence.c
  - tests/container/test_container_type_inference.c
  - tests/parser/test_numeric_foreach_cardinality_dataflow.c
  - tests/parser/test_numeric_loop_assignment_dataflow.c
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
doc_type: module
---

# `zr.iteration`：协议身份、注册与借用边界

`zr.iteration` 是 `Iterable<T>`、`Enumerator<T>`、`Iterator<T>` 与 `AsyncIterator<T>` 的规范公开 TypeId 所有者。模块只发布协议表；具体集合 provider 使用这些身份发布能力和实际回调，不重新定义协议所有者类型。描述符声明 Runtime phase，公共契约哈希为 `zr.iteration:v1:canonical-iterator-protocols`，官方 provider 收敛测试核对这些身份与反射投影。

## 入口与生命周期

| 入口 | 使用时机与结果 | 前提、所有权与失败状态 |
| --- | --- | --- |
| `ZrVmLibIteration_GetModuleDescriptor` | 注册器、反射与编译器需要规范协议身份时取得描述符；本身不注册模块 | 无参数，返回模块静态 const 对象的借用地址，不分配、不调用 provider 回调；调用方不得修改或释放，插件映像卸载后地址失效 |
| `ZrVmLibIteration_Register` | global 初始化后、消费者查询模块前附着并发布描述符；容器注册路径先调用它 | global 非空且 mainThreadState 已建立；Attach 和描述符注册都成功才返回 true。Attach 成功后的注册失败不撤回 registry；同名项由兼容性检查决定，初始化调用须串行化 |
| `ZrVm_GetNativeModule_v1` | 共享变体的插件加载器按固定符号名取得 ABI v1 描述符 | 只在 ZR_LIBRARY_TYPE_SHARED 下导出；返回与公开 getter 相同的借用值，不执行注册。加载器与注册表使用期间必须保持 provider 映像加载 |

实现入口见 `zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c:17`、`:21`、`:43`；容器调用见 `zr_vm_lib_container/src/zr_vm_lib_container/module.c:7922`。底层 Attach 对已附着的 registry 返回成功，RegisterModule 的记录路径也会调用 Attach，因此原有重复附着疑点已由当前调用路径解释；本轮保留调用顺序。未建立 mainThreadState 会被 Attach 拒绝，接口未承诺并发注册安全。

## 类型表的不变量

全部表项及其字符串是静态存储，模块描述符借用这些表，注册表再借用模块描述符；其嵌套指针不会随 getter 转移所有权。

| 类型/成员 | 规范协议契约 | 当前实现边界 |
| --- | --- | --- |
| `Iterable<T>.getEnumerator` | `ITERABLE_INIT` 角色，结果为 `zr.iteration.Enumerator<T>` | callback 为空，只描述协议；容器提供实际初始化回调 |
| `Enumerator<T>.moveNext/current` | `ITERATOR_MOVE_NEXT` 与 `ITERATOR_CURRENT_FIELD`；moveNext 返回 bool | moveNext 的规范项 callback 为空；current 的 ROLE_INIT 元数据将 isReadonly 设为 false |
| `Iterator<T>` | 与 Enumerator 共用 `ITERATOR` 能力位，implements `zr.iteration.Enumerator<T>` | 编译器的不透明载体，禁止普通 value/boxed construction；模块不实现其 frame 或 producer |
| `AsyncIterator<T>` | 独立 `ASYNC_ITERATOR` 能力位；moveNext/close 返回 Task<bool>/Task<void> | 仅发布静态角色及签名，没有本模块异步推进/关闭回调；同步绑定器不消费它 |

native metadata 构建器会跳过没有 callback 的方法（`zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1301`）。容器的实际 getIterator 和 moveNext 回调绑定相同成员角色，形成规范身份与执行实现的协作边界。`Array<T>`、`Map<K,V>`、`Set<T>` 与 `LinkedList<T>` 保留自己的能力位和回调，implements 行指向规范 Iterable 身份。

TODO: `current` 的 isReadonly=false 会投影为 writable（`native_binding_metadata.c:1240`），但不能据此证明脚本赋值被接受。需用真实 Enumerator 在 moveNext 成功后执行 `current = value`，沿属性赋值器与 prototype 元数据确认结果；现有协议测试只查询方法角色和返回类型，既未查询 current 字段，也未覆盖该赋值；源码 References 中的测试函数仅为后续核查的邻接入口。源注释和模块台账保留同一疑点，不在本轮改变字段属性。

## 编译器消费边界

普通 `for` 通过 `ZrParser_EnumeratorBinding_ResolveElementType` 解析元素类型。桥接器仅消费已解析泛型参数的 `ITERATOR` 或 `ITERABLE` 投影（`zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c:5`）；没有 Array 基类型、ARRAY_LIKE、具体类型名、成员拼写或源码文本回退。Array 类型推导提供 ITERABLE 事实，保留静态 ITER_INIT/ITER_MOVE_NEXT/ITER_CURRENT lowering。

循环绑定的清理由 scope exit 持有。协议模块的 M1 只建立描述符身份和同步绑定；后来 parser/core 的 yield、SemIR 与 iterator frame 演进是相邻实现，不能从本模块的 AsyncIterator 表项推断异步循环或 boxing adapter 已实现。

## 当前验证与已知测试问题

2026-10-13 在实际 checkout 的 MSVC Debug 静态构建中，模块和六个现有 Iterator 目标修改前构建通过；33 个 case 中 30 个通过，yield_semantic 的三个 case 失败。最终注释版本复用同一构建目录、配置和测试，增量构建退出0；33 个结果及失败标识逐项与基线相同，四个模块源文件和六个测试的哈希在运行前后保持。三个 C 文件的 Clang raw lexer 非注释 token、行首标志一致，CMake 非注释命令一致；最终证据分别见 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/iteration-final-after-results.json` 与 `iteration-final-comment-equivalence.json`。对应测试源码只读，前三个运行时/协议测试文件有既有差异，另外三个文件 clean，但都耦合活跃 parser/core 会话；本批不覆盖其源码或混合旧台账。

BUG: `tests/iterator/test_yield_semantic.c:154`、`:237`、`:302` 的断言把顶层 `compiler->preSemanticIr` 当作被声明函数的持久 IR。函数声明现以临时 child IR 生成 yield/completion，再释放 child 并恢复 caller；现行 callable-isolation 测试和 `docs/parser-and-semantics/pre-semantic-ir-flow.md:442` 明确要求不向 entry graph 泄漏。实际失败与这个旧观测契约一致，不是本模块的迭代回调证据。首个 case 在154失败后，其后续断言未执行；stderr 的负例编译错误对应通过 case。根因调用链、历史提交和原始日志保存在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/yield-semantic-before-diagnosis.md`；本轮只记录，不修改测试或生产行为。

逐单元定义、全部已检索调用方、间接插件入口及测试证据见 `docs/code-review/coverage/zr_vm_lib_iteration.tsv` 和本次批次文档。静态构建未授予 shared plugin 加载或异步执行的运行信用；WSL 启动 E_ACCESSDENIED，Windows 结果也不替代 Linux 验证。
