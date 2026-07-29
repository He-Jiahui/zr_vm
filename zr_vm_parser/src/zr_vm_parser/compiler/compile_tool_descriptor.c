#include "zr_vm_parser/compile_tool.h"

#include <string.h>

#define ZR_COMPILE_TOOL_HASH_OFFSET ((TZrUInt64)14695981039346656037ULL)
#define ZR_COMPILE_TOOL_HASH_PRIME ((TZrUInt64)1099511628211ULL)

static const TZrChar *const g_feature_parameters[] = {"string"};
static const TZrChar *const g_assert_parameters[] = {"bool", "string", "SymbolId?"};
static const TZrChar *const g_diagnostic_parameters[] = {"string", "SymbolId?"};

static const SZrParserCompileToolCallableDescriptor g_build_callables[] = {
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE,
                .qualifiedName = "compile.build.feature",
                .returnTypeName = "bool",
                .parameterTypeNames = g_feature_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_feature_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_BUILD_FACTS,
        },
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT,
                .qualifiedName = "compile.assert",
                .returnTypeName = "void",
                .parameterTypeNames = g_assert_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_assert_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
        },
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_ERROR,
                .qualifiedName = "compile.error",
                .returnTypeName = "void",
                .parameterTypeNames = g_diagnostic_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_diagnostic_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
        },
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_WARNING,
                .qualifiedName = "compile.warning",
                .returnTypeName = "void",
                .parameterTypeNames = g_diagnostic_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_diagnostic_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
        },
};

static const TZrChar g_build_contract[] =
        "zr.compile/v1\n"
        "compile.build.feature(string)->bool|PureValue|BuildFacts\n"
        "compile.assert(bool,string,SymbolId?)->void|Diagnostic|LateCheck\n"
        "compile.error(string,SymbolId?)->void|Diagnostic|LateCheck\n"
        "compile.warning(string,SymbolId?)->void|Diagnostic|LateCheck\n";

static const SZrParserCompileToolModuleDescriptor g_build_descriptor = {
        .moduleName = ZR_PARSER_COMPILE_TOOL_MODULE_BUILD,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
        .publicContractHash = ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH,
        .canonicalContract = g_build_contract,
        .computedPublicContractHash = (TZrUInt64)0x00907d878657beb9ULL,
        .callables = g_build_callables,
        .callableCount = ZR_ARRAY_COUNT(g_build_callables),
};

const SZrParserCompileToolModuleDescriptor *ZrParser_CompileTool_FindModule(
        const TZrChar *moduleName) {
    if (moduleName == ZR_NULL) {
        return ZR_NULL;
    }
    if (strcmp(moduleName, g_build_descriptor.moduleName) == 0) {
        return &g_build_descriptor;
    }
    return ZR_NULL;
}

TZrBool ZrParser_CompileTool_IsModuleName(const TZrChar *moduleName) {
    return moduleName != ZR_NULL &&
           (strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD) == 0 ||
            strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) == 0);
}

const SZrParserCompileToolCallableDescriptor *ZrParser_CompileTool_FindCallable(
        const SZrParserCompileToolModuleDescriptor *module,
        EZrParserCompileToolRole role) {
    if (module == ZR_NULL || module->callables == ZR_NULL || role == ZR_PARSER_COMPILE_TOOL_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < module->callableCount; index++) {
        if (module->callables[index].role == role) {
            return &module->callables[index];
        }
    }
    return ZR_NULL;
}

TZrUInt64 ZrParser_CompileTool_ComputePublicContractHash(
        const SZrParserCompileToolModuleDescriptor *module) {
    TZrUInt64 hash = ZR_COMPILE_TOOL_HASH_OFFSET;
    const TZrByte *cursor;

    if (module == ZR_NULL || module->canonicalContract == ZR_NULL) {
        return 0U;
    }

    cursor = (const TZrByte *)module->canonicalContract;
    while (*cursor != 0U) {
        hash ^= (TZrUInt64)*cursor++;
        hash *= ZR_COMPILE_TOOL_HASH_PRIME;
    }
    return hash;
}
