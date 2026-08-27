#include "compiler_ffi_wrapper_decorators.h"

#include "compiler_extern_decorator_diagnostics.h"

static const TZrChar *const kFfiWrapperLeafNames[] = {
        "lowering", "viewType", "underlying", "ownerMode", "releaseHook"};

static const TZrChar *compiler_ffi_wrapper_identifier_text(
        SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(node->data.identifier.name);
}

static const TZrChar *compiler_ffi_wrapper_member_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_NULL;
    }
    return compiler_ffi_wrapper_identifier_text(
            node->data.memberExpression.property);
}

static TZrBool compiler_ffi_wrapper_text_equals(
        const TZrChar *left,
        const TZrChar *right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static TZrBool compiler_ffi_wrapper_extract(
        SZrAstNode *decoratorNode,
        TZrBool *outIsFfi,
        const TZrChar **outLeafName,
        SZrFunctionCall **outCall) {
    SZrAstNode *expression;
    SZrPrimaryExpression *primary;

    if (outIsFfi != ZR_NULL) {
        *outIsFfi = ZR_FALSE;
    }
    if (outLeafName != ZR_NULL) {
        *outLeafName = ZR_NULL;
    }
    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (decoratorNode == ZR_NULL ||
        decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }
    expression = decoratorNode->data.decoratorExpression.expr;
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    primary = &expression->data.primaryExpression;
    if (!compiler_ffi_wrapper_text_equals(
                compiler_ffi_wrapper_identifier_text(primary->property),
                "zr") ||
        primary->members == ZR_NULL || primary->members->count == 0U ||
        !compiler_ffi_wrapper_text_equals(
                compiler_ffi_wrapper_member_text(primary->members->nodes[0]),
                "ffi")) {
        return ZR_FALSE;
    }
    if (outIsFfi != ZR_NULL) {
        *outIsFfi = ZR_TRUE;
    }
    if (primary->members->count != 3U) {
        return ZR_FALSE;
    }
    if (outLeafName != ZR_NULL) {
        *outLeafName = compiler_ffi_wrapper_member_text(
                primary->members->nodes[1]);
        if (*outLeafName == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    if (primary->members->nodes[2] == ZR_NULL ||
        primary->members->nodes[2]->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }
    if (outCall != ZR_NULL) {
        *outCall = &primary->members->nodes[2]->data.functionCall;
    }
    return ZR_TRUE;
}

static TZrBool compiler_ffi_wrapper_leaf_is_supported(
        const TZrChar *leafName) {
    for (TZrSize index = 0U;
         index < ZR_ARRAY_COUNT(kFfiWrapperLeafNames);
         index++) {
        if (compiler_ffi_wrapper_text_equals(
                    leafName, kFfiWrapperLeafNames[index])) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_ffi_wrapper_underlying_is_supported(
        SZrString *value) {
    static const TZrChar *const kIntegerNames[] = {
            "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64"};

    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < ZR_ARRAY_COUNT(kIntegerNames); index++) {
        if (extern_compiler_string_equals(value, kIntegerNames[index])) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_ffi_wrapper_view_is_source_extern_struct(
        SZrCompilerState *cs,
        SZrString *typeName) {
    SZrScript *script;

    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }
    script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < script->statements->count; index++) {
        SZrAstNode *statement = script->statements->nodes[index];
        SZrAstNode *candidate;

        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK) {
            continue;
        }
        candidate = extern_compiler_find_named_declaration(
                &statement->data.externBlock, typeName);
        if (candidate != ZR_NULL &&
            candidate->type == ZR_AST_STRUCT_DECLARATION) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_ffi_wrapper_invalid(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        const TZrChar *message,
        const TZrChar *suggestion) {
    return compiler_extern_report_invalid_decorator(
            cs,
            decoratorNode,
            message,
            "The decorator is not a valid canonical zr.ffi directive for this class wrapper.",
            suggestion);
}

static TZrBool compiler_ffi_wrapper_read_string(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        const TZrChar *leafName,
        SZrFunctionCall *call,
        SZrString **outValue) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (extern_compiler_extract_string_argument(call, outValue) &&
        outValue != ZR_NULL && *outValue != ZR_NULL) {
        return ZR_TRUE;
    }
    snprintf(
            message,
            sizeof(message),
            "zr.ffi.%s on class wrappers requires a single string argument",
            leafName != ZR_NULL ? leafName : "decorator");
    return compiler_ffi_wrapper_invalid(
            cs,
            decoratorNode,
            message,
            "Use one supported string literal argument.");
}

TZrBool compiler_ffi_wrapper_bind_decorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration,
        SZrFfiWrapperDecoratorContract *outContract) {
    SZrFfiWrapperDecoratorContract contract;
    SZrClassDeclaration *classDeclaration;
    SZrAstNode *loweringNode = ZR_NULL;
    SZrAstNode *viewTypeNode = ZR_NULL;
    SZrAstNode *underlyingNode = ZR_NULL;

    memset(&contract, 0, sizeof(contract));
    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (cs == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_CLASS_DECLARATION) {
        return ZR_FALSE;
    }
    classDeclaration = &declaration->data.classDeclaration;
    if (classDeclaration->decorators == ZR_NULL) {
        if (outContract != ZR_NULL) {
            *outContract = contract;
        }
        return ZR_TRUE;
    }
    for (TZrSize index = 0U;
         index < classDeclaration->decorators->count;
         index++) {
        SZrAstNode *decoratorNode =
                classDeclaration->decorators->nodes[index];
        SZrFunctionCall *call = ZR_NULL;
        const TZrChar *leafName = ZR_NULL;
        TZrBool isFfi = ZR_FALSE;

        if (!compiler_ffi_wrapper_extract(
                    decoratorNode, &isFfi, &leafName, &call)) {
            if (!isFfi) {
                continue;
            }
            return compiler_ffi_wrapper_invalid(
                    cs,
                    decoratorNode,
                    "zr.ffi class wrapper decorators require a named call with one string argument",
                    "Use lowering, viewType, underlying, ownerMode, or releaseHook with one string literal.");
        }
        if (!compiler_ffi_wrapper_leaf_is_supported(leafName)) {
            TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

            snprintf(
                    message,
                    sizeof(message),
                    "zr.ffi.%s is not valid on class declarations",
                    leafName);
            return compiler_ffi_wrapper_invalid(
                    cs,
                    decoratorNode,
                    message,
                    "Use a supported class wrapper decorator.");
        }
        if (compiler_ffi_wrapper_text_equals(leafName, "lowering")) {
            loweringNode = decoratorNode;
            contract.hasLowering = ZR_TRUE;
            if (!compiler_ffi_wrapper_read_string(
                        cs, decoratorNode, leafName, call, &contract.lowering)) {
                return ZR_FALSE;
            }
            if (!extern_compiler_string_equals(contract.lowering, "value") &&
                !extern_compiler_string_equals(contract.lowering, "pointer") &&
                !extern_compiler_string_equals(contract.lowering, "handle_id")) {
                return compiler_ffi_wrapper_invalid(
                        cs,
                        decoratorNode,
                        "zr.ffi.lowering on class wrappers requires one of: value, pointer, handle_id",
                        "Use value, pointer, or handle_id.");
            }
        } else if (compiler_ffi_wrapper_text_equals(leafName, "viewType")) {
            viewTypeNode = decoratorNode;
            contract.hasViewType = ZR_TRUE;
            if (!compiler_ffi_wrapper_read_string(
                        cs, decoratorNode, leafName, call, &contract.viewType)) {
                return ZR_FALSE;
            }
        } else if (compiler_ffi_wrapper_text_equals(leafName, "underlying")) {
            underlyingNode = decoratorNode;
            contract.hasUnderlying = ZR_TRUE;
            if (!compiler_ffi_wrapper_read_string(
                        cs, decoratorNode, leafName, call, &contract.underlying)) {
                return ZR_FALSE;
            }
            if (!compiler_ffi_wrapper_underlying_is_supported(
                        contract.underlying)) {
                return compiler_ffi_wrapper_invalid(
                        cs,
                        decoratorNode,
                        "zr.ffi.underlying on class wrappers requires a supported integer type name: i8, u8, i16, u16, i32, u32, i64, u64",
                        "Use a supported fixed-width integer type name.");
            }
        } else if (compiler_ffi_wrapper_text_equals(leafName, "ownerMode")) {
            contract.hasOwnerMode = ZR_TRUE;
            if (!compiler_ffi_wrapper_read_string(
                        cs, decoratorNode, leafName, call, &contract.ownerMode)) {
                return ZR_FALSE;
            }
            if (!extern_compiler_string_equals(contract.ownerMode, "borrowed") &&
                !extern_compiler_string_equals(contract.ownerMode, "owned")) {
                return compiler_ffi_wrapper_invalid(
                        cs,
                        decoratorNode,
                        "zr.ffi.ownerMode on class wrappers requires one of: borrowed, owned",
                        "Use borrowed or owned.");
            }
        } else {
            contract.hasReleaseHook = ZR_TRUE;
            if (!compiler_ffi_wrapper_read_string(
                        cs,
                        decoratorNode,
                        leafName,
                        call,
                        &contract.releaseHook)) {
                return ZR_FALSE;
            }
        }
    }

    if (contract.hasUnderlying &&
        (!contract.hasLowering ||
         !extern_compiler_string_equals(contract.lowering, "handle_id"))) {
        return compiler_ffi_wrapper_invalid(
                cs,
                underlyingNode,
                "zr.ffi.underlying on class wrappers requires zr.ffi.lowering(\"handle_id\")",
                "Use underlying only with handle_id lowering.");
    }
    if (contract.hasLowering &&
        extern_compiler_string_equals(contract.lowering, "handle_id") &&
        !contract.hasUnderlying) {
        return compiler_ffi_wrapper_invalid(
                cs,
                loweringNode,
                "zr.ffi.lowering(\"handle_id\") on class wrappers requires zr.ffi.underlying(...)",
                "Add one supported fixed-width integer underlying type.");
    }
    if (contract.hasViewType &&
        !compiler_ffi_wrapper_view_is_source_extern_struct(
                cs, contract.viewType)) {
        return compiler_ffi_wrapper_invalid(
                cs,
                viewTypeNode,
                "zr.ffi.viewType on class wrappers requires a source extern struct name",
                "Reference a struct declared in a source native extern block.");
    }

    if (outContract != ZR_NULL) {
        *outContract = contract;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_ValidateFfiWrapperDecorators(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    return compiler_ffi_wrapper_bind_decorators(cs, declaration, ZR_NULL);
}
