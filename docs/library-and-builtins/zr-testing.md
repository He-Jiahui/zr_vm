---
related_code:
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
implementation_files:
  - zr_vm_lib_testing/src/zr_vm_lib_testing/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_parser/src/zr_vm_parser/test_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/2026-07-20-14-test-function-harness-design.md
tests:
  - tests/testing/test_test_role_binding.c
  - tests/testing/test_assertions.c
  - tests/artifact/test_manifest_roundtrip.c
  - tests/fixtures/projects/testing_reference
  - tests/testing/test_runner.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/debug/test_debug_agent_protocol.c
  - tests/migration/test_percent_test_migration.c
doc_type: module-detail
---

# zr.testing Test-Phase Provider

## Public Contract

`zr.testing` is the official N3 Test-phase provider. Runtime hosts cannot
consume it. A test host may consume Runtime and Test providers, but never
CompileTool providers. Its descriptor owns three retained metadata roles:

- `zr.testing.test` marks an ordinary top-level `fn` as a test root.
- repeatable `zr.testing.case` supplies compile-time constant arguments.
- `zr.testing.skip` retains one non-empty skip reason.

The callable surface is `assert`, generic `equal<T>`, and synchronous
`throws<E>`. The compiler binds the public `throws<E>(fn() -> void)` call and
passes canonical `E` TypeId metadata through a compiler-hidden native argument;
the runtime accepts only the exact exception type or an allowed subtype.
Failures are bounded structured `AssertionFailure` values rather than
process-global text matching. They carry the caller source span and bounded
type/value snapshots. In `equal`, a value formatter fault sets the snapshot's
`formatterFaulted` flag when formatting throws or returns no text, without
replacing the comparison failure. The `throws`
path snapshots a caught exception before matching its type. BUG: a throwing
formatter clears the caught exception, so the later match loses the original
value. TODO: add the public formatter/throws regression. The provider
descriptor, phase, role ids, and public contract hash participate in the
official-provider inventory.

The current `testing_copy_bounded` helper truncates these diagnostic strings by
byte count. A multibyte UTF-8 character crossing the limit can leave an invalid
trailing byte sequence in the copied failure message or snapshot; see its `BUG:`
review note in `runtime/assertions.c`. The existing assertion tests do not cover
this boundary case.

## 宿主 C 接口与失败副本

公共头部的八个 C 接口保持原签名。查询描述符、登记 VM、调用断言和读取诊断副本有各自的使用窗口。

| 入口 | 调用目的 | 使用边界 |
| --- | --- | --- |
| GetModuleDescriptor | 核查静态 Test provider 签名、属性和 phase。 | 无需 VM；返回 const 静态借用表，provider 须活到最后一次消费。 |
| Register | 登记 zr.task、按需附着 native registry，再登记 zr.testing。 | 有效 global/mainThread；不自动切 Test phase，失败保留前序登记，不回滚。 |
| ClearLastFailure | 在当前 OS 线程开始或结束诊断窗口。 | 不清 VM currentException，也不清其它线程记录；已复制值仍有效。 |
| GetLastFailure | 按值复制当前 OS 线程的最近失败。 | out 非空且有记录才成功；false 时不改输出，不消费记录，不按 GlobalState 分区。 |
| Assert | 把 false 条件转成结构化失败与 VM 异常。 | 正常 native context；一或两个实参，成功写 null 并清 TLS；失败由 VM 异常边界处理。 |
| Equal | 先作规范相等比较，失败后制作 actual/expected 诊断。 | 两实参；格式化故障不是相等性结论；有界文本仍有按字节截断 BUG。 |
| Throws | 同步捕获 action 异常并匹配 Error 派生 E。 | public 一实参/native 两实参含隐藏 TypeId；result 沿 VM 初始化/复制协议，不按诊断结构处理。 |
| ZrVm_GetNativeModule_v1 | 共享 provider 的固定描述符查询符号。 | 仅共享构建；查询不执行宿主 Register 依赖链；加载器先查 ABI/登记，后续解析或物化时查 phase。 |

