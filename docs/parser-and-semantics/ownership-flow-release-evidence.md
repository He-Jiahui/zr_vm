---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_regions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_symbols.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_owner_sets.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_statements.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_observations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_source_metadata.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_ownership_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_ownership.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_diagnostic_projection.c
  - tests/parser/test_weak_owner_release_evidence_cases.h
  - tests/parser/test_semantic_using_release_evidence_cases.h
  - tests/parser/test_primary_expression_source_range_cases.h
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_dynamic_return_representation_cases.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics_region_cases.h
  - tests/language_server/test_ownership_diagnostics_owner_set_cases.h
  - tests/language_server/test_ownership_diagnostics_using_body_cases.h
  - tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h
  - tests/cmake/aot-pipeline-targets.cmake
  - tests/cmake/lsp-query-targets.cmake
  - tests/cmake/manifest-suite-registration.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_regions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_symbols.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_owner_sets.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_statements.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_observations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_source_metadata.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_ownership_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_ownership.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_diagnostic_projection.c
plan_sources:
  - "user: 2026-10-16 document shared ownership release evidence, lower regression boundaries, and unresolved Windows M1 failures"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
tests:
  - tests/parser/test_weak_owner_release_evidence_cases.h
  - tests/parser/test_semantic_using_release_evidence_cases.h
  - tests/parser/test_primary_expression_source_range_cases.h
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_dynamic_return_representation_cases.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics_region_cases.h
  - tests/language_server/test_ownership_diagnostics_owner_set_cases.h
  - tests/language_server/test_ownership_diagnostics_using_body_cases.h
  - tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: module-detail
---

# 所有权流与释放证据

## 目的与当前边界

编译器和语言服务器共用 parser 的所有权事实、CFG 分析与语义查询。
一个调用已经因 weak 值不能直接借用而被拒绝，并不意味着这个错误可以丢失此前的 owner 释放位置。
后续查询仍需要区分哪个 alias、哪个可能 owner、哪条释放路径造成了错误。
本模块文档说明这条共享链路，以及围绕它新增的 lower 回归。

2026-10-18 当前分类回归：普通 `ownership_mismatch`/`2008` 即使是同一 READ 节点上的
WEAK ERROR，也不表示该读取需要 wake。GDB r2 收据
`F66EBE24C1532B25E2586B17AD2A0D70FCC450663B0BAC4F49702990C40950BA`
直接命中旧宽泛分支：`requiresWake=0`，而 CFG 随后给该 ERROR 补入 owner region 和 release 节点。
打印的诊断仍是 `2008`，`relatedInformation.length=0`；没有实际观测到诊断 `4004`。
后者只是查询投影路径的静态风险，不能写成已经验证的结果。

随后 focused RED r2 实际执行原 83 项及两条新增分类测试：
`85 Tests 3 Failures 0 Ignored`，native 自然退出 3，失败恰为原 mismatch 综合控制和两个新增控制。
该运行六个子进程均已终态，但七个其他会话源码路径在跑中改变，故严格收据
`04CF41E51E231815F7269CF65737F3E856E7BC9EAAA24CE259B30764F94139F5`
保持 FAIL，不能赋予固定输入或平台验收信用。生产修复只为旧 fallback 增加
`error->requiresWake` 条件。focused GREEN r2 的实际 native 已得到
`85 Tests 0 Failures 0 Ignored`，自然退出 0；但三个其他会话 SSA 路径在跑中改变，
严格收据 `06EBDF099525A9F90DDC4FFFDAF6C6E604D7A7389235F2D2F8BFFCA741B816CC`
仍为 FAIL。GREEN r3 的源码前后相同，但 HEAD 改变，严格收据同样为 FAIL。
新登记 GREEN r4 才取得固定输入 PASS：完整 85 项、0 失败、0 忽略，native 0；
源码 4907/521603AA、HEAD 0307da9 及 support、配置、环境、登记、二进制前后均稳定，
六个子进程自然终止。收据为
`3C70657DFAE5191D6AC2D5DAF5C41598DBF042BE6583966A88D72130A8B791F5`。
这是该局部目标的闭包，不替代完整平台或 M1 验收，M1 仍 OPEN。

以下为历史阶段记录，不代表当前 checkout 的通过状态：

