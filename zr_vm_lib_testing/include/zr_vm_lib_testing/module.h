/**
 * @file
 * ## Summary
 * 公开 Test 阶段的断言 provider 与宿主诊断副本，让脚本异常和 CLI 失败报告共享同一场景。
 * 描述符查询只检查元数据；Register 登记依赖，实际访问仍由宿主的 provider phase 控制。
 *
 * ## Constraints and lifecycle
 * 诊断记录保存在当前 OS 线程的 TLS 中，不按 GlobalState 或用例自动隔离；宿主须管理读取/清理窗口。
 * 记录与快照是内联数组和普通值，不保活 VM 对象；回调参数仅在正常 native 派发期间借用。
 * 回调的 bool 与宿主调用是否完成不同；回调 false 且 threadStatus 正常时，派发层可交付 null，宿主仍返回 true。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:551、zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570 和 zr_vm_cli/src/zr_vm_cli/commands/test_command.c:592 建立用例窗口；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:33 将三个断言绑定到原生回调；docs/library-and-builtins/zr-testing.md 说明 Test phase 与异常投影。
 */
#ifndef ZR_VM_LIB_TESTING_MODULE_H
#define ZR_VM_LIB_TESTING_MODULE_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_TESTING_API ZR_API
/**
 * ## Summary
 * 限制跨 VM/宿主诊断副本的文本容量，使 TLS 记录和宿主按值副本不借用托管字符串。
 * 以下容量均以数据字节计；字段另留一字节 NUL，不表示字符数或完整 UTF-8 边界。
 *
 * ## Value model
 * SNAPSHOT 限制值调试文本，MESSAGE 限制断言消息，TYPE_NAME 限制诊断类型名，SOURCE_FILE 限制源码名称。
 * 只有 snapshot.text 的正常格式化拷贝设置 truncated；其它字段截断没有独立标志。
 *
 * ## Constraints and lifecycle
 * BUG: testing_copy_bounded 按字节裁剪；86 个“€”的258字节跨256字节边界时会留下不完整字符。
 * 该缺陷可影响宿主副本及脚本异常中的文本，不能把 NUL 结尾解释成有效 UTF-8。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:225 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:241 是有界复制；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:370、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:750 和 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:430 是实际文本消费者。
 */
#define ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY 256U
#define ZR_VM_LIB_TESTING_MESSAGE_CAPACITY 256U
#define ZR_VM_LIB_TESTING_TYPE_NAME_CAPACITY 128U
#define ZR_VM_LIB_TESTING_SOURCE_FILE_CAPACITY 256U

/**
 * ## Summary
 * 标识最后一次断言失败由 assert、equal 或 throws 产生，供宿主报告和脚本 AssertionFailure 选择解释方式。
 * 这是断言场景分类，不是 VM threadStatus，也不表示被捕获异常的运行时类型。
 *
 * ## Value model
 * provider 只产生三个具名值；失败记录先清零再写实际分类。清零记录不意味着发生 ASSERT 失败。
 *
 * ## Invariants and lifecycle
 * 与 SZrTestingAssertionFailure 按值复制，不持有线程或 VM 身份。可用记录须由 GetLastFailure 的成功结果确认。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:748 写分类；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:693 投影到异常；
 * tests/testing/test_assertions.c:152、tests/testing/test_assertions.c:188 和 tests/testing/test_assertions.c:306 核对三个场景。
 */
typedef enum EZrTestingAssertionKind {
    /**
     * assert 条件为假形成的失败分类；不是 VM 异常状态或 bool 返回值。
     */
    ZR_TESTING_ASSERTION_KIND_ASSERT = 1,
    /**
     * equal 的规范相等性比较失败分类，对应 expected/actual 两份诊断视图。
     */
    ZR_TESTING_ASSERTION_KIND_EQUAL = 2,
    /**
     * throws 未抛出或异常类型不符的失败分类，不代表 action 实际异常类型。
     */
    ZR_TESTING_ASSERTION_KIND_THROWS = 3
} EZrTestingAssertionKind;