诊断结构全部内嵌字符数组与普通值，不保活 VM 对象，宿主可保存成功读取的副本。
TLS 以 OS 线程为单位，同一线程的多个 GlobalState 或重入用例共享它；宿主应先读取再清理。
`hasValue` 表示视图有生产记录，合法 null 值仍可为真，Throws 的 expected 还可能只有 typeName。
`truncated` 只报告正常 snapshot.text 的裁剪，message/sourceFile/typeName 没有独立标志。
没有可用 VM 帧/调试元数据时 source span 清零；有源码名称但没有匹配位置记录时可以只有 sourceFile 非空。
TODO: endColumn 的包含/排除语义仍须沿执行位置生产者核对，不能仅凭字段名推定字符串切片规则；
具体从 `zr_vm_core/src/zr_vm_core/io/io_runtime.c:701` 的 instructionRanges 复制继续追踪。

原生回调 bool 与宿主 `ZrLib_CallModuleExport` 的 bool 含义不同：回调 false 且 threadStatus 正常时，
派发层把结果置 null，宿主仍可返回 true。非法 E 的用例验证 action 未执行和 null 结果；
宿主 true 只说明调用完成且没有 VM 异常，不能作为断言已经执行的证明。

当前证据为 `zr_vm_cli/src/zr_vm_cli/commands/test_command.c:551`、`zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570`、`zr_vm_cli/src/zr_vm_cli/commands/test_command.c:592` 的诊断窗口，
`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:771`、`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:792`、`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:795` 的 TLS 边界，
`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:304`、`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1026` 的 hasValue 生产者，
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1910` 的两层返回路径，
及 `tests/testing/test_assertions.c:337` 的非法 E 调用。
本节描述公共头部契约；后续内部审查记录见下节，全模块及运行时验收仍需各自当前证据。

## TestManifest

Test compilation type-checks ordinary functions and emits a versioned binary
`TestManifest`. Each entry carries the bound function's canonical semantic
SymbolId/TypeId, callable child index, module-qualified name, source range,
async bit, skip reason, and constant case arguments. The manifest carries the
compiler's module signature identity rather than a name-derived surrogate.
Decode rejects unknown schema versions, oversized counts, invalid booleans,
truncation, trailing bytes, and overlong strings. Cleanup is valid for partially
decoded input, including allocation failure and corrupt-count paths.

Production compilation still type-checks test declarations, then trims their
roots and emits no manifest. A production declaration cannot call a trimmed
test function. There is no hidden `main`, source-text test discovery, or old
`%test` AST/lowering path.

## Async And Isolation Boundary

Synchronous tests return `void`. Async tests explicitly return
`zr.task.Task<void>`; the host awaits the result through the canonical task
contract. Test execution happens in a fresh global state, and the CLI process
runner can place each selected case in a child process with timeout and output
capture.

## Tooling Projection

LSP run/debug/case CodeLens is derived from the compiled `TestManifest`; an AST
decorator that fails test-role binding cannot produce a lens. The debugger
decodes the same manifest and projects the active test's canonical symbol,
type, module, case, and qualified name onto stack frames. Parameter values stay
ordinary Arguments-scope values, and async tests retain the scheduler's logical
stack contract. Legacy `%test`, draft `test fn`, and `#zr.test.*#` are accepted
only by migration or negative fixtures, never by the production parser.

## 内部断言 bridge 的当前审查

本批登记 assertions.c 的63个实际单元：21函数、2个同步请求类型及5字段、2个TLS状态、TLS宏、
文件/依赖边界和30个有独立契约的语句块。普通局部guard与字段适配流程随所属函数审查；
不是按花括号数制造覆盖条目。旧27条粗粒度记录替换为当前路径/行号证据；公共头部38项覆盖保持，Snapshot/Assert/Equal/Throws四项同步已证BUG及软失败边界，
其中30处内部数字References按保护行映射迁移；旧TODO行530的引用更正为实际snapshot调用532。内部文件已在2b667bb提交中按63项完整性、当前台账及实际编译对照闭合；
本批只迁移其反向数字引用，不授整个Testing模块或运行时验收。