实际 lower r1/r2 各有一个达到行为断言的 RED 和一个 guard 控制前提失败。
lower r3 的原 64 项通过，四个 weak 用例前提失败，using2 达到缺少 READ/RELEASE 的行为 RED；历史不改。
r4 已自然结束：query71 为 66 PASS / 5 FAIL，原64与 using2 通过，weak4 仍是前提失败，
direct incoming-parameter 用例达到精确编译行为 RED；ownership25 为 22 PASS / 3 FAIL，剩余三例均是 weak 链路。
显式 wake 证据已实现；新参数声明 sidecar 暴露的 incoming value 缺口现已在参数 producer 中有界补齐，
r5 实际 query71 为 70 PASS / 1 FAIL：direct-degrade、using2 与三个 weak 控制通过，
主 owner-set 用例仍在精确释放证据行为断言处失败，已无 weak fixture 前提失败。
ownership25 仍为 22 PASS / 3 FAIL。guard 夹具现为 `const fn ping`，不能追溯修改旧结果。
r6 保持 query71 的 70 PASS / 1 主 weak 行为 FAIL 和 ownership25 的 22 PASS / 3 weak FAIL；
另一路 AOT17 全部通过，不能替代本模块仍失败的释放证据断言。
受控观察器自然 exit 0 不等于整体行为 GREEN。
2026-10-17 UTC 的 r8 在两处 PRIMARY 父范围合并后，默认 query72 / ownership25 / AOT17 全部 PASS；
这是 focused integration GREEN，不是完整 incoming ABI 或全局语义保证。
随后的完整 Windows r11 仍为 FAIL；更晚的 R11-support R4 保持 ownership25 全 PASS，
但当时 lambda 与 BOOL 支持仍有真实失败。R11-support R6 后续通过 query77、
原 LSP84 与 ownership25；R7 进一步通过 facts20、AOT18 与 guardrail6，
但其 OBJECT 新例夹具预期和 mixed-postfix 仍失败，严格收据保持 FAIL。
用户现已批准保留原 TokenSpan、按 AST 身份判归属、跨源诊断使用锚点及相关来源的方向；
新 mixed-postfix 已到达真实行为 RED，修复与相关来源诊断仍未通过验证；
当前不再以“产品决策未答”为阻塞。
Windows、Linux 与 M1 均未因这些局部证据获得验收信用。

## 当前事实与调用链

核心入口与分工如下：

| 阶段 | 当前实现与输出 | 关键边界 |
| --- | --- | --- |
| 调用兼容性 | `type_inference_call_diagnostics.c` 分类 weak wake、ownership mismatch 等拒绝，发布结构化编译错误和 ownership `ERROR` | 类型错误成立，不要求该调用存在已解析的 `CALL` |
| CFG 区域绑定 | `dataflow_ownership_regions.c`、`dataflow_ownership_symbols.c` 与 `dataflow_ownership.c` 建立 alias/owner 的 region 关系 | `degrade` 构造、声明和重新绑定要保持同一源码身份 |
| 控制流求解 | `dataflow_ownership_owner_sets.c` 与 ownership driver 维护分支合流后的可能 owner 集合及释放状态 | 条件重新绑定不能只保留一条路径的 owner |
| 读取分类 | `dataflow_ownership_moves.c` 判断读取是否移动、释放或需要 wake | 成功调用使用真实 resolved `CALL`；拒绝实参与旧错误 fallback 均需显式 `requiresWake`，并保持直接 AST 身份 |
| 观测发布 | `dataflow_ownership_observations.c` 追加 transition，或给现有 weak `ERROR` 补充 symbol、region 和 cause | 不能把一般 weak 类型不兼容自动解释成 owner-release 错误 |
| 查询物化 | `semantic_query.c` 与 `semantic_query_ownership_diagnostics.c` 将事实投影为结构化诊断 | 相关信息必须来自真实释放节点，不能从提示文字猜测 |
| LSP 发布 | `semantic_analyzer_query_diagnostics.c` 调用共享分析和查询，再经 diagnostic projection 发布 | 同码同位置的已有诊断可被带完整证据的查询结果替换和合并 |

[公开事实声明](../../zr_vm_parser/include/zr_vm_parser/semantic_facts.h) 中，
`SZrSemanticOwnershipFact` 当前包含 `node`、`range`、`kind`、`qualifier`、
`symbolId`、`lifetimeRegionId`、`ownerLifetimeRegionId`、`relatedNode`、
`isViolation`、显式 `requiresWake` 和展示用 `diagnosticMessage`。
`requiresWake` 仅由既有 weak-wake 诊断分类发布；其他所有权事实默认为 FALSE。
weak qualifier 只描述所有权类别，不足以说明一个错误是否属于必须 wake 的借用。

调用诊断生产的初始 `ERROR` 可以具有真实节点和范围，但 symbol/region 尚为无效值、没有释放 cause。
CFG 发布阶段需要把真实引用身份和路径证据补齐；query 层不能凭空补出这些信息。
`diagnosticMessage` 是展示文本，不是稳定的错误分类协议，也不是所有权决策的输入契约。

## 被拒绝的 weak-ref 调用

主 lower 用例使用实际源码：

```zr
resource class Resource {}
fn observe(resource: ref readonly Resource): int { return 0; }
fn run(first: Shared<Resource>, second: Shared<Resource>, choose: bool): int {
    var watcher = degrade(first);
    if (choose) {
        watcher = degrade(second);
    }
    drop(second);
    observe(ref watcher);
    return 0;
}
```

`watcher` 在合流后可能指向 first 或 second 的 weak alias。second 已释放，而直接 ref 借用仍需 wake，
因此调用被 `weak_value_requires_wake`、descriptor `4004` 拒绝。
拒绝之后缺少 resolved `CALL` 是这个输入的真实状态，不应通过伪造成功调用来绕过。

[新 lower 测试](../../tests/parser/test_weak_owner_release_evidence_cases.h) 先验证真实 Parse 和 compile 的产物：
精确错误 code/descriptor、实参源码位置对应的 resolved `READ`、与该 READ 节点关联的 weak `ERROR`，
以及第 9 行不存在 resolved `CALL`。只有这些前提满足，才显式调用
`ZrParser_SemanticFacts_ResolveControlFlowOwnership`。

