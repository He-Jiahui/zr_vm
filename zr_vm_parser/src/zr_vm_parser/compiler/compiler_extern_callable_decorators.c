#include "compiler_internal.h"
#include "compiler_extern_decorator_diagnostics.h"

/* leafName 锚定 canonical 叶名，requireCall 再限定节点必须是 zr.ffi.<leaf>(...) 调用。 */
typedef struct SZrExternCallableDecoratorRule {
    const TZrChar *leafName;
    TZrBool requireCall;
} SZrExternCallableDecoratorRule;

/* extern function 可声明完整调用与平台策略，并为导入项指定入口符号和 capability。 */
static const SZrExternCallableDecoratorRule kExternFunctionDecoratorRules[] = {
        {"entry", ZR_TRUE},
        {"callingConvention", ZR_TRUE},
        {"callconv", ZR_TRUE},
        {"charset", ZR_TRUE},
        {"errorPolicy", ZR_TRUE},
        {"cleanup", ZR_TRUE},
        {"callbackLifetime", ZR_TRUE},
        {"callbackThread", ZR_TRUE},
        {"callbackException", ZR_TRUE},
        {"platform", ZR_TRUE},
        {"requiredCapabilities", ZR_TRUE},
};

/* delegate 描述回调签名；当前只接受 ABI 和字符集两类调用约定元数据。 */
static const SZrExternCallableDecoratorRule kExternDelegateDecoratorRules[] = {
        {"callingConvention", ZR_TRUE},
        {"callconv", ZR_TRUE},
        {"charset", ZR_TRUE},
};

/* 这些静态白名单与后续 FFI contract 字段映射使用的策略词汇保持一致。 */
static const TZrChar *const kExternCallableAbiValues[] = {
        "c", "cdecl", "stdcall", "system"};
static const TZrChar *const kExternCallableCharsetValues[] = {
        "utf8", "utf16", "ansi"};
static const TZrChar *const kExternCallableErrorPolicyValues[] = {
        "returnCode", "lastError", "errno", "throws"};
static const TZrChar *const kExternCallableCleanupValues[] = {
        "caller", "callee", "registered"};
static const TZrChar *const kExternCallableLifetimeValues[] = {
        "call", "scoped", "static"};
static const TZrChar *const kExternCallableThreadValues[] = {
        "caller", "attach", "forbidden"};
static const TZrChar *const kExternCallableExceptionValues[] = {
        "abort", "returnDefault", "errorResult"};
static const TZrChar *const kExternCallablePlatformValues[] = {
        "any", "windows", "unix"};

/**
 * @brief 在固定白名单中比较 AST 字符串的 native view。
 * @pre value 从当前 AST 借用，allowedValues 指向静态表；两者仅在本次调用中读取。
 * @return 字符串存在且命中白名单时返回 true，否则返回 false。
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
    for (TZrSize index = 0; index < allowedValueCount; index++) {
        if (allowedValues[index] != ZR_NULL &&
            strcmp(text, allowedValues[index]) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 校验单个 callable 装饰器的参数形状及当前支持的字面量集合。
 * @pre call 由 extern_compiler_match_decorator_path 从同一 decorator AST 借出；不负责释放 AST。
 * @return 参数符合规则时返回 true；失败只返回 false，由上层统一发布诊断。
 */
static TZrBool compiler_extern_callable_arguments_valid(
        const TZrChar *leafName,
        SZrFunctionCall *call) {
    SZrString *stringValue = ZR_NULL;
    TZrInt64 integerValue = 0;

    if (leafName == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(leafName, "requiredCapabilities") == 0) {
        /* TODO: 对照编译期后置 contract 构建与 LSP 直接校验。本入口只拒绝负数；
         * compiler_extern_declaration.c:669 的 builder 要求 FFI_RUNTIME 位，而
         * semantic_analyzer_typecheck.c:1108 只调用本入口；补 requiredCapabilities(0) 对照。 */
        return extern_compiler_extract_int_argument(call, &integerValue) &&
               integerValue >= 0;
    }
    if (!extern_compiler_extract_string_argument(call, &stringValue)) {
        return ZR_FALSE;
    }
    if (strcmp(leafName, "entry") == 0) {
        /* entry 是导入符号名，不是封闭策略枚举；前面的提取仍要求单个字符串 literal。 */
        return ZR_TRUE;
    }
    if (strcmp(leafName, "callingConvention") == 0 ||
        strcmp(leafName, "callconv") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableAbiValues,
                ZR_ARRAY_COUNT(kExternCallableAbiValues));
    }
    if (strcmp(leafName, "charset") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableCharsetValues,
                ZR_ARRAY_COUNT(kExternCallableCharsetValues));
    }
    if (strcmp(leafName, "errorPolicy") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableErrorPolicyValues,
                ZR_ARRAY_COUNT(kExternCallableErrorPolicyValues));
    }
    if (strcmp(leafName, "cleanup") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableCleanupValues,
                ZR_ARRAY_COUNT(kExternCallableCleanupValues));
    }
    if (strcmp(leafName, "callbackLifetime") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableLifetimeValues,
                ZR_ARRAY_COUNT(kExternCallableLifetimeValues));
    }
    if (strcmp(leafName, "callbackThread") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableThreadValues,
                ZR_ARRAY_COUNT(kExternCallableThreadValues));
    }
    if (strcmp(leafName, "callbackException") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallableExceptionValues,
                ZR_ARRAY_COUNT(kExternCallableExceptionValues));
    }
    if (strcmp(leafName, "platform") == 0) {
        return compiler_extern_string_in_set(
                stringValue,
                kExternCallablePlatformValues,
                ZR_ARRAY_COUNT(kExternCallablePlatformValues));
    }
    return ZR_FALSE;
}

