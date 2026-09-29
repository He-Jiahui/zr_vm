#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"

/**
 * @brief 区分 extern struct 声明级与字段级指令的校验域。
 * @note 声明级控制 aggregate kind/layout；字段级只影响单字段布局或字符串 ABI。
 */
typedef enum EZrExternStructDecoratorTarget {
    ZR_EXTERN_STRUCT_DECORATOR_DECLARATION = 0,
    ZR_EXTERN_STRUCT_DECORATOR_FIELD = 1
} EZrExternStructDecoratorTarget;

/** @brief 将规范指令叶名称绑定到唯一允许出现的 AST 目标层级。 */
typedef struct SZrExternStructDecoratorRule {
    const TZrChar *leafName;
    EZrExternStructDecoratorTarget target;
} SZrExternStructDecoratorRule;

/**
 * @brief 指定 struct 及字段两层可识别的 zr.ffi 指令白名单。
 * @note 未列出的路径由统一诊断拒绝，避免编译器、LSP 和 FFI 消费端各自猜测。
 */
static const SZrExternStructDecoratorRule kExternStructDecoratorRules[] = {
        {"kind", ZR_EXTERN_STRUCT_DECORATOR_DECLARATION},
        {"pack", ZR_EXTERN_STRUCT_DECORATOR_DECLARATION},
        {"align", ZR_EXTERN_STRUCT_DECORATOR_DECLARATION},
        {"offset", ZR_EXTERN_STRUCT_DECORATOR_FIELD},
        {"charset", ZR_EXTERN_STRUCT_DECORATOR_FIELD},
};

/** @brief aggregate kind 仅接受当前 descriptor/布局消费者理解的拼写。 */
static const TZrChar *const kExternStructKindValues[] = {
        "struct", "union"};
/** @brief 字段 charset 的当前白名单；扩展需同步核对字符串 ABI 消费端。 */
static const TZrChar *const kExternStructCharsetValues[] = {
        "utf8", "utf16", "ansi"};

/**
 * @brief 按精确 native 文本匹配一项字面值白名单。
 * @pre value 与候选表来自仍有效的同一 AST/编译状态。
 * @return 仅完整匹配返回 true；缺失文本不会退化为默认值。
 */
