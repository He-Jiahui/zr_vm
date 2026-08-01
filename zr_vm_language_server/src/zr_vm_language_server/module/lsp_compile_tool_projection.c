#include "module/lsp_compile_tool_projection.h"

#include <string.h>

static const ZrLibParameterDescriptor g_feature_parameters[] = {
        {.name = "name", .typeName = "string"},
};

static const ZrLibParameterDescriptor g_assert_parameters[] = {
        {.name = "condition", .typeName = "bool"},
        {.name = "message", .typeName = "string"},
        {.name = "target", .typeName = "SymbolId?"},
};

static const ZrLibParameterDescriptor g_diagnostic_parameters[] = {
        {.name = "message", .typeName = "string"},
        {.name = "target", .typeName = "SymbolId?"},
};

static const ZrLibMethodDescriptor g_build_methods[] = {
        {
                .name = "feature",
                .minArgumentCount = 1U,
                .maxArgumentCount = 1U,
                .returnTypeName = "bool",
                .documentation = "Declared build feature predicate. CompileTool only.",
                .isStatic = ZR_TRUE,
                .parameters = g_feature_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_feature_parameters),
                .contractRole = ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE,
        },
};

static const ZrLibFunctionDescriptor g_build_functions[] = {
        {
                .name = "assert",
                .minArgumentCount = 2U,
                .maxArgumentCount = 3U,
                .returnTypeName = "void",
                .documentation = "Compile-time assertion. CompileTool only.",
                .parameters = g_assert_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_assert_parameters),
                .contractRole = ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT,
        },
        {
                .name = "error",
                .minArgumentCount = 1U,
                .maxArgumentCount = 2U,
                .returnTypeName = "void",
                .documentation = "Compile-time error diagnostic. CompileTool only.",
                .parameters = g_diagnostic_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_diagnostic_parameters),
                .contractRole = ZR_PARSER_COMPILE_TOOL_ROLE_ERROR,
        },
        {
                .name = "warning",
                .minArgumentCount = 1U,
                .maxArgumentCount = 2U,
                .returnTypeName = "void",
                .documentation = "Compile-time warning diagnostic. CompileTool only.",
                .parameters = g_diagnostic_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_diagnostic_parameters),
                .contractRole = ZR_PARSER_COMPILE_TOOL_ROLE_WARNING,
        },
};

static const ZrLibFieldDescriptor g_declaration_view_fields[] = {
        {.name = "symbolId", .typeName = "SymbolId"},
        {.name = "kind", .typeName = "DeclarationKind"},
        {.name = "name", .typeName = "string"},
        {.name = "ownerSymbolId", .typeName = "SymbolId?"},
        {.name = "visibility", .typeName = "Visibility"},
        {.name = "sourceRange", .typeName = "SourceRange"},
        {.name = "attributes", .typeName = "AttributeData[]"},
};

static const ZrLibFieldDescriptor g_type_view_fields[] = {
        {.name = "symbolId", .typeName = "SymbolId"},
        {.name = "typeId", .typeName = "TypeId"},
        {.name = "capabilities", .typeName = "TypeCapabilities"},
        {.name = "fields", .typeName = "Field[]"},
        {.name = "methods", .typeName = "Method[]"},
        {.name = "properties", .typeName = "Property[]"},
        {.name = "interfaces", .typeName = "TypeId[]"},
};

static const ZrLibFieldDescriptor g_patch_fields[] = {
        {.name = "target", .typeName = "SymbolId"},
        {.name = "additions", .typeName = "GeneratedDeclaration[]"},
        {.name = "interfaceAdds", .typeName = "TypeId[]"},
        {.name = "attributeAdds", .typeName = "AttributeData[]"},
        {.name = "diagnostics", .typeName = "CompileDiagnostic[]"},
};

static const ZrLibFieldDescriptor g_generated_field_fields[] = {
        {.name = "name", .typeName = "string"},
        {.name = "type", .typeName = "TypeId"},
        {.name = "visibility", .typeName = "Visibility"},
        {.name = "mutability", .typeName = "Mutability"},
        {.name = "initializer", .typeName = "ConstantValue?"},
};

static const ZrLibFieldDescriptor g_compile_diagnostic_fields[] = {
        {.name = "isError", .typeName = "bool"},
        {.name = "message", .typeName = "string"},
        {.name = "target", .typeName = "SymbolId"},
};

static const ZrLibFieldDescriptor g_attribute_data_fields[] = {
        {.name = "typeId", .typeName = "TypeId"},
        {.name = "fieldValues", .typeName = "ConstantValue[]"},
};

#define ZR_COMPILE_TOOL_VIEW_TYPE(NAME, FIELDS) \
    {.name = (NAME), \
     .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, \
     .fields = (FIELDS), \
     .fieldCount = ZR_ARRAY_COUNT(FIELDS), \
     .documentation = "Immutable CompileTool declaration view."}

#define ZR_COMPILE_TOOL_OPAQUE_TYPE(NAME, DOCUMENTATION) \
    {.name = (NAME), \
     .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, \
     .documentation = (DOCUMENTATION)}