这个显式步骤不是手工拼接事实。它对同一 AST、semantic context 和实际编译产生的 reference facts 运行共享 CFG。
编译器的正常脚本结束路径在已经失败时不会完成正常的语义查询发布，因此测试需要单独进入这一低层边界。
旧实现的 wake 形参分类只从 resolved `CALL` 找参数；主用例恰好缺少该事实，故能隔离拒绝路径上的证据缺口。
当前补充分类读取同一实参的显式 `requiresWake`，仍由原 CFG 查找真实释放 cause。

期望补齐后的结果同时满足以下关系：

- weak `ERROR` 的 symbol 是实参 READ 的同一 symbol，alias region 与 owner region 均有效。
- second 的真实 `RELEASE` 对应 `drop(second)` 中的 owner READ，qualifier 保持 SHARED。
- `ERROR.ownerLifetimeRegionId` 指向该 RELEASE 的 region，`relatedNode` 指向真实释放节点。
- 条件重新绑定产生的 weak `COPY` 保持 watcher 的 symbol/alias region，并关联 second 的 owner region。
- 查询只产生一条 descriptor `4004` 的 `weak_value_requires_wake`，主位置仍是 watcher 实参。
- 唯一 related information 指向 `drop(second)` 的实际 owner 位置，消息为 `Owner was released here`。

这些是主用例的完整行为断言，不是 r1 已经满足的事实。
对相关位置的验证使用同一 source 对象和源码 offset，而不只比较显示行号。

## 共享查询与 LSP 投影

[CFG ownership driver](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership.c)
按脚本、函数体和成员等实际边界建立 CFG，运行 forward dataflow，汇总 move/release/violation 观测。
alias 的可能 owner 集合跨分支合流；只有读取策略确实要求 wake，并且集合内存在释放路径，
才应把该释放 cause 解释为当前 weak 读取的违规证据。

[观测发布器](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_observations.c)
优先补充同节点或同范围的已有 weak `ERROR`，避免为同一失败再制造竞争错误。
这仍要求观测阶段正确识别读取策略；找到了一个 weak `ERROR`，本身不代表它就是 wake 错误。

[ownership 诊断投影器](../../zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_ownership_diagnostics.c)
对 weak-after-release 要求有效 symbol、两个 region、非空 related node 和 violation 标记。
它据此构造 weak-wake 诊断，并保留 owner 释放位置。
如果较早阶段没有这些证据，LSP 上的一条 weak-wake 文本诊断不能替代缺失的相关位置。

[LSP query 入口](../../zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_query_diagnostics.c)
在有 AST 时调用共享 CFG 所有权分析，再物化模块范围诊断。
[diagnostic projection](../../zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_diagnostic_projection.c)
保留结构化诊断及其 related information；入口按同 code、同 range 替换和合并已报告项。
这种合并不是另一个所有权求解器，也不能修复 CFG 层没有发布的释放证据。

当前失败处理也有边界：CFG 返回 FALSE 可表示未完整完成，不能理解为没有违规；
该 driver 的部分绑定或事实发布不保证回滚。LSP 当前调用点未检查 ownership driver 的返回值，
不能据其继续物化推导出完整分析已经成功。本文没有新增全局 OOM、事务回滚或错误传播保证。

## using 的释放证据

既有 [region cases](../../tests/language_server/test_ownership_diagnostics_region_cases.h)
分别覆盖 using owner 与 using borrowed alias 的作用域出口。
预期是主体内合法读取不报错，作用域出口发布真实 RELEASE，后续读取关联该释放位置。
owner 情况与 borrowed alias 情况的 qualifier、symbol 和 region 不能互换。

r1 原始观测中，两例均为 `inside=0 outside=0 release=0`。
这里 `inside=0` 和 `outside=0` 表示对应诊断指针为空，`release=0` 表示未查询到所需 RELEASE。
其余打印为 0 的 kind/qualifier/ID 是空指针分支的输出占位，不是一个有效 RELEASE 的字段。
两例因此分别保留以下失败：

- `Using owner did not publish the expected post-scope RELEASE flow`。
- `Using borrow did not end the alias at scope exit`。

这只证明被检查的语义 RELEASE/诊断证据没有达到断言，不能推导为运行时析构或资源释放已经执行失败。
weak 拒绝调用的证据补齐，也不自动解决 using 出口的 RELEASE 生产问题。

新增 [using lower 回归](../../tests/parser/test_semantic_using_release_evidence_cases.h)
只通过真实编译入口准备 resource class 和三个变量声明，不编译 using 本体。
它先确认 resource 尚无 READ，再调用共享 `InferUsingResourceType` 与 `RecordUsingCleanup`，
随后推断主体内和主体外的 borrowed READ，并运行同一 CFG 和诊断物化。
元数据路径若只取得绑定类型而没有发布 resource READ，会到达最终行为断言失败，
不会被当作 fixture 前提错误；READ 的 place 只与真实绑定比较，允许生产路径中的合法零值。
两例分别要求 Shared owner 与 borrowed alias 在出口产生对应身份的 RELEASE，
主体内无违规，主体外唯一 `borrow_escape` 关联实际资源节点。lower r3 两例前提均通过，
实际 `readCount=0`，并到达最终 READ/RELEASE 行为断言失败。r4 中这两条 lower 回归及
既有 LSP using owner/borrow 两例均通过；这只是该 dated 输入上的行为信用，整体收据仍为 FAIL。