static TZrBool compiler_extern_string_in_set(
        SZrString *value,
        const TZrChar *const *allowedValues,
        TZrSize allowedValueCount) {
    const TZrChar *text;

    if (value == ZR_NULL || allowedValues == ZR_NULL) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < allowedValueCount; index++) {
        if (allowedValues[index] != ZR_NULL &&
            strcmp(text, allowedValues[index]) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 检查可写入 32 位 FFI 布局字段的正二次幂参数。
 * @note pack/align 的共享布局消费者按位运算要求二次幂且使用 u32 范围。
 */
static TZrBool compiler_extern_struct_integer_is_power_of_two(TZrInt64 value) {
    TZrUInt64 unsignedValue;

    if (value <= 0 || (TZrUInt64)value > UINT32_MAX) {
        return ZR_FALSE;
    }
    unsignedValue = (TZrUInt64)value;
    return (unsignedValue & (unsignedValue - 1U)) == 0U;
}

/**
 * @brief 将白名单指令限制为单个正确类别的 AST 字面量及可消费取值。
 * @pre rule 已由当前 target 的白名单选出；call 必须是该指令的调用节点。
 * @return 这里只决定参数契约，不发布诊断；失败由外层定位到完整 decorator。
 */
static TZrBool compiler_extern_struct_arguments_valid(
        const SZrExternStructDecoratorRule *rule,
        SZrFunctionCall *call) {
    SZrString *stringValue = ZR_NULL;
    TZrInt64 integerValue = 0;

    if (rule == ZR_NULL || rule->leafName == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }
    /* kind 只接受聚合类别名，不能借用字段级字符串语义。 */
    if (strcmp(rule->leafName, "kind") == 0) {
        return extern_compiler_extract_string_argument(call, &stringValue) &&
               compiler_extern_string_in_set(
                       stringValue,
                       kExternStructKindValues,
                       ZR_ARRAY_COUNT(kExternStructKindValues));
    }
    /* pack 与 align 共用受限整数域，供布局算法安全执行二次幂运算。 */
    if (strcmp(rule->leafName, "pack") == 0 ||
        strcmp(rule->leafName, "align") == 0) {
        return extern_compiler_extract_int_argument(call, &integerValue) &&
               compiler_extern_struct_integer_is_power_of_two(integerValue);
    }
    /* offset 允许零，但写入 u32 前拒绝负值和不可表示值。 */
    if (strcmp(rule->leafName, "offset") == 0) {
        return extern_compiler_extract_int_argument(call, &integerValue) &&
               integerValue >= 0 && (TZrUInt64)integerValue <= UINT32_MAX;
    }
    /* charset 是字段文本 ABI 提示，仅接受 descriptor 消费端已知的编码。 */
    if (strcmp(rule->leafName, "charset") == 0) {
        return extern_compiler_extract_string_argument(call, &stringValue) &&
               compiler_extern_string_in_set(
                       stringValue,
                       kExternStructCharsetValues,
                       ZR_ARRAY_COUNT(kExternStructCharsetValues));
    }
    return ZR_FALSE;
}

/**
 * @brief 统一验证一组声明级或字段级指令，并把失败映射为 parser 结构化诊断。
 * @pre 调用方按 AST 所属层级选择 target；外部编译与 LSP 查询共用此判定。
 * @return 无装饰器合法；首个未知/错层/错参指令令查询失败并保留原 decorator 范围。
 * @note TODO: 同一叶名称的多个合法指令目前均通过；下游 helper 只消费首个匹配项，
 * 尚未定义重复 pack/align/offset/charset/kind 应拒绝还是按次序覆盖。
 */
static TZrBool compiler_extern_validate_struct_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        EZrExternStructDecoratorTarget target) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *targetText = target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                                       ? "extern struct fields"
                                       : "extern struct declarations";
    const TZrChar *cause = target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                                   ? "The decorator is not a valid canonical zr.ffi directive for this extern struct field."
                                   : "The decorator is not a valid canonical zr.ffi directive for this extern struct declaration.";
    const TZrChar *suggestion = target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                                        ? "Use zr.ffi.offset with a nonnegative integer or zr.ffi.charset with a supported charset."
                                        : "Use zr.ffi.kind, zr.ffi.pack, or zr.ffi.align with a supported canonical value.";

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }
    /* 每个 decorator 独立归类；首个失败即停止，保留最接近根因的诊断范围。 */
    for (TZrSize decoratorIndex = 0U;
         decoratorIndex < decorators->count;
         decoratorIndex++) {
        SZrAstNode *decoratorNode = decorators->nodes[decoratorIndex];
        const SZrExternStructDecoratorRule *matchedRule = ZR_NULL;
        SZrFunctionCall *call = ZR_NULL;

        if (decoratorNode == ZR_NULL) {
            continue;
        }
        /* 只有当前层级的已知指令才参与参数验证，其他装饰器统一走可定位诊断。 */
        for (TZrSize ruleIndex = 0U;
             ruleIndex < ZR_ARRAY_COUNT(kExternStructDecoratorRules);
             ruleIndex++) {
            SZrFunctionCall *candidateCall = ZR_NULL;
            const SZrExternStructDecoratorRule *rule =
                    &kExternStructDecoratorRules[ruleIndex];

            if (rule->target == target &&
                extern_compiler_match_decorator_path(
                        decoratorNode,
                        rule->leafName,
                        ZR_TRUE,
                        &candidateCall)) {
                matchedRule = rule;
                call = candidateCall;
                break;
            }
        }
        /* 失败在 decorator 节点处报告，保持 parser query 与 LSP 的范围一致。 */
        if (matchedRule == ZR_NULL) {
            snprintf(
                    message,
                    sizeof(message),
                    "Decorator is not valid on %s",
                    targetText);
            return compiler_extern_report_invalid_decorator(
                    cs, decoratorNode, message, cause, suggestion);
        }
        if (!compiler_extern_struct_arguments_valid(matchedRule, call)) {
            snprintf(
                    message,
                    sizeof(message),
                    "zr.ffi.%s has invalid arguments for this %s",
                    matchedRule->leafName,
                    target == ZR_EXTERN_STRUCT_DECORATOR_FIELD
                            ? "extern struct field"
                            : "extern struct declaration");
            return compiler_extern_report_invalid_decorator(
                    cs, decoratorNode, message, cause, suggestion);
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 对一个 extern struct AST 同时校验声明指令和所有字段指令。
 * @pre declaration 必须是 struct 声明；生产入口仅从 extern block 进入。
 * @return 输入类型不符或任一诊断校验失败时返回 false；无错时供编译/LSP 继续处理。
 * @note 此处只验证注解契约；布局与 descriptor 构造仍由后续 FFI 阶段完成。
 */
TZrBool ZrParser_Compiler_ValidateExternStructDecorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    SZrStructDeclaration *structure;

    if (cs == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_STRUCT_DECLARATION) {
        return ZR_FALSE;
    }
    structure = &declaration->data.structDeclaration;
    /* 先拒绝声明级错误，避免继续解释其下属字段的布局元数据。 */
    if (!compiler_extern_validate_struct_decorator_array(
                cs,
                structure->decorators,
                ZR_EXTERN_STRUCT_DECORATOR_DECLARATION)) {
        return ZR_FALSE;
    }
    if (structure->members == ZR_NULL) {
        return ZR_TRUE;
    }
    /* 仅结构字段拥有字段级规则；其他 AST 成员不参与这组 FFI 属性校验。 */
    for (TZrSize memberIndex = 0U;
         memberIndex < structure->members->count;
         memberIndex++) {
        SZrAstNode *member = structure->members->nodes[memberIndex];

        if (member != ZR_NULL && member->type == ZR_AST_STRUCT_FIELD &&
            !compiler_extern_validate_struct_decorator_array(
                    cs,
                    member->data.structField.decorators,
                    ZR_EXTERN_STRUCT_DECORATOR_FIELD)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