/**
 * @brief 按目标 callable 的规则表验证全部装饰器，并将首个失败节点交给共享诊断出口。
 * @pre cs 为可写 compiler state；rules 是与 declaration 类型匹配的静态规则表。
 * @return 全部装饰器合法时返回 true；非法节点或内部输入无效时返回 false。
 * @note decorators、rules 和匹配到的 call 都是借用对象；报告函数接管诊断载荷而不接管 AST。
 */
static TZrBool compiler_extern_validate_callable_rules(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        const SZrExternCallableDecoratorRule *rules,
        TZrSize ruleCount) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize decoratorIndex = 0;
         decoratorIndex < decorators->count;
         decoratorIndex++) {
        SZrAstNode *decoratorNode = decorators->nodes[decoratorIndex];
        const SZrExternCallableDecoratorRule *matchedRule = ZR_NULL;
        SZrFunctionCall *call = ZR_NULL;

        if (decoratorNode == ZR_NULL) {
            continue;
        }
        /* 匹配器返回的 call 指向当前 decorator AST，matchedRule 指向静态规则表，二者均不转移所有权。 */
        for (TZrSize ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
            SZrFunctionCall *candidateCall = ZR_NULL;

            if (extern_compiler_match_decorator_path(
                        decoratorNode,
                        rules[ruleIndex].leafName,
                        rules[ruleIndex].requireCall,
                        &candidateCall)) {
                matchedRule = &rules[ruleIndex];
                call = candidateCall;
                break;
            }
        }
        if (matchedRule == ZR_NULL) {
            return compiler_extern_report_invalid_decorator(
                    cs,
                    decoratorNode,
                    "Decorator is not valid on extern callable declarations",
                    "The decorator is not a valid canonical zr.ffi directive for this extern callable declaration.",
                    "Use a supported zr.ffi callable decorator with the required argument shape and value.");
        }
        /* message 是栈缓冲区；reporter 在本次调用中构造并交接诊断载荷后立即返回。 */
        if (!compiler_extern_callable_arguments_valid(
                    matchedRule->leafName, call)) {
            snprintf(message,
                     sizeof(message),
                     "zr.ffi.%s has invalid arguments for this extern callable declaration",
                     matchedRule->leafName);
            return compiler_extern_report_invalid_decorator(
                    cs,
                    decoratorNode,
                    message,
                    "The decorator is not a valid canonical zr.ffi directive for this extern callable declaration.",
                    "Use a supported zr.ffi callable decorator with the required argument shape and value.");
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 为 extern function/delegate 提供 parser 与 LSP 共用的 callable 装饰器校验入口。
 * @pre declaration 必须是 ZR_AST_EXTERN_FUNCTION_DECLARATION 或 ZR_AST_EXTERN_DELEGATE_DECLARATION。
 * @return 声明类型正确且所有 callable 装饰器合法时返回 true；非法输入或诊断失败返回 false。
 * @note 编译路径还会由 FFI contract builder 校验策略组合；LSP 直调本入口，capability 位一致性留有 TODO。
 */
TZrBool ZrParser_Compiler_ValidateExternCallableDecorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    if (cs == ZR_NULL || declaration == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (declaration->type) {
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return compiler_extern_validate_callable_rules(
                    cs,
                    declaration->data.externFunctionDeclaration.decorators,
                    kExternFunctionDecoratorRules,
                    ZR_ARRAY_COUNT(kExternFunctionDecoratorRules));
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return compiler_extern_validate_callable_rules(
                    cs,
                    declaration->data.externDelegateDeclaration.decorators,
                    kExternDelegateDecoratorRules,
                    ZR_ARRAY_COUNT(kExternDelegateDecoratorRules));
        default:
            return ZR_FALSE;
    }
}