当前 using 修复仅改变共享类型取得入口：checked binding lookup 的错误返回 FALSE，
真实 miss 仍回退表达式推断；已知绑定完整复制其 ownership/borrow 限定。
对 canonical identifier publisher 可追踪的绑定，复用身份字段精确相同的既有 READ，
否则调用现成 publisher，而不手写 symbol/type/place 或声明范围。
编译器可能两次访问同一 using resource，因此不能每次无条件追加 READ。
合法 legacy 绑定若不满足 publisher 的来源 gate，仍保持 type-only TRUE；
这不扩大其现有 void publisher 的 OOM 保证，也不把无 READ 的旧绑定当作完整语义身份。
该入口本身不构造 RELEASE，仍由现有 cleanup plan 和 CFG 根据真实 READ 产生出口事实。

## 测试组织与生命周期

[主测试文件](../../tests/parser/test_compiler_semantic_query_diagnostics.c) 保留原 64 个注册及断言，
r1/r2 默认末尾追加四项，总计 68 个唯一用例；r3 加入 using2 后为 70 项，没有 selector。
lower r3 实际执行的是这 70 项。其后再追加直接验证 incoming Shared 参数 `degrade`
可编译的 lower 支持用例，r4-r6 注册为 71 项；它在真实解析和 compiler 初始化后，
把四种编译错误标志作为目标行为断言，在 r4 到达精确行为 RED，不能覆盖或重算 r3 的结果。
该 direct-degrade 用例在 r5 通过，不改变 r4 的失败事实。
随后 [PRIMARY parse-only 回归](../../tests/parser/test_primary_expression_source_range_cases.h)
再追加一项，r7/r8 默认为 72 项；原 71 项顺序、断言和无 selector 的默认入口保留。
后续 R11-support R3/R4 追加三项
[definite-assignment callable identity](../../tests/parser/test_definite_assignment_callable_identity_cases.h)
回归，实际执行默认 75 项；其中 negative 不再锁重复 READ 数量。
R11-support R6 再追加 lambda body-uninitialized 和 conditional-nesting 两例，
默认 query77 与原 LSP84 全 PASS，不删减原 75 项顺序或断言。
当前 runner 为原 83 项加两条分类控制，共 85 个唯一有序 Unity 用例。
新增 `test_weak_to_shared_mismatch_does_not_require_wake` 与
`test_weak_to_plain_mismatch_does_not_require_wake` 插在原 non-wake mismatch 综合用例之后，
原 83 项的相对顺序和断言不变。两例分别走真实 Parse/compile，先锁定 `2008`、
`requiresWake=FALSE`、resolved READ、实际 statement 和无 resolved CALL，再直接要求
`ZrParser_DataflowOwnership_StatementWeakReadRequiresWake` 返回 FALSE。
它们不运行 CFG 或诊断物化；原综合用例仍负责完整释放事实及诊断不变边界，不能被新控制替代。
新增两例在 focused RED r2 均到达最终分类断言并失败；一行修复后的 GREEN r2 行为全通过，
但 source drift 导致该旧运行的严格收据保持 FAIL。后续 GREEN r4 的完整 85 项及固定输入闭包
均通过，见上述新收据；旧 FAIL 不追溯改记。
lower 目标 `zr_vm_compiler_semantic_query_diagnostics_test` 来自
[aot pipeline targets](../../tests/cmake/aot-pipeline-targets.cmake)，
既有 ownership 目标为 `zr_vm_language_server_ownership_diagnostics_test`，其支持配置位于
[LSP query targets](../../tests/cmake/lsp-query-targets.cmake)。
聚合注册仍由 [suite registration](../../tests/cmake/manifest-suite-registration.cmake) 管理，
本次没有以 focused 运行替换原聚合范围。

| 新用例 | 意图与必要控制 | 实际 r1 |
| --- | --- | --- |
| `test_weak_call_owner_set_error_retains_release_evidence` | 条件 weak rebind、second 释放、拒绝 ref 调用无 CALL；检查共享 CFG 与诊断 related location | 行为 RED，达到最终证据断言 |
| `test_non_wake_weak_ownership_mismatches_preserve_diagnostics` | 同一 Unity 用例分别传给 Shared 与 plain Resource 参数；保持 `ownership_mismatch`/`2008` 和原 ERROR 身份，不增加 wake 或释放关联 | PASS |
| `test_weak_by_value_calls_preserve_release_policy` | 合法 Weak 按值参数、真实 VALUE argument mapping；owner 释放后仍不要求 wake | PASS |
| `test_guarded_weak_receivers_preserve_release_policy` | 意图验证释放前后直接 weak receiver 的真实 guard 和 CALL，不新增 wake 错误 | compile 前提 FAIL，未到达控制行为断言 |

三个控制的意图分别约束错误类别、传参模式与 receiver guard，不能用“所有 WEAK 错误都需要 wake”来满足主用例。
r1 只有前两个新控制通过。旧 ownership 套件中的两个 receiver guard 用例 PASS，
也不能替代这个 lower 编译夹具失败的前提；二者入口和夹具不同。
后续夹具修订仍需自己的实际结果，不能追溯修改 r1。