static const ZrLibTypeDescriptor g_build_types[] = {
        {
                .name = "build",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .methods = g_build_methods,
                .methodCount = ZR_ARRAY_COUNT(g_build_methods),
                .documentation = "CompileTool build predicates.",
        },
};

static const ZrLibTypeDescriptor g_declaration_types[] = {
        ZR_COMPILE_TOOL_OPAQUE_TYPE("SymbolId", "Canonical declaration symbol identity."),
        ZR_COMPILE_TOOL_VIEW_TYPE("DeclarationView", g_declaration_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("TypeView", g_type_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Class", g_type_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Struct", g_type_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Function", g_declaration_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Field", g_declaration_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Method", g_declaration_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Property", g_declaration_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Parameter", g_declaration_view_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("Patch", g_patch_fields),
        ZR_COMPILE_TOOL_OPAQUE_TYPE("GeneratedDeclaration", "Closed generated declaration union."),
        ZR_COMPILE_TOOL_VIEW_TYPE("GeneratedField", g_generated_field_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("CompileDiagnostic", g_compile_diagnostic_fields),
        ZR_COMPILE_TOOL_VIEW_TYPE("AttributeData", g_attribute_data_fields),
};

static const ZrLibModuleDescriptor g_build_module = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = ZR_PARSER_COMPILE_TOOL_MODULE_BUILD,
        .functions = g_build_functions,
        .functionCount = ZR_ARRAY_COUNT(g_build_functions),
        .types = g_build_types,
        .typeCount = ZR_ARRAY_COUNT(g_build_types),
        .documentation = "CompileTool projection; provider phase CompileTool; contract "
                         ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH,
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
        .publicContractHash = ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH,
};

static const ZrLibModuleDescriptor g_declaration_module = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION,
        .types = g_declaration_types,
        .typeCount = ZR_ARRAY_COUNT(g_declaration_types),
        .documentation = "CompileTool projection; provider phase CompileTool; contract "
                         ZR_PARSER_COMPILE_TOOL_DECLARATION_PUBLIC_CONTRACT_HASH,
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
        .publicContractHash = ZR_PARSER_COMPILE_TOOL_DECLARATION_PUBLIC_CONTRACT_HASH,
};

static TZrBool compile_tool_projection_role_is_present(
        const ZrLibModuleDescriptor *projection,
        EZrParserCompileToolRole role) {
    for (TZrSize index = 0U; projection != ZR_NULL && index < projection->functionCount; index++) {
        if (projection->functions[index].contractRole == (TZrUInt32)role) {
            return ZR_TRUE;
        }
    }
    for (TZrSize typeIndex = 0U; projection != ZR_NULL && typeIndex < projection->typeCount; typeIndex++) {
        const ZrLibTypeDescriptor *type = &projection->types[typeIndex];
        for (TZrSize methodIndex = 0U; methodIndex < type->methodCount; methodIndex++) {
            if (type->methods[methodIndex].contractRole == (TZrUInt32)role) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspCompileToolProjection_MatchesCanonical(
        const ZrLibModuleDescriptor *projection,
        const SZrParserCompileToolModuleDescriptor *canonical) {
    if (projection == ZR_NULL || canonical == ZR_NULL ||
        canonical->providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL ||
        projection->providerPhase != canonical->providerPhase ||
        projection->publicContractHash == ZR_NULL ||
        canonical->publicContractHash == ZR_NULL ||
        strcmp(projection->publicContractHash, canonical->publicContractHash) != 0 ||
        projection->moduleName == ZR_NULL || canonical->moduleName == ZR_NULL ||
        strcmp(projection->moduleName, canonical->moduleName) != 0 ||
        canonical->computedPublicContractHash == 0U ||
        canonical->computedPublicContractHash !=
                ZrParser_CompileTool_ComputePublicContractHash(canonical)) {
        return ZR_FALSE;
    }
    if (strcmp(canonical->moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) == 0) {
        if (projection->typeCount != canonical->typeCount) {
            return ZR_FALSE;
        }
        for (TZrSize index = 0U; index < canonical->typeCount; index++) {
            if (projection->types[index].name == ZR_NULL ||
                canonical->types[index].name == ZR_NULL ||
                strcmp(projection->types[index].name, canonical->types[index].name) != 0) {
                return ZR_FALSE;
            }
        }
    }
    for (TZrSize index = 0U; index < canonical->callableCount; index++) {
        if (!compile_tool_projection_role_is_present(
                    projection, canonical->callables[index].role)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

const ZrLibModuleDescriptor *ZrLanguageServer_LspCompileToolProjection_FindModule(
        const TZrChar *moduleName) {
    const SZrParserCompileToolModuleDescriptor *canonical =
            ZrParser_CompileTool_FindModule(moduleName);
    const ZrLibModuleDescriptor *projection = ZR_NULL;

    if (canonical == ZR_NULL) {
        return ZR_NULL;
    }
    if (strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD) == 0) {
        projection = &g_build_module;
    } else if (strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) == 0) {
        projection = &g_declaration_module;
    }
    return ZrLanguageServer_LspCompileToolProjection_MatchesCanonical(
                   projection, canonical)
                   ? projection
                   : ZR_NULL;
}