### 同步异常与诊断窗口

`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:141` 的TryRun包装保存/恢复stackTop、CallInfo base/top/return anchors及handler深度，
使内部action或formatter结束后继续外层调用位置。它不保存/恢复currentException、threadStatus或TLS；
TryRun返回状态、CallRequest.completed宿主完成状态、host export bool是各自不同的事实。
本descriptor未启用stack-root opt-in；正常Equal/Throws按实参走标量快路或inline-pinned稳定副本及必要pin，不能把公开参数借用误报为现行悬空BUG。
Throws在`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1012`先验证隐藏E TypeId，`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1020`动作后重读；非法E不执行动作。
仅同一同步action受捕获，不等待Task；匹配成功在`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1076`先复制VM异常结果，再清VM异常和TLS。
GetLastFailure交付的普通C内嵌副本无VM所有权，和这个result不能互换。

快照区分缺少输入与合法VM null，先诊断typeName后格式化；hasValue、text-only truncated和
formatterFaulted不表示同一状态。源码范围选择最近VM帧和不晚于PC的位置表前驱；源码名可有而坐标为零。
UTF-8按字节截断BUG及endColumn边界TODO仍保留，现有ASCII/line范围测试不证明UTF-8或列边界。

### 分配失败与GC边界

当前 `SetString` 在字符串创建返回NULL时先Throw(MEMORY_ERROR)并return；有效state/output的普通返回已初始化fieldValue，不能继续沿旧短串表失败前提推断未初始化读取。
这是当前callee对旧BUG前提的证伪，不归因于本批修复，未运行故障注入。字段写入的void wrapper在有效state失败时也可Throw，后续字段和普通清理不保证执行；先前写入不整体回滚。
证据：`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1317`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:12`。

TODO: new_record返回的是未建立native root/pin的普通INITED/current-generation raw对象；
注册原型虽永久，新record不是永久。`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:616`/`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:648`先创建字符串再进入字段setter的owner pin，
后续setter间和outer对象跨nested构造也有裸地址窗口。强制full/compacting GC核查根及搬迁重取，
不把当前代暂不判未引用写成已证跨代安全或UAF。
TODO: `zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:708`的NormalizeThrownValue在字段/traceback分配后才发布currentException根；
核查outer/nested/payload的临时根。有效state下void字段wrapper失败可Throw并跳过后续投影；先前字段不整体回滚，不能据void接口推断静默完成。
TODO: TypeId参数pin仅保证其对象根，canonical name仍为borrowed；沿`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:960`→`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:962`
及FindPrototype内部字符串创建/限定名导入，核查跨分配地址稳定性。

### 脚本异常字段与宿主快照

BUG: zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:243将AssertionFailure.exception声明为ValueSnapshot；`zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:701`先投影该快照，
随后Normalize已有Error把exception覆为self(zr_vm_core/src/zr_vm_core/exception/exception_errors.c:433、zr_vm_core/src/zr_vm_core/exception/exception_errors.c:629)。底层Object_SetValue
不检查descriptor字段类型；object/object.c:2393、object/object.c:2399的成员读取优先返回own nodeMap，
普通成功构造后的脚本failure.exception因此是AssertionFailure自身。宿主C TLS exception仍是普通快照，
不受该VM字段覆写影响。TODO: 沿投影→Normalize→脚本成员读取补schema回归并核查Error规范化协议。

### formatter重入与关联模块

BUG: `zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:358`可执行用户TO_STRING；失败Assert覆盖TLS，成功Assert或显式ClearLastFailure清空外层记录；
TryRun恢复VM帧不会恢复归因。@toString内assert(false)使外层投影内层ASSERT；assert(true)清空TLS，外层仍抛错。
若native formatter经宿主CallModuleExport重入，host false是被宿主捕获的状态；必须再Throw才能进入formatter外层TryRun。
TODO: 补嵌套回归并核查保存恢复方案；本轮未执行该组合fixture。
BUG: `zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1048`格式化currentException时，公开AssertionFailure原型可经ObjectPrototype_AddMeta安装抛错TO_STRING；
合法throws<AssertionFailure>(assert(false))的Normalize将exception字段写为self，DebugString字段转换触发该元方法，
snapshot ClearCurrent之后CatchMatches(null)为false，把原本匹配的异常判成错型。Fine+NULL文本也走同一清理分支。
证据为 `zr_vm_core/include/zr_vm_core/object.h:525`、`zr_vm_core/src/zr_vm_core/object/object.c:2022`、
`zr_vm_core/src/zr_vm_core/exception/exception_errors.c:433`、`zr_vm_core/src/zr_vm_core/exception/exception_errors.c:629`、
`zr_vm_core/src/zr_vm_core/value/value.c:600`、`tests/testing/test_assertions.c:250`、`tests/testing/test_assertions.c:432`。
TODO: 补此正常公开入口组合回归及异常保存恢复；现有Equal fault测试不覆盖该Throws分支，本轮未执行。
BUG: core Value_ToDebugString的合法对象若含超过buffer容量的字符串字段，snprintf所需长度直接累计offset，
后续尾括号及终止NUL写越界；入口 `zr_vm_core/src/zr_vm_core/value/value.c:604`、
`zr_vm_core/src/zr_vm_core/value/value.c:628`、`zr_vm_core/src/zr_vm_core/value/value.c:695`。
TODO: formatter元方法GC期间obj/pair裸遍历地址仍需对应core模块强制GC复核；
本批只记录跨模块BUG，不修改活跃core实现，不授其运行时信用，未运行长字段或GC fixture.

描述表/注册批次现已细分82项：descriptor.c 69、module.c 3、runtime_internal.h 2、Testing CMake 8。
类型/字段/参数/泛型/角色/链接及15个显式descriptor成员均逐项核对；两份已闭合断言源码和公开头只迁移反向引用。
parser-only test_test_role_binding只作schema cross-check，不是这些native表的直接调用者。
本批四文件在当前源/台账、实际编译对照和独立完整性审查一致后才闭合，不授整个运行时验收。


## 描述表、直接注册与共享加载

公开参数表与native arity分别记账：throws只声明一个action，第二个native实参是编译器隐藏的E TypeId；
泛型E受Error约束。字段类型是描述/反射schema，普通对象setter不据此阻止exception被Normalize覆为self。
普通ValueSnapshot.typeName是诊断桶/原型名，Throws expected-only可为预期类型名；不承诺canonical identity。
hasValue可表示合法null或expected-only，formatterFaulted包括抛错和Fine但没有文本；旧英文descriptor文案的三处偏差保留TODO。
函数、类型、字段以及模块documentation当前未进入对应FunctionInfo/TypeInfo/field/ModuleInfo和结构hash；
参数、泛型和moduleLink文案有复制入口，反射展示其它文案须另核发布实现。

提供者attributeRoles镜像parser builtin并参与结构hash及直接测试核查；compiler实际读parser自身schema。
Register按task builtin、Attach、Testing登记顺序短路，失败不回滚前序；CLI释放新global，直接宿主管理残留。
phase由宿主在注册成功后另设TEST，getter和Register均不自动切换；链接task导出在物化时另解析zr.task。
共享v1入口只交付静态descriptor，不执行Register。静态表和嵌套字符串/数组由provider持有，注册记录和闭包借用直到消费结束。

本地CMake八项分别说明配置入口、基名、common并入/变体开关、三项PRIVATE依赖、公开头和库安装；
私有runtime头不安装，dependency头的PRIVATE路径不等于消费者自动具有完整include契约。
CommonMacros共享目标定义SHARED=1，static目标不定义它；自定义SHARED=0下defined guard与数值ZR_API条件差异留TODO。
双变体Windows同名lib冲突及模块名PUBLIC宏用途沿共用helper已有BUG/TODO继续核查，不改其行为。