新 lower fixture 使用静态持有的 compiler/AST/source 状态，断言通过 Unity longjmp 退出后，
`tearDown` 先释放编译函数、compiler context 与 AST，再销毁运行时。
双输入 mismatch 控制在两次编译之间也显式清理。
source name 与节点是同一快照内的借用身份，不得跨 cleanup 使用。
reference fact 指针只跨不增长 reference 数组的同步分析使用；ownership 指针在 CFG 后重新查询，
查询物化完成后再读取诊断及 related information，不跨其下一次重建或释放持有这些指针。

## 已执行 r1 与不可变证据

本节对应实际运行 `11489`，不是未来 GREEN 或完整矩阵结果。
收据路径为 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-red-r1-receipt.json`，
SHA256 为 `219DB5C357EEFCBB506D54F266FF168D022CCD34F86B90A90C564C9745D62BEA`。
所有自有子进程已自然终态；build `37372` exit 0，lower native `18320` exit 2，
ownership native `26972` exit 1。

source-pre/post 均为 4792 项，完整文件 SHA256 同为
`14093FB571F054F30BD265790C221794DF2B8A9D40B28BE84474BD8FE2D7A30D`，没有 source drift。
HEAD 前后同为 `a2a324cc7c0f2b6d1baffc564c7283cd888677dd`。
r1 的 lower 输入 main 为 `10AC2054378B084F3D8F5C546203277C33C208956D7DE55CAF1E34D03942699F`，
新增 header 为 `2A5D1C2BCD25EA7D817EBEA414799EE45D6CCBC05660159A3310F665044D259F`。
这些是历史执行身份，不宣称其后的 checkout 或夹具修订仍等于这些 pins。

lower 原始日志为 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-red-r1-weak.log`，
SHA256 为 `09C084F0EC684F9622E1D0C7B9405A357C5B2FAE2DF3A35202D7CF57869FFB72`。
Unity footer 是 `68 Tests 2 Failures 0 Ignored`，即原 64 项及两个新控制 PASS，
主 owner-set 用例行为 FAIL，guard 用例 prerequisite FAIL。
主失败原因为 `Rejected weak ref calls must retain the branch owner release without a resolved CALL fact`；
guard 失败原因为 `Fixture prerequisite: legal weak control did not compile`。

合并 stdout/stderr 还把两个旧用例的名称拆开：
`test_invalid_interface_variance_publishes_structured_semantic_diagnostic` 和
`test_array_element_assignment_builder_publishes_no_fix`。
raw 共含 16 条以下精确形态的编译 stderr，其中每个拆开的名称中插入了四条：

```text
    Compilation error at statement N, resetting error and continuing...
```

只在 RAM 删除该确切形态及其换行，才能重建与原注册完全同序、唯一的 68 个结果。
这不是通用噪声过滤规则，不允许删除未知诊断、FAIL、IGNORE 或其他未解释字节。
原始日志没有改写；控制器原始名称/顺序比较拒绝的收据仍为 `FAIL`，
RAM 重建不能把它倒改为 PASS，也不能抹去两个真实失败。

ownership 原始日志为 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-red-r1-ownership.log`，
SHA256 为 `6252E6B36D3E2BE690EB01C0F50381F990CB2CEA1E5A904DE946491E6A4925C4`。
其自定义协议是 25 个有序唯一 `Unit Test`/result，20 PASS、5 FAIL，不是 Unity footer：

- `Semantic Analyzer Releases Using Owner At Scope Exit`。
- `Semantic Analyzer Releases Using Borrow At Scope Exit`。
- `Semantic Analyzer Links Weak Use To Possible Owner Release`。
- `Semantic Analyzer Rebinds Weak Alias Owner`。
- `Semantic Analyzer Joins Weak Alias Owner Set`。

## 当前实现与待验证边界

兼容性诊断生产阶段现已保存显式的 requires-wake 语义证据，
让 CFG 能识别“已经被类型检查拒绝、但仍确实要求 wake 的直接 READ”。
`SZrSemanticOwnershipFact.requiresWake` 仅在分类为 weak-wake 时设为 TRUE，
完整结构复制保留该标志；共享头变更后需要正常依赖重编，不能复用未更新的 ABI 对象。

显式 rejected-argument 分支要求 resolved WEAK READ、有效 symbol、当前 statement expression 包含该 READ，
以及同一实参节点或无成员 primary wrapper 上的 WEAK ERROR、violation 和 requiresWake。
直接 AST 身份约束不从诊断英文推导语义，也不生成虚假的 resolved CALL。
旧 `ownership_weak_read_has_error` fallback 此前只检查同 node、WEAK ERROR 和 violation，
会把普通 mismatch 判成需要 wake；当前一行修复另要求显式 `requiresWake`。
这不改变合法 weak 观测的发布规则，也不在 query 物化层过滤所有 `requiresWake=FALSE` 的事实。
正常已解析调用继续使用真实参数契约；已有 receiver guard 和 Weak 按值策略必须保持。
禁止通过匹配诊断英文文本、仅看 WEAK qualifier、或无差别补充所有 ownership ERROR 来替代显式证据。

该实现已进入 r3/r4 正常编译和运行，但当时 weak4 均停在共享编译前提，不能记为行为通过。
r5/r6 在 entry-value producer 补齐后，三个 weak 控制通过、主例到达精确行为 RED，已无前提失败。
r4-r6 的 using2 及既有 using owner/borrow 通过，与当时仍失败的 weak 释放证据必须分别记录。
r8 在独立 PRIMARY 范围修复后通过 weak 主例及完整 ownership25，不改写上述历史失败。
该范围契约仅限同一物理坐标系，详见 [PRIMARY 范围模块](primary-expression-source-ranges.md)。
后续仍需保留原 64 项、weak4、using2、direct-degrade、PRIMARY 与完整 ownership25 的顺序及断言。
不得通过减少用例、改写原失败、放宽 source/HEAD gate 或以局部观察替代完整矩阵来获得验收。

### Incoming 参数的 entry value

参数声明先前仅发布 symbol/type/place，sidecar 的 `valueId` 为零；直接 `degrade(owner)`
在没有先前 load 时暴露了 ownership lowering 对已有 slot 必有有效值的假设。
当前 [参数 producer](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c)
SHA256 为 `8D730AC043790766E63207012868F316AE4FAF9BB042EED72DEC1ADB81BA69CD`。
它通过真实 AddLocal/AddValue 为 incoming 参数发布 definition-free external entry value，
把实际 ID 写入 sidecar，而不伪造 READ、LOAD、INITIALIZE 或额外运行时 slot。

values 数组的形状、容量乘法和 UINT32 ID 边界均受检查；四数组预 reserve，
AddLocal/AddValue 同处 TryRun，失败恢复 place/local/slot/value 长度并释放新 projection 缓冲。
严格幂等要求现有 value 的 ID/type/range/definition 均匹配，consumed zero 不会被重复注册复活。
这不是全局 OOM 事务，也未改变通用 ExecIR adapter 的 external-value 接受边界。
详细接口、consumer 与清理限制见
[Callable parameter identity](callable-parameter-semantic-identity.md)。

## 已执行 r3、r4、r5 与 r6

r3 收据 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-mixed-r3-receipt.json`
SHA256 为 `0384CAD8E9B52ECF1E08B36D57C441924635F3118D23A7589CF0CC414AEA5E7B`。
其 4799 source、HEAD、support6、binary3 稳定，八个子进程自然终态，但 raw 保持 FAIL：
query70 是 64 PASS / 6 FAIL（weak4 前提失败、using2 行为失败），ownership25 是 20 PASS / 5 FAIL，
AOT15 是 14 PASS / 1 FAIL。不能把当前 using 修复倒写为 r3 已通过。

