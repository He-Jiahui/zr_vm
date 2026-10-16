#include "runtime_internal.h"

#include "zr_vm_parser/attribute_contract.h"

/**
 * ## Summary
 * 为 assert 的公开签名提供条件和可选消息元数据；回调仍按 native context 读取实参。
 *
 * ## Value model
 * condition 必须为 bool；message 按值传入，公开调用可省略。
 * 两条描述项不代表每次调用都携带两个参数。
 *
 * ## Invariants and lifecycle
 * 注册校验读取passingMode并拒绝越界模式；这不执行回调。表和字符串静态只读，随 provider 借给注册表和签名元数据消费者；实际参数只在同步派发窗口借用。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:604
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:568
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:607
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:312
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:92
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:823
 * tests/testing/test_assertions.c:142
 */
static const ZrLibParameterDescriptor g_assert_parameters[] = {
/* assert 判定输入；false 进入结构化失败链；bool 按值参数；不是返回槽。 */
        {"condition", "bool", "Condition required to be true.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
/* 可选的失败诊断消息；公开可省略；消息被有界复制，不转移原字符串。 */
        {"message", "string", "Optional failure message.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
};

/**
 * ## Summary
 * 让 equal<T> 的签名消费方识别 actual/expected 的只读输入关系，避免把 IN 当输出或所有权转移。
 *
 * ## Value model
 * 两项均使用同一泛型 T；IN 描述输入 passing mode，最终相等性由 Equal 的规范比较决定。
 *
 * ## Invariants and lifecycle
 * 这张表不是值副本或 GC root；不能由字段名推导两个参数对象的地址在分配后仍稳定。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:604
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:568
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:607
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:312
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:92
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:290
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:875
 */
static const ZrLibParameterDescriptor g_equal_parameters[] = {
/* 被比较的实际输入；与 expected 同属 T；IN 借用。 */
        {"actual", "T", "Actual value.", ZR_LIB_PARAMETER_PASSING_MODE_IN},
/* 被比较的预期输入；与 actual 同属 T；IN 不表示输出。 */
        {"expected", "T", "Expected value.", ZR_LIB_PARAMETER_PASSING_MODE_IN},
};

/**
 * ## Summary
 * 公开 throws<E> 只接收一个同步 action；将编译器隐藏的 E TypeId 与用户参数清单分开。
 *
 * ## Value model
 * action 是 fn() -> void；E 从泛型元数据传入第二个 native 实参。
 * 异步 Task 结果不属于此捕获窗口。
 *
 * ## Invariants and lifecycle
 * 表只描述公开参数；native min/max=2 另约束运行桥，不得向用户要求第二个 TypeId 参数。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:604
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:568
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:607
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:312
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:92
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1012
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1017
 */
static const ZrLibParameterDescriptor g_throws_parameters[] = {
/* 同步异常捕获的唯一公开参数；fn() -> void；不等待 Task，另有隐藏 E TypeId。 */
        {"action", "fn() -> void", "Synchronous action required to throw E.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
};

/**
 * ## Summary
 * 给 equal 的两个输入建立同一个命名泛型，供签名与契约哈希保留类型关系。
 *
 * ## Value model
 * 仅 T 一项，未声明约束；这里不将相等性算法限制为数字或具体对象类型。
 *
 * ## Invariants and lifecycle
 * 静态泛型表由 equal 条目引用；约束指针为 NULL 且数量为0，不表示未初始化元数据。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:609
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:377
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:93
 */
static const ZrLibGenericParameterDescriptor g_single_t[] = {
/* 保持 equal 两输入类型关联；无约束条目，约束指针NULL/count0。 */
        {"T", "Compared value type.", ZR_NULL, 0U},
};

/**
 * ## Summary
 * 把 throws 的 E 限定到 Error 类型族，供签名消费和回调的类型匹配使用。
 *
 * ## Value model
 * 唯一约束名为 Error；它是解析/解析后类型契约，不是错误对象或当前 VM exception。
 *
 * ## Invariants and lifecycle
 * 字符串与数组随 provider 借用；实际 subtype/TypeId 检查仍在 Throws，不能把名称相等当成匹配完成。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:342
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:81
 * tests/testing/test_assertions.c:85
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1012
 */
static const TZrChar *g_error_constraint[] = {
/* 约束 E 的 Error 类型族；名称约束不等于实际异常匹配。 */
        "Error",
};

/**
 * ## Summary
 * 让 throws 的返回类型 E、Error 约束与编译器传给 callback 的隐藏 TypeId 保持关联。
 *
 * ## Value model
 * 仅 E 一项；约束数组明确含一个 Error。native arity 不由泛型项数直接等同公开参数数。
 *
 * ## Invariants and lifecycle
 * 泛型声明静态只读；无效隐藏 TypeId 在 action 前被拒绝，回调的 bool 不等于宿主调用是否完成。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:609
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:377
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:93
 * tests/testing/test_assertions.c:80
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1012
 */
static const ZrLibGenericParameterDescriptor g_single_e[] = {
/* 声明返回 E 并携带 Error 约束；E 由隐藏 TypeId 驱动，不是第二个公开形参。 */
        {"E", "Expected exception type.",
         g_error_constraint, ZR_ARRAY_COUNT(g_error_constraint)},
};

/**
 * ## Summary
 * 把公开 assert/equal/throws 签名和三条原生 callback 连接为同一提供者表。
 *
 * ## Value model
 * assert 的 native arity 为1..2；equal 为2；throws 为2但公开只列 action。
 * throws 返回 E，另外两项通过 VM 异常和 TLS 诊断交付失败，正常脚本结果为 void。
 *
 * ## Invariants and lifecycle
 * 此表为不可变描述，不执行回调；物化产生原生闭包后才由 dispatcher 调用。
 * function documentation仅保存在descriptor，当前FunctionInfo不复制，结构hash不含；参数/泛型documentation有各自复制路径。
 * callback 代码和所有描述项必须活到最后一次闭包/注册消费，不能只保留 getter 返回的外层地址。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:598
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:597
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:972
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:964
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:203
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:607
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:609
 * zr_vm_library/include/zr_vm_library/native_binding.h:497
 */
static const ZrLibFunctionDescriptor g_testing_functions[] = {
/* 条件断言的脚本签名与 C callback 绑定；native 1..2参数；成功void，失败经VM异常和TLS。 */
        {"assert", 1U, 2U, ZrVmLibTesting_Assert, "void",
         "Raise AssertionFailure when condition is false.",
         g_assert_parameters, ZR_ARRAY_COUNT(g_assert_parameters), ZR_NULL, 0U, 0U, 0U},
/* 规范相等比较入口及 T 输入元数据；native 2参数；formatter不决定相等性。 */
        {"equal", 2U, 2U, ZrVmLibTesting_Equal, "void",
         "Compare values through canonical equality.",
         g_equal_parameters, ZR_ARRAY_COUNT(g_equal_parameters),
         g_single_t, ZR_ARRAY_COUNT(g_single_t), 0U, 0U},
/* 同步捕获 action 并返回匹配 E 的异常；public 1/native 2含隐藏TypeId；只验证同步action。 */
        {"throws", 2U, 2U, ZrVmLibTesting_Throws, "E",
         "Return the exception thrown by a synchronous action.",
         g_throws_parameters, ZR_ARRAY_COUNT(g_throws_parameters),
         g_single_e, ZR_ARRAY_COUNT(g_single_e), 0U, 0U},
};

/**
 * ## Summary
 * 给 Skip 元数据声明一个编译期原因字段，与 parser 内建 skip schema 保持同义。
 *
 * ## Value model
 * reason 为非空 string；非空限制由编译期测试角色绑定执行，不由普通对象 setter 保证。
 *
 * ## Invariants and lifecycle
 * 这是保留到测试清单的跳过原因，不是 AssertionFailure 的宿主 TLS 字段。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1716
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1224
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:809
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:151
 * zr_vm_parser/src/zr_vm_parser/attribute_contract.c:102
 * zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c:732
 */
static const ZrLibFieldDescriptor g_skip_fields[] = {
/* Skip.reason：保留到测试清单的编译期跳过原因；非空 string 由角色绑定校验，普通setter不校验。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("reason", "string", "Non-empty compile-time skip reason."),
};

/**
 * ## Summary
 * 声明结构化失败的脚本字段，使位置、消息和不同断言类别的诊断可被工具按 schema 消费。
 *
 * ## Value model
 * assertionKind 选择分类；expected/actual/exception 适用场景不同，未生产部分来自清零的 TLS 副本。
 *
 * ## Invariants and lifecycle
 * BUG: exception 声明为 ValueSnapshot，但 AssertionFailure 继承 Error；投影后 Normalize 的已有 Error 分支把同名字段覆为 self。
 * 对象 setter 未按 descriptor 字段类型验证，脚本读取得到自身，宿主 C TLS exception 仍为快照。
 * TODO: 沿投影、Normalize 和 own member 读取补 schema 回归，核查 Error 的规范化协议。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1224
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:809
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:151
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:701
 * zr_vm_core/src/zr_vm_core/exception/exception.c:407
 * zr_vm_core/src/zr_vm_core/exception/exception.c:684
 * zr_vm_core/src/zr_vm_core/object/object.c:2393
 */
static const ZrLibFieldDescriptor g_assertion_failure_fields[] = {
/* AssertionFailure.assertionKind：选择 ASSERT/EQUAL/THROWS 的失败分类；与其他诊断字段适用场景关联。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("assertionKind", "int", "Canonical assertion kind."),
/* AssertionFailure.sourceSpan：保存断言调用位置的结构化投影；位置缺失可全零，也可只有文件名。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceSpan", "SourceSpan", "Call-site source span when available."),
/* AssertionFailure.message：保存有界失败消息；字符数组副本，不借用原VM字符串；UTF8字节裁剪BUG见copy helper。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("message", "string", "Bounded failure message."),
/* AssertionFailure.expected：保存 Equal预期值或Throws预期类型视图；expected-only不必存在实际值。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("expected", "ValueSnapshot", "Bounded expected snapshot."),
/* AssertionFailure.actual：保存 Equal实际值诊断；Assert/Throws 不生产这一输入快照，来自清零记录。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("actual", "ValueSnapshot", "Bounded actual snapshot."),
/* AssertionFailure.exception：声明 Throws错型时捕获异常快照；BUG:脚本字段经Error Normalize覆为self，C TLS仍为快照；TODO补schema回归。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("exception", "ValueSnapshot", "Bounded exception snapshot."),
};

/**
 * ## Summary
 * 把最近 VM 调用帧的源码范围投影成脚本 SourceSpan，避免依赖宿主文本栈信息。
 *
 * ## Value model
 * sourceFile 可独立非空；位置缺失时坐标保持0，非零坐标来自执行位置表。
 *
 * ## Invariants and lifecycle
 * 这些字段是有界值副本，不借用 sourceCodeList。TODO: endColumn 端点是否包含仍从 instructionRanges 生产链核实；现有测试未断言列端点。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1716
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1224
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:809
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:151
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:429
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:436
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:616
 * zr_vm_core/src/zr_vm_core/io/io_runtime.c:695
 * tests/testing/test_assertions.c:503
 */
static const ZrLibFieldDescriptor g_source_span_fields[] = {
/* SourceSpan.sourceFile：保存调用源文件名；有界复制；没有匹配位置时仍可能非空。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceFile", "string", "Source file name."),
/* SourceSpan.startLine：投影执行位置表的起始行；非零行按源坐标；缺位置为0，不与文件名空值等价。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("startLine", "int", "One-based start line."),
/* SourceSpan.startColumn：投影执行位置表的起始列；不是文本buffer字节偏移；缺位置为0。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("startColumn", "int", "One-based start column."),
/* SourceSpan.endLine：投影执行位置表的终止行；不可由这一行单独判断列端点包含关系。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("endLine", "int", "One-based end line."),
/* SourceSpan.endColumn：投影执行位置表的终止列；TODO:包含/排除端点未确认，沿instructionRanges生产核查。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("endColumn", "int", "One-based end column."),
};

/**
 * ## Summary
 * 给快照声明诊断名称、文本及三个独立状态，区分无输入、合法 null、裁剪与格式化故障。
 *
 * ## Value model
 * 普通 typeName 是 VM 标签/原型名，Throws expected-only 可只填预期类型名；hasValue 不要求原值非 null。
 * truncated 仅报告普通 text 裁剪；formatterFaulted 在抛错或正常返回无文本时为真。
 *
 * ## Invariants and lifecycle
 * 快照字段是普通投影，不保活原始值，也不能代替 canonical TypeId。
 * TODO: 当前 typeName/hasValue/formatterFaulted 的英文 documentation 与生产语义有偏差；沿快照生产和descriptor直接读取统一文案。
 * 这些field文案目前仅存在静态descriptor，ModuleInfo字段条目不复制它们，结构hash也不含；反射展示需另做发布与验证。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1716
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1224
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:809
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:151
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:304
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:362
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:370
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1026
 */
static const ZrLibFieldDescriptor g_value_snapshot_fields[] = {
/* ValueSnapshot.typeName：保存VM诊断标签/对象原型名或Throws预期类型名；TODO:英文canonical文案过强，快照不承诺canonical TypeId，沿快照生产和descriptor文案核对；若反射需要此文案须另加发布。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("typeName", "string", "Canonical runtime type name."),
/* ValueSnapshot.text：保存普通调试文本、回退文本或Throws错型的预期类型文本；有界复制；不是原值和原对象保活凭据。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("text", "string", "Bounded debug text."),
/* ValueSnapshot.hasValue：表示视图有生产内容；TODO:英文捕获值过窄；合法null与Throws expected-only也为真。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("hasValue", "bool", "Whether a value was captured."),
/* ValueSnapshot.truncated：报告普通snapshot.text被有界裁剪；不报告typeName、message或expected类型文本的裁剪。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("truncated", "bool", "Whether debug text was truncated."),
/* ValueSnapshot.formatterFaulted：表示格式化抛错或正常返回无文本；TODO:英文raised漏Fine+NULL，统一文案并补正常无文本分支测试。 */
        ZR_LIB_FIELD_DESCRIPTOR_INIT("formatterFaulted", "bool", "Whether value formatting raised."),
};

/**
 * ## Summary
 * 并列提供编译期测试属性镜像与运行期断言诊断原型，供模块物化和契约核查。
 *
 * ## Value model
 * Test/Case/Skip 是属性 schema；SourceSpan/ValueSnapshot 是值投影；AssertionFailure 为 Error 派生 class。
 *
 * ## Invariants and lifecycle
 * 类型表静态借用；原型及字段在消费 VM 内物化，描述表本身不是 VM 对象或 GC root。
 * 当前TypeInfo及field entry不复制type/field documentation，结构hash不含文案；不能把它们写成已发布的反射帮助。
 * parser 测试角色绑定实际读自身 builtin schema；此处元数据与它交叉核对，不能冒充编译器的角色来源。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:619
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:618
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:948
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:982
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:219
 * tests/testing/test_assertions.c:88
 * zr_vm_parser/src/zr_vm_parser/attribute_contract.c:69
 * zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c:506
 */
static const ZrLibTypeDescriptor g_testing_types[] = {
/**
 * ## Summary
 * 表示一个普通测试函数的属性schema镜像
 *
 * ## Fields
 * 没有实例字段；角色要求function target且不可重复。
 *
 * ## Invariants and lifecycle
 * 编译绑定由parser builtin实施；provider type供反射/契约核查和物化，不能把VM对象构造当测试注册。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/attribute_contract.c:69
 * tests/testing/test_assertions.c:88
 * zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c:506
 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Test", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Metadata schema for an ordinary test function.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
/**
 * ## Summary
 * 表示可重复的编译期测试实参元数据schema镜像
 *
 * ## Fields
 * 没有此descriptor定义的实例字段；常量参数实际保存在TestManifest。
 *
 * ## Invariants and lifecycle
 * function target/artifact retention，可重复；参数绑定和序列化由parser/test contract，不由字段数组完成。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/attribute_contract.c:86
 * zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c:720
 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Case", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Metadata schema for compile-time bound test arguments.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
/**
 * ## Summary
 * 表示保留到清单的跳过原因schema镜像
 *
 * ## Fields
 * 一个reason字符串字段；非空要求由编译期角色绑定实施。
 *
 * ## Invariants and lifecycle
 * function target/artifact retention，不可重复；不是运行时抛出的AssertionFailure。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/attribute_contract.c:102
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:207
 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Skip", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    g_skip_fields, ZR_ARRAY_COUNT(g_skip_fields),
                                    ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Metadata schema for a retained skipped test.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
/**
 * ## Summary
 * 承载断言调用位置的脚本值投影，与宿主C SourceSpan 同源。
 *
 * ## Fields
 * sourceFile与四个坐标；位置缺失不要求文件名也空。
 *
 * ## Invariants and lifecycle
 * 有界值副本，不借用VM帧。TODO: endColumn端点语义待沿执行位置生产者确认，现有测试只检查行范围。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:616
 * tests/testing/test_assertions.c:503
 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("SourceSpan", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    g_source_span_fields, ZR_ARRAY_COUNT(g_source_span_fields),
                                    ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Structured assertion call-site span.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
/**
 * ## Summary
 * 承载值或预期类型的诊断投影，使formatter故障与原断言结果可分别观察。
 *
 * ## Fields
 * typeName/text/hasValue/truncated/formatterFaulted，三布尔表示不同事实。
 *
 * ## Invariants and lifecycle
 * 不保活原始VM对象，不承诺canonical identity；typeName和expected-only不走普通text裁剪标志。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:648
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:362
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1026
 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("ValueSnapshot", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                    g_value_snapshot_fields, ZR_ARRAY_COUNT(g_value_snapshot_fields),
                                    ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Bounded assertion value snapshot.",
                                    ZR_NULL, ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
/**
 * ## Summary
 * 以Error派生异常携带断言分类、位置、消息和诊断视图。
 *
 * ## Fields
 * 六个声明字段；未生产部分来自清零记录，不表示每类断言都有actual或exception。
 *
 * ## Invariants and lifecycle
 * BUG:继承Error使Normalize把exception投影覆为自身，违反此处ValueSnapshot schema；C TLS仍保存快照。
 * TODO:联查Error规范化协议与脚本own member读取并补回归。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:701
 * zr_vm_core/src/zr_vm_core/exception/exception.c:407
 * zr_vm_core/src/zr_vm_core/exception/exception.c:684
 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("AssertionFailure", ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                    g_assertion_failure_fields,
                                    ZR_ARRAY_COUNT(g_assertion_failure_fields),
                                    ZR_NULL, 0U, ZR_NULL, 0U,
                                    "Structured assertion exception.",
                                    "Error", ZR_NULL, 0U, ZR_NULL, 0U, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0U),
};

/**
 * ## Summary
 * 镜像 parser 的 test/case/skip 稳定角色，让签名/官方提供者检查发现提供者与编译契约分叉。
 *
 * ## Value model
 * 三项均为 FUNCTION target 和 ARTIFACT retention；仅 case 可重复，关联 Test/Case/Skip schema。
 *
 * ## Invariants and lifecycle
 * 编译器实际从 parser builtin schema 绑定角色和清单；这张 provider 表参与契约哈希和直接交叉测试。
 * 不能仅因两张表值相同，就声称 compiler 读取本数组。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:222
 * tests/testing/test_assertions.c:50
 * tests/testing/test_assertions.c:88
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:220
 * tests/testing/test_assertions.c:90
 * zr_vm_parser/src/zr_vm_parser/attribute_contract.c:69
 * zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c:506
 * zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c:716
 */
static const ZrLibAttributeRoleDescriptor g_testing_roles[] = {
/* 普通顶层测试根；FUNCTION/ARTIFACT，不可重复，关联Test。稳定ID与parser builtin交叉核对；compiler实际读parser表。 */
        {"zr.testing.test", 0xdf51f287U, ZR_PARSER_ATTRIBUTE_ROLE_TEST,
         ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
         ZR_FALSE, "Test"},
/* 常量实参测试case；FUNCTION/ARTIFACT，可重复，关联Case。稳定ID与parser builtin交叉核对；compiler实际读parser表。 */
        {"zr.testing.case", 0xbe2aca8fU, ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE,
         ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
         ZR_TRUE, "Case"},
/* 编译期跳过标记；FUNCTION/ARTIFACT，不可重复，关联Skip。稳定ID与parser builtin交叉核对；compiler实际读parser表。 */
        {"zr.testing.skip", 0x91a67a48U, ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP,
         ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
         ZR_FALSE, "Skip"},
};

/**
 * ## Summary
 * 在 zr.testing 物化时公开 task 模块链接，让异步测试类型通过规范 zr.task 访问。
 *
 * ## Value model
 * 公开名称 task 指向 zr.task；链接目标为注册表解析出的整个 module export，不是一个 Task 值。
 *
 * ## Invariants and lifecycle
 * 宿主 Register 先登记 task provider；插件 getter 只提供描述符，插件宿主仍须让解析器找到 zr.task。
 * 解析/物化目标失败可使本模块链接建立失败；不能把静态数组存在等同依赖已可用。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:983
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:991
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1142
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:214
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1153
 * zr_vm_library/src/zr_vm_library/task/task_runtime.c:1393
 */
static const ZrLibModuleLinkDescriptor g_testing_links[] = {
/* task 别名解析 zr.task 的完整module export；宿主须先让规范Task provider可解析，不创建Task实例。 */
        {"task", "zr.task", "Async tests return the canonical Task<void>."},
};

/**
 * ## Summary
 * 汇集 Test provider 的 ABI、公开符号、类型和角色声明，让直接注册和动态查询共享同一身份。
 *
 * ## Value model
 * 15个显式成员见下；未指定的常量/提示/capability/materialize hook 等按静态初始化为0或NULL。
 * publicContractHash 是版本化声明标识，实际结构签名另由 ComputeModuleSignatureHash 计算。
 *
 * ## Invariants and lifecycle
 * 本表和嵌套表/字符串由 provider 持有，注册记录和闭包借用；卸载前须结束这些消费。
 * TEST 是消费/物化 phase 限制；读取本表或 Register 不自动把 host 切到 TEST。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:594
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:839
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:939
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:192
 * zr_vm_library/include/zr_vm_library/native_binding.h:496
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:738
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c:188
 * tests/testing/test_assertions.c:59
 */
static const ZrLibModuleDescriptor g_testing_descriptor = {
/* 固定插件descriptor布局ABI；不是模块业务版本。 */
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
/* 注册/加载按zr.testing身份核对并建立module名。 */
        .moduleName = "zr.testing",
/* 借用三项公开callback及签名表。 */
        .functions = g_testing_functions,
/* 和functions数组同源，控制物化/哈希遍历。 */
        .functionCount = ZR_ARRAY_COUNT(g_testing_functions),
/* 借用属性schema和失败诊断类型表。 */
        .types = g_testing_types,
/* 和types数组同源，控制类型物化遍历。 */
        .typeCount = ZR_ARRAY_COUNT(g_testing_types),
/* TODO:provider用途文案未在ModuleInfo/hash路径读取；核查外部直接descriptor消费者及发布目的。 */
        .documentation = "Test-phase metadata and assertion provider.",
/* 借用task到zr.task的解析链接。 */
        .moduleLinks = g_testing_links,
/* 和moduleLinks数组同源，控制链接遍历。 */
        .moduleLinkCount = ZR_ARRAY_COUNT(g_testing_links),
/* 声明模块版本，参与结构签名和ModuleInfo。 */
        .moduleVersion = "1.0.0",
/* 0采用当前runtime ABI默认值；非0声明最低运行ABI，高于runtime拒绝；不与插件布局ABI混用。 */
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
/* TEST消费门禁，不在注册或getter中切换宿主phase。 */
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_TEST,
/* 借用parser角色镜像，供hash和直接一致性核查。 */
        .attributeRoles = g_testing_roles,
/* 和attributeRoles数组同源，控制角色hash遍历。 */
        .attributeRoleCount = ZR_ARRAY_COUNT(g_testing_roles),
/* 版本化声明标识；完整结构签名另计算，不是此字符串的源码hash。 */
        .publicContractHash = "zr.testing:v2:typed-manifest-structured-assertions",
};

/**
 * ## Summary
 * 向本模块公开getter交付同一静态描述表，使直接注册与插件查询共享身份。
 *
 * ## Returns
 * @return 非NULL的const借用指针；外层descriptor及其嵌套表/字符串无需调用方释放。
 *
 * ## Constraints and lifecycle
 * 本查询不读取VM状态、不注册依赖、不物化原型，也不切换provider phase。
 * provider映像须覆盖所有注册记录和闭包的消费期，卸载前结束借用。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:23 是唯一仓内直接转发；zr_vm_library/include/zr_vm_library/native_binding.h:497 声明provider借用寿命。
 */
const ZrLibModuleDescriptor *ZrVmLibTesting_Runtime_GetModuleDescriptor(void) {
    return &g_testing_descriptor;
}