/**
 * ## Summary
 * 保存断言失败时可交付给宿主的有界值视图，而不是保存原 VM 值或精确语义类型身份。
 * equal 保存 expected/actual，throws 保存预期类型或捕获异常；格式化可能执行用户代码。
 *
 * ## Fields
 * typeName 是诊断名称，数值类型会归为 int/float；text 是调试文本。
 * hasValue 表示该视图已被生产者填写，不表示原值非 null；throws 的预期类型视图也可仅填写 typeName。
 * truncated 只报告正常调试文本的裁剪；formatterFaulted 报告格式化异常或没有取得文本。
 *
 * ## Invariants and lifecycle
 * 全部字段按值复制，无 VM 指针或 GC 根；保存副本无需保留原 GlobalState。
 * formatterFaulted 时 text 为 <format-error>，不能将该文本作为参与相等比较的实际值。BUG: 调试格式化含长字符串字段的对象可使core snprintf累计offset越界；入口zr_vm_core/src/zr_vm_core/value.c:604、zr_vm_core/src/zr_vm_core/value.c:628、zr_vm_core/src/zr_vm_core/value.c:695，TLS裁剪不能隔离此core写越界。
 * BUG: 文本和诊断类型名按字节截断，可能以不完整 UTF-8 字符结束；见 testing_copy_bounded。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:297 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:370 生产快照；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1026 和 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1059 生产仅预期类型视图；
 * tests/testing/test_assertions.c:189 和 tests/testing/test_assertions.c:395 核对裁剪与格式化故障。
 */
typedef struct SZrTestingValueSnapshot {
    /**
     * 有界诊断类型名称；数值可归为 int/float，长度按字节截断，不是 canonical TypeId。
     */
    TZrChar typeName[ZR_VM_LIB_TESTING_TYPE_NAME_CAPACITY + 1U];
    /**
     * 有界调试文本或 <format-error>；NUL 结尾不保证 UTF-8 完整，不持有原 VM 值。
     */
    TZrChar text[ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY + 1U];
    /**
     * 视图已被生产者填写；合法 null 值也可为真，throws 的预期类型可只有 typeName。
     */
    TZrBool hasValue;
    /**
     * 正常调试文本拷贝被裁剪的标志；不覆盖 typeName、message 或 sourceFile 的截断。
     */
    TZrBool truncated;
    /**
     * 调试格式化抛错或没有返回文本的标志；与断言比较结论分开解释。
     */
    TZrBool formatterFaulted;
} SZrTestingValueSnapshot;

/**
 * ## Summary
 * 给宿主和脚本异常保存原生断言最近 VM 调用帧的源码范围，帮助把失败归因到调用位置。
 * 它投影 VM 当前执行位置，不能当成任意 AST 节点或测试声明的完整范围。
 *
 * ## Fields
 * sourceFile 是有界源码名称；四个坐标取自该帧最后一个不晚于当前指令的位置记录。
 * 位置缺失时相应坐标为零；有帧/源码名称但没有匹配位置记录时仍可能只有 sourceFile 非空。
 *
 * ## Invariants and lifecycle
 * 内联字符数组和整数可按值保存，不借用 Function、String 或 CallInfo；不会保持调试元数据可达。
 * TODO: endColumn 的包含/排除边界需沿执行位置记录生产者核对，宿主不要仅凭字段名推定切片规则。
 * 核查入口为 zr_vm_core/src/zr_vm_core/io_runtime.c:695 的位置生产者及 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:439 的消费者。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:399 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:440 投影最近 VM 帧；
 * tests/testing/test_assertions.c:503 至 tests/testing/test_assertions.c:506 只核对源码名称、正起始行与行范围顺序。
 */
typedef struct SZrTestingSourceSpan {
    /**
     * 按字节限制的源码名称副本；没有匹配坐标记录时仍可能非空。
     */
    TZrChar sourceFile[ZR_VM_LIB_TESTING_SOURCE_FILE_CAPACITY + 1U];
    /**
     * VM 位置记录的起始行，缺少匹配记录时为零；不是测试声明的默认起始行。
     */
    TZrUInt32 startLine;
    /**
     * VM 位置记录的起始列，缺少匹配记录时为零；不独立证明范围完整。
     */
    TZrUInt32 startColumn;
    /**
     * VM 位置记录的结束行，缺少匹配记录时为零；宿主须结合 startLine 解读。
     */
    TZrUInt32 endLine;
    /**
     * VM 位置记录的结束列；包含/排除边界沿类型 TODO 核对，不能推定字符串切片规则。
     */
    TZrUInt32 endColumn;
} SZrTestingSourceSpan;