r4 收据 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r4-receipt.json`
SHA256 为 `EF7C4826C3578AD1D624D1E84C6F435F7AC08F6728DE40281308A8E3D26C5E44`。
其 4800 source、HEAD `2e2c95d004317014334c9a09de599734d8da9746`、support6、binary3 稳定，
八个子进程自然终态，严格收据仍为 FAIL：

- query71：66 PASS / 5 FAIL / 0 IGNORE。原64和 using2 PASS，weak4 都是 prerequisite FAIL；
  direct-degrade 的目标失败精确为 `degrade must accept a real incoming Shared parameter without a prior load`。
- ownership25：22 PASS / 3 FAIL，剩余为 weak use after release、weak alias rebind、weak owner-set join 三例。
- AOT17：15 PASS / 2 FAIL / 0 IGNORE，旧 scalar 行为失败和新 positive fixture 前提失败；
  negative 单例内五种受控 writer-input shapes 全部完成并 PASS，不能把 positive 前提失败记为行为 RED。

r5 收据 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r5-receipt.json`
SHA256 为 `A84C6B57F6E8A722F625E559756D53FD3C4287221F7A36AE425D7A365649C938`。
Root unified `27138` 于 `2026-10-17T04:28:47Z` 自然 exit 0 并 drained，
正常 build `47160` exit 0，八个子进程均自然终态。
4800 source、HEAD `2e2c95d004317014334c9a09de599734d8da9746`、support6 与 binary3 前后稳定。

- query71：70 PASS / 1 FAIL / 0 IGNORE。direct-degrade、using2、三个 weak 控制及原64均 PASS，
  唯一主例失败精确为 `Rejected weak ref calls must retain the branch owner release without a resolved CALL fact`，
  不再是 fixture 前提失败。
- ownership25：22 PASS / 3 FAIL，仍为 weak use after release、weak alias rebind、weak owner-set join 三例。
- AOT17：15 PASS / 2 FAIL / 0 IGNORE。旧 scalar 和新 positive 均到达行为 RED，没有 fixture 前提失败；
  positive 原因为 `A known dynamic i64 return must refine only its C signature representation`。
  negative 单例内五种 shapes、parameter4、Stamp 和 owned-copy 均 PASS。

收据状态是 `CONTROLLED_LOWER_SUPPORT_OBSERVATIONS_NOT_PLATFORM_ACCEPTED`。
controller 0 只表示预登记的受控观察完成；query、ownership、AOT native 实际分别 exit 1、1、2，
不是整体行为 PASS。RAM controller SHA256 为
`A5A231A5D53714865E66EC4C5A2E8BDDC4CFC9C9CB04E68DC7E89052314BBE76`。
正常三目标及默认 query71 / ownership25 / Windows AOT17 不变；
完整 source/HEAD、support/binary、注册顺序和 prerequisite 门禁未放宽，也不替代完整矩阵。

r6 收据 `E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r6-receipt.json`
SHA256 为 `353AE08DEE38402F1C851F4E608D989B4F48ACD5091E472476A2CF56B3B767E2`。
Root handle `40266` 自然 exit 0 并收尽；正常 build `29420` exit 0，八个子进程全部自然终态。
4800 whole-source、HEAD `2e2c95d004317014334c9a09de599734d8da9746`、support6、binary3 前后稳定。
query71 为 70 PASS / 1 FAIL / 0 IGNORE，唯一失败仍是上述主 owner-set 行为断言；
direct-degrade、using2、三个 weak 控制及原64均 PASS，没有 fixture 前提失败。
ownership25 保持 22 PASS / 3 weak FAIL。AOT17 为 17 PASS / 0 FAIL / 0 IGNORE，
包括旧 scalar、新 immutable positive、单例内五种 negative shapes、parameter4、Stamp 与 owned-copy。
这些 AOT 通过不能替代仍未满足的 weak 释放证据断言。

r6 failure-only 输出显示：真实 ERROR 的 `requiresWake=1`，READ 已 resolved，
READ 范围为 `296..303`；同源码行的真实 expression statement 范围却为 `292..292`，
其 PRIMARY expression 范围为 `284..291`，只覆盖函数名，分类器实际返回 `wake=0`。
这些范围与 source 身份来自原 AST/事实，只读调用没有发布新事实或伪造 CALL。
该观测在 r6 时将下一步限定到独立 parse-only 范围回归；当时对应 parser 修复尚未实施。
不能从这次打印宣称解析问题、CFG 释放证据或全部 ownership 行为已修好；
后续 r7/r8 的独立结果如下，不能倒写进 r6。

r6 状态仍为 `CONTROLLED_LOWER_SUPPORT_OBSERVATIONS_NOT_PLATFORM_ACCEPTED`。
query 与 ownership native 各 exit 1，AOT exit 0；controller 0 只是受控观察契约完成。
正常 query71 / ownership25 / Windows AOT17 范围与严格完整输入门禁均未缩减。

## 日期化 R7/R8 与完整 R11

2026-10-17 UTC 的 r7 收据为
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r7-receipt.json`，
SHA256 `4EBF6E3F8CD04D58B80894E24C025206AD7786DB38EB27D2C0F299D7B4B5E7C3`。
query72 是 70 PASS / 2 行为 FAIL（weak 主例与 PRIMARY 新例），ownership25 是 22 PASS / 3 weak FAIL，
AOT17 全部 PASS。五种真实 source 及嵌套 PRIMARY 的 leaf/CALL/member/return 前提均通过，
范围断言是真实行为 RED；r7 不是整体行为 GREEN。

r8 收据为
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r8-receipt.json`，
SHA256 `FFC86B539C59CB9FD4FE1CFA9F3E19741793DA1AEC8BEE319B17A2E99E9E907D`。
两轮源码间仅 PRIMARY producer 的两处成功 adoption 父范围赋值改变，未新增 weak 专用 fallback。
默认 query72、ownership25 与 Windows AOT17 全部 PASS，无 fixture/IGNORE/顺序或数量漂移；
ownership25 仍按其自定义结果协议核对，不是 Unity footer。
每轮各自 4801 source、HEAD、support6、binary3 前后稳定，八个子进程自然终态。
r8 状态仅为 `FOCUSED_DEFAULT_SUITES_ALL_PASS_NOT_PLATFORM_ACCEPTED`。
参数 entry、using READ 发布和范围修复各自有界，不代表完整 incoming ABI；
混源 PRIMARY 投影当时仍待选择；用户之后批准了保留原始 provenance 与 AST 归属的方向，
但实现和回归仍未获验证，不能用单物理坐标夹具外推。

完整 Windows r11 的 inner 收据
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-receipt.json`
SHA256 为 `7C7D2DDCDCCE2BC12135D53B927DE86D3D5783A9621728E118626C47DB0CFF78`；
outer 收据
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-environment-launch-receipt.json`
SHA256 为 `3AE3BD1E7E4317BD7FEF8D9BB256F3C0ACBD5E9485AEF89D3CBAA98C11D17995`。
foundation52、lower323、original127 全部 PASS；full55 为 53 PASS / 2 FAIL，
失败聚合是 `language_pipeline` 和 `language_server`。
原 semantic analyzer84 的 nested metadata 用例 case7 报一个 read-before-assignment 诊断，
owner generic member signatures 用例也失败；这些事实尚不能确定根因或预告修复结果。

4801 source 前后相同，但 HEAD 从 `2e2c95d004317014334c9a09de599734d8da9746`
变为 `d7d4c8f28a4dd78920ce5ab1bb9e882f68e51dc0`，严格收据保持 FAIL。
后续 CLI gate、outer auxiliary native3+4 与 source-final 未运行。

## 后续 R11-Support R3/R4