/**
 * ## Summary
 * 向宿主交付同一 OS 线程最近一次结构化断言失败的独立副本，方便 CLI 报告而无需重读异常对象。
 * 脚本 AssertionFailure 的字段由此记录另行构造；C 结构不是 VM 对象布局或跨进程序列化格式。
 *
 * ## Fields
 * assertionKind 选择失败场景；sourceSpan 与 message 给出调用位置和消息。
 * equal 填 expected/actual，throws 填 expected/exception；未参与当前场景的视图保持清零。
 * 消息或坐标为空不表示没有失败，须以 GetLastFailure 的返回值区分。
 *
 * ## Invariants and lifecycle
 * 三个快照、消息和位置全部内嵌，副本不持有 VM 对象；读取不清除 TLS。
 * TLS 不按 GlobalState 分区，同一线程的另一次失败或成功断言可以覆盖/清除原记录，宿主应先复制再清理。
 * 普通副本可由宿主保存；struct/enum 布局仍遵循同一头部与构建 ABI，不是稳定 wire protocol。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:746 重置记录，zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:795 按值复制；
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570 读取，zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:693 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:702 投影脚本异常。
 */
typedef struct SZrTestingAssertionFailure {
    /**
     * 当前失败场景分类，只有 GetLastFailure 成功时才表示有效记录。
     */
    EZrTestingAssertionKind assertionKind;
    /**
     * 最近 VM 调用帧的位置副本；缺少调试记录时可能为空或只有源码名称。
     */
    SZrTestingSourceSpan sourceSpan;
    /**
     * 有界失败消息；没有单独的裁剪标志，空文本不表示没有失败。
     */
    TZrChar message[ZR_VM_LIB_TESTING_MESSAGE_CAPACITY + 1U];
    /**
     * equal 的预期值或 throws 的预期类型视图；其它场景清零。
     */
    SZrTestingValueSnapshot expected;
    /**
     * equal 的实际值视图；不持有原值，也不能据调试文本替代规范相等性判断。
     */
    SZrTestingValueSnapshot actual;
    /**
     * throws 不匹配时的捕获异常诊断视图；不是异常对象所有权或重新抛出句柄。
     */
    SZrTestingValueSnapshot exception;
} SZrTestingAssertionFailure;

/**
 * ## Summary
 * 借出 Test provider 的静态描述符，让宿主核查断言签名、属性角色和 phase，而不创建 VM 或物化导出。
 *
 * ## Returns
 * @return 当前 provider 的非 NULL const 静态描述符；嵌套表、字符串、回调均借用，不修改或释放。
 *
 * ## Constraints and lifecycle
 * 查询无需 VM 或 Test phase；在 VM 中访问导出仍须 Register 并选 Test phase。
 * provider 必须保持加载到最后一次描述符/回调消费结束。
 *
 * ## References
 * tests/testing/test_assertions.c:59 和 tests/library/test_official_provider_convergence.c:415 查询契约；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:8 转交，zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:153 返回静态表。
 */
ZR_VM_LIB_TESTING_API const ZrLibModuleDescriptor *ZrVmLibTesting_GetModuleDescriptor(void);
/**
 * ## Summary
 * 在宿主启动链登记 zr.task 内建类型、附着 native registry，再登记 zr.testing Test provider。
 * 登记使依赖与描述符可被查询，实际物化/访问权限仍由宿主 provider phase 决定。
 *
 * ## Parameters
 * @param[in,out] global 宿主拥有的有效 GlobalState，mainThreadState 已建立；NULL 正常返回 ZR_FALSE。
 *                      登记链可按需 Attach registry，不转移或释放 global。
 *
 * ## Returns
 * @return global 为 NULL 或某阶段报告失败时为 ZR_FALSE，全部阶段报告成功时为 ZR_TRUE。
 *         失败不回滚前面成功的 task/registry 登记；宿主处理部分状态和 GlobalState 的清理。
 *
 * ## Constraints and lifecycle
 * 注册不自动把执行 state 切换为 Test phase，Runtime/CompileTool 宿主不能据此使用测试导出。
 * 同一域的登记与执行同步由宿主协调；描述符/回调仍被 VM 使用时 provider 不得卸载。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:83 和 tests/testing/test_assertions.c:25 传播登记结果；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:13 至 zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:17 建立依赖链；
 * tests/testing/test_assertions.c:109 和 tests/testing/test_assertions.c:123 核对访问 phase。
 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Register(SZrGlobalState *global);
/**
 * ## Summary
 * 结束当前 OS 线程的失败记录窗口，供宿主在用例开始前和完成报告后避免旧记录误归因。
 *
 * ## Returns
 * 无返回值；清零当前线程记录并使 GetLastFailure 返回 ZR_FALSE，不清除 VM currentException。
 *
 * ## Constraints and lifecycle
 * 无需 VM；不会清除其它 OS 线程的记录，但同一线程中不同 GlobalState/重入用例共享这个窗口。
 * 已由宿主复制的普通值副本不受影响。成功的三个断言回调也会调用此入口。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:551 和 zr_vm_cli/src/zr_vm_cli/commands/test_command.c:592 管理用例窗口；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:771 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:772 清理 TLS。
 */