本节不重用或改写前述 r10 lower r3/r4 的历史。
R11-support R3 收据
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lower-support-red-r3-receipt.json`
SHA256 为 `8D63E96790549D636DAC7B901EFF6ABEDB8E9C8AAB5BC6690F98B0BD443F06B5`，
保持 FAIL。query75 三个失败中，原两条新正例到达声明隔离/lambda 初始化行为 RED；
第三条 negative 停在 `count==1` 前提，实际同一 return AST 被两次 Infer，产生两条 READ。
该计数假设错误不是第三个行为 RED，后续修订也不能倒改这份 raw。

R11-support R4 收据
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-declaration-identity-r4-receipt.json`
SHA256 为 `8BA8F1F51B8DFC3ED5712305476948EF0FBDAE2AE578929852C5D39B99D0ADE2`，
状态严格为 `DECLARATION_IDENTITY_VERIFIED_OTHER_SUPPORT_RED_NO_PLATFORM_CREDIT`。
build exit 0；query75 为 74 PASS / 1 lambda FAIL，ownership25 与 facts19 全 PASS；
AOT18 为 17 PASS / 1 BOOL FAIL，LSP84 为 83 PASS / 1 nested-lambda FAIL，
guardrail6 为 5 PASS / 1 BOOL FAIL。原 owner generic signatures 已恢复 PASS。
4805 source、HEAD `676c3c88bc39d155c16cae873ee565207b3c166b`、support6、binary6 稳定，
十一子进程全部自然终态。

已执行的 [DA 声明查找](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c)
SHA256 为 `DD206F9BF593DEAAD3C6530F9084BABF232F318C5914E57E0EA6CC8BBEA69EF1`：
先匹配真实 declaration/pattern AST 身份，无命中才允许 known、同源、精确 pattern range 的 synthetic 支持。
这不是按同名重置其他 callable 的状态，也不等于已经隔离所有嵌套 lambda body。
修订 negative 仅要求真实 AST、resolved 同 symbol、所有对应 READ 为 UNINIT，
并保留 descriptor 3002 的精确范围，不锁 READ/诊断重复数。
更详细的声明身份边界见 [参数身份模块](callable-parameter-semantic-identity.md)。

R4 当时的 private lambda AST 归属模块和 unknown/显式 OBJECT 回归尚无新 native 信用。
ownership25 的局部 PASS 不豁免 query/lambda、BOOL 或更广 callable 支持失败；
以下 R6/R7 不倒改 R4 原始失败，也不扩大成完整 incoming ABI 保证。

## R11-Support R6/R7 实测边界

R6 收据为
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lambda-and-origin-red-r6-receipt.json`，
SHA256 `5C66CA772D337D99291669926CBADE3ACC072E94E61C2ADD1869EF04E1496637`。
unified `24087` 自然 exit 0 并 drained，build exit 0，十三子进程全部自然终态。
4810 source、HEAD `83e88fad7c70876c63b1fca86457259ba300d647`、support6、binary8 前后稳定。
query77、原 LSP84 与 ownership25 全 PASS，包括真实 lambda/using、body-uninitialized 与
conditional-nesting；这是有界 lambda 归属证据，不是整体平台通过。
facts20 是原19 PASS/新 standalone BLOCK missing-state FAIL；SemIR4 是原2 PASS/新2行为 FAIL；
token7 是原6 PASS/mixed-postfix 行为 FAIL，原 Span 前提通过但五个 AST anchor 全 false，
parent 为 `0..70`。AOT18 与 guardrail6 各保留原一条 BOOL 行为失败。
R6 状态是 `LAMBDA_IDENTITY_VERIFIED_AOT_AND_PROVENANCE_RED_BLOCK_REGRESSION_NO_PLATFORM_CREDIT`；
controller 0 不是所有 native 套件行为 GREEN。

Root 随后对顶层 BLOCK 调度和 SemIR default UNKNOWN 做了最小修复。R7 收据
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-da-aot-repair-r7-receipt.json`，SHA256
`985AF370C14738D6AD637272505A4DC3348F8A7E9A1898E1063C3B1CE9229B80`，自然 exit 1/drained，仍为 FAIL。
normal build 0；query77/ownership25/AOT18/LSP84/guardrail6/facts20 全 PASS。
SemIR 原2与 UNKNOWN 通过，显式 OBJECT 新例在 header160 失败：`Expected 0 Was 12`。
该夹具错将已有 producer 标注为 GC_REF 的 OBJECT 要求为 plain DYNAMIC；
不是已证明的 dangling 问题，也不授予后续 OBJECT 断言通过信用。
token7 仍为原6 PASS/mixed1行为 RED。同样 4810 source/HEAD/support6/binary8 稳定，
十三子进程全部自然终态。新 OBJECT 夹具/按值身份修正准备中，无 R8 信用。
跨源锚点和相关来源仍待修复验证；详见 [PRIMARY 模块](primary-expression-source-ranges.md)。

## 验收与其他问题

主 lower 用例的历史反例与 r3/r4 严格 FAIL 保留；r5/r6 当时仍有 weak 行为失败。
r8 的 focused 默认套件全通过不覆盖这些旧结果，也不升级实际 r11 FAIL。
完整 Windows、Linux、M1 仍未验收，无提交或 M2 推进信用；Linux 仍未通过。
后续 R11-support R6 的 lambda 通过与 R7 严格 FAIL 均不改变这些门禁；未提交，释放空间为 0 B。
完整 M1-M6 范围与后续平台要求仍以
[设计计划](../plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md) 和
[M1 验收文档](../../tests/acceptance/2026-10-16-macro-m1-token-foundation.md) 为准。
并行外部修改不改变旧收据的输入边界，新运行仍需独立固定完整输入并重新验证。

Stamp 的 AOT partial-ID/frame 问题属于另一条诊断链，见
[AOT generic struct return](../testing-and-validation/aot-generic-struct-return.md)，本文不扩展其归因。