ZR_VM_LIB_TESTING_API void ZrVmLibTesting_ClearLastFailure(void);
/**
 * ## Summary
 * 把当前 OS 线程的最近结构化失败复制给宿主，以便独立报告而不消费或清除 TLS 原记录。
 *
 * ## Parameters
 * @param[out] outFailure 宿主拥有的可写完整结构；成功时被整体覆盖，NULL 或无记录时保持不变。
 *
 * ## Returns
 * @return 存在记录且 outFailure 非 NULL 时为 ZR_TRUE；否则为 ZR_FALSE，输出不变。
 *         成功输出是普通值副本，无 VM 对象所有权，宿主保存后可另调 ClearLastFailure。
 *
 * ## Constraints and lifecycle
 * 无需 VM；只读取调用线程 TLS，不按 GlobalState 查找，也不替宿主关闭用例窗口。
 * 读取失败时不得使用未初始化的输出；重复读取成功会复制同一记录。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570 在失败报告分支读取；tests/testing/test_assertions.c:151 核对副本；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:792 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:796 定义输出边界。
 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_GetLastFailure(SZrTestingAssertionFailure *outFailure);
/**
 * ## Summary
 * 实现 zr.testing.assert(condition[, message]) 的原生回调，把不满足条件转换成宿主记录及脚本可捕获异常。
 * 这是描述符派发入口；宿主通常通过 ZrLib_CallModuleExport 调用，不直接手造 context。
 *
 * ## Parameters
 * @param[in,out] context 派发期间借用的有效 context/state；一或两个实参分别为 bool 和可选 string。
 * @param[out] result 派发者拥有的有效输出槽；正常成功写 null，不转移 context、参数或 VM 的所有权。
 *
 * ## Returns
 * @return 条件为真且参数有效时为 ZR_TRUE，并清除当前线程上次失败；无有效 context/result 或形状检查失败可返回 ZR_FALSE。
 *         条件为假调用不返回的 VM 异常入口，不应把失败断言理解成普通 bool false。
 *
 * ## Constraints and lifecycle
 * context/参数仅在 native 调用期间有效，异常须由正常 VM/宿主异常边界处理。
 * 断言失败先保存 TLS 再构造 AssertionFailure；仅软返回失败走 VM 运行错误回退，GC分配OOM可直接抛错。BUG: Normalize把脚本exception快照字段覆为self，宿主TLS仍为快照；入口zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:57、zr_vm_core/src/zr_vm_core/exception.c:407、zr_vm_core/src/zr_vm_core/exception.c:684。
 * 注册/物化仍遵循 Test phase，成功清理 TLS 不清理其它线程记录。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:34 注册回调；tests/testing/test_assertions.c:142 和 tests/testing/test_assertions.c:148 通过正常宿主入口调用；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:821 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:839 是回调边界。
 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Assert(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现 zr.testing.equal<T> 的断言回调，按 VM 规范相等性判断；失败后才制作 expected/actual 诊断视图。
 * 快照格式化与裁剪不能作为相等性判断，也不替换被比较的 VM 值。
 *
 * ## Parameters
 * @param[in,out] context 派发期间借用的有效 context/state，恰两个值实参依次是 actual、expected。
 * @param[out] result 派发者拥有的有效输出槽；相等成功时写 null，调用方不取得被比较值所有权。
 *
 * ## Returns
 * @return 参数有效且相等时为 ZR_TRUE，并清除线程失败记录；参数槽/形状无效可返回 ZR_FALSE。
 *         不相等时保存两个快照并通过不返回的 VM 异常入口抛 AssertionFailure，而非正常返回 false。
 *
 * ## Constraints and lifecycle
 * 相等比较按 CompareDirectly/Value_Equal 的值语义执行；后续快照调试格式化可能执行 VM/用户代码。
 * 格式化失败标 formatterFaulted 和 <format-error>，TLS 副本不保活 actual/expected。BUG: TO_STRING内失败Assert覆盖外层TLS；成功Assert或显式ClearLastFailure清空记录，Equal仍构造失败；TODO: 补嵌套formatter回归及保存恢复。
 * BUG: 有界快照可按字节裁剪到 UTF-8 字符中间；见 testing_copy_bounded 与 SZrTestingValueSnapshot。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:37 注册回调；tests/testing/test_assertions.c:178 和 tests/testing/test_assertions.c:185 是实际相等/失败调用；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:875 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:888 建立比较和失败快照顺序。
 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Equal(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现 throws<E>(action) 的同步断言，捕获 action 的异常并匹配 Error 派生类型 E，成功交付捕获异常值。
 * 公开脚本只传无参同步 action；原生回调还接收编译器注入的 canonical E TypeId，不可省成一个原生参数。
 *
 * ## Parameters
 * @param[in,out] context 派发期间借用的有效 context/state；恰两个 native 实参为同步 callable 和隐藏 E TypeId。
 * @param[out] result 派发者拥有、已初始化且遵循 VM 值复制协议的输出槽；成功复制捕获的异常值。
 *
 * ## Returns
 * @return 捕获异常匹配 E 时为 ZR_TRUE，复制结果后清除 VM currentException 与当前线程失败记录。
 *         参数/类型验证或异常状态规范化失败可返回 ZR_FALSE；action 未抛出或类型不符则另抛 AssertionFailure。
 *         这是回调完成状态；回调拒绝且线程仍正常时，宿主入口可返回 ZR_TRUE/null，不能仅据宿主 bool 判断断言已执行。
 *         result 是 VM 值，须沿现有 VM 结果/所有权协议使用，不能像诊断副本那样当普通 C 文本保存。
 *
 * ## Constraints and lifecycle
 * 这是同步捕获，不等待 Task；E 必须为 Error 或其派生类型。action 后重读隐藏身份，避免持有跨 GC 搬迁的旧参数指针。
 * BUG: AssertionFailure的exception自字段TO_STRING可抛错，快照清currentException后将原本匹配判成错型；Fine+NULL文本也会清异常。入口为
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1048 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1054。
 * TODO: 经公开ObjectPrototype_AddMeta安装抛错formatter，组合现有throws<AssertionFailure>(assert(false))补回归；equal formatterFaulted测试未覆盖此分支。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:41 注册 native 两参回调；tests/testing/test_assertions.c:297、tests/testing/test_assertions.c:324 和 tests/testing/test_assertions.c:432 走正常 export 调用；
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619 和 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1997 区分回调拒绝与宿主完成；tests/testing/test_assertions.c:337 核对非法 E 不执行 action 的 null 路径；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1007 至 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1080 实施同步捕获/类型重读/结果交付。
 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Throws(ZrLibCallContext *context, SZrTypeValue *result);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/**
 * ## Summary
 * 通过动态加载器约定的固定 v1 符号借出 zr.testing 描述符；加载器先做 ABI/登记检查，后续解析或物化时另查 phase。
 * 该查询不等价于宿主 Register，不主动登记 zr.task 依赖或更改 state 的访问 phase。
 *
 * ## Returns
 * @return 当前 provider 的非 NULL const 静态描述符；嵌套元数据和回调借用，不修改或释放。
 *
 * ## Constraints and lifecycle
 * 仅 ZR_LIBRARY_TYPE_SHARED 构建声明；符号名称须与加载器查询约定一致，描述符 ABI 另行检查。
 * provider 必须保持加载到最后一个描述符/回调消费者结束。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:370 查找符号，zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:383 调用；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:22 转交描述符查询。
 */
ZR_VM_LIB_TESTING_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif
