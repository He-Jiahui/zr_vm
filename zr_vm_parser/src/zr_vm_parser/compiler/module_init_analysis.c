#include "module_init_analysis.h"

#include "type_inference_internal.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_library/file.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

typedef enum EZrParserInitBindingKind {
    ZR_PARSER_INIT_BINDING_LOCAL = 0,
    ZR_PARSER_INIT_BINDING_MODULE_ALIAS = 1,
    ZR_PARSER_INIT_BINDING_IMPORTED_SYMBOL = 2,
    ZR_PARSER_INIT_BINDING_TOP_LEVEL_CALLABLE = 3,
    ZR_PARSER_INIT_BINDING_TOP_LEVEL_ENTRY = 4,
    ZR_PARSER_INIT_BINDING_TOP_LEVEL_TYPE = 5
} EZrParserInitBindingKind;

typedef struct SZrParserInitBinding {
    SZrString *name;
    TZrUInt8 kind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    SZrString *moduleName;
    SZrString *symbolName;
    TZrUInt32 callableIndex;
} SZrParserInitBinding;

typedef struct SZrParserInitCallableInfo {
    SZrString *name;
    SZrAstNode *body;
    SZrAstNodeArray *params;
    TZrBool isExported;
    TZrBool isAnalyzing;
    TZrBool isAnalyzed;
    SZrArray effects;
} SZrParserInitCallableInfo;

typedef struct SZrParserInitAnalysisContext {
    SZrCompilerState *cs;
    SZrParserModuleInitSummary *summary;
    SZrString *moduleName;
    SZrAstNode *scriptAst;
    SZrArray callables;
    SZrArray bindings;
    TZrBool inCallableBody;
} SZrParserInitAnalysisContext;

typedef struct SZrParserSummaryPathBuffer {
    SZrString *modules[64];
    TZrUInt32 length;
} SZrParserSummaryPathBuffer;

typedef enum EZrBinaryMetadataSection {
    ZR_BINARY_METADATA_SECTION_NONE = 0,
    ZR_BINARY_METADATA_SECTION_LOCAL_BINDINGS = 1,
    ZR_BINARY_METADATA_SECTION_EXPORTED_SYMBOLS = 2,
    ZR_BINARY_METADATA_SECTION_STATIC_IMPORTS = 3,
    ZR_BINARY_METADATA_SECTION_MODULE_ENTRY_EFFECTS = 4,
    ZR_BINARY_METADATA_SECTION_EXPORTED_CALLABLE_SUMMARIES = 5,
    ZR_BINARY_METADATA_SECTION_TOP_LEVEL_CALLABLE_BINDINGS = 6
} EZrBinaryMetadataSection;

typedef struct SZrBinaryMetadataCallableBuilder {
    TZrBool active;
    SZrIoFunctionCallableSummary summary;
    SZrArray effects;
} SZrBinaryMetadataCallableBuilder;

typedef struct SZrBinaryMetadataSyntheticSource {
    SZrIoSource source;
    SZrIoModule module;
    SZrIoFunction function;
} SZrBinaryMetadataSyntheticSource;

static SZrParserModuleInitCache *module_init_get_cache(SZrGlobalState *global, TZrBool createIfMissing);
static SZrParserModuleInitSummary *module_init_find_summary_mutable(SZrGlobalState *global, SZrString *moduleName);
static SZrParserModuleInitSummary *module_init_find_summary_by_ast_mutable(SZrGlobalState *global,
                                                                           const SZrAstNode *ast);
static SZrParserModuleInitSummary *module_init_context_summary_mutable(SZrParserInitAnalysisContext *context);
static void module_init_free_summary(SZrGlobalState *global, SZrParserModuleInitSummary *summary);
static TZrBool module_init_reset_summary(SZrState *state,
                                         SZrParserModuleInitSummary *summary,
                                         SZrString *moduleName,
                                         const SZrAstNode *astIdentity);
static void module_init_summary_set_error(SZrParserModuleInitSummary *summary,
                                          const SZrFileRange *location,
                                          const TZrChar *message);
static void module_init_upsert_binding_info(SZrState *state,
                                            SZrParserModuleInitSummary *summary,
                                            SZrString *name,
                                            EZrParserInitBindingKind kind,
                                            SZrString *moduleName,
                                            SZrString *symbolName,
                                            EZrModuleExportKind exportKind,
                                            EZrModuleExportReadiness readiness,
                                            TZrUInt32 callableChildIndex);
static TZrBool module_init_prescan_source_summary(SZrState *state,
                                                  SZrParserModuleInitSummary *summary,
                                                  SZrAstNode *ast);
static TZrBool module_init_analyze_source_summary(SZrCompilerState *cs,
                                                  SZrParserModuleInitSummary *summary,
                                                  SZrAstNode *ast);
static TZrBool module_init_analyze_binary_summary(SZrCompilerState *cs,
                                                  SZrParserModuleInitSummary *summary,
                                                  const SZrIoFunction *function);
static TZrBool module_init_validate_summary(SZrCompilerState *cs, SZrParserModuleInitSummary *summary);
static TZrBool module_init_ensure_summary_analyzed(SZrCompilerState *cs, SZrString *moduleName);
static TZrBool module_init_binary_metadata_build_sidecar_path(const TZrChar *binaryPath,
                                                              TZrChar *buffer,
                                                              TZrSize bufferSize);
static void module_init_binary_metadata_free_symbol_array_buffers(SZrGlobalState *global, SZrArray *symbols);
static void module_init_binary_metadata_free_callable_summary_array_buffers(SZrGlobalState *global,
                                                                           SZrArray *summaries);
static void module_init_binary_metadata_release_synthetic_source(SZrGlobalState *global,
                                                                 SZrBinaryMetadataSyntheticSource *synthetic);

static TZrBool module_init_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void module_init_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!module_init_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-module-init] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}

static EZrValueType module_init_base_type_from_name(SZrString *typeName) {
    const TZrChar *text;

    if (typeName == ZR_NULL) {
        return ZR_VALUE_TYPE_OBJECT;
    }

    text = ZrCore_String_GetNativeString(typeName);
    if (text == ZR_NULL) {
        return ZR_VALUE_TYPE_OBJECT;
    }

    if (strcmp(text, "bool") == 0) {
        return ZR_VALUE_TYPE_BOOL;
    }
    if (strcmp(text, "i8") == 0) {
        return ZR_VALUE_TYPE_INT8;
    }
    if (strcmp(text, "i16") == 0) {
        return ZR_VALUE_TYPE_INT16;
    }
    if (strcmp(text, "i32") == 0) {
        return ZR_VALUE_TYPE_INT32;
    }
    if (strcmp(text, "int") == 0) {
        return ZR_VALUE_TYPE_INT64;
    }
    if (strcmp(text, "u8") == 0) {
        return ZR_VALUE_TYPE_UINT8;
    }
    if (strcmp(text, "u16") == 0) {
        return ZR_VALUE_TYPE_UINT16;
    }
    if (strcmp(text, "u32") == 0) {
        return ZR_VALUE_TYPE_UINT32;
    }
    if (strcmp(text, "uint") == 0) {
        return ZR_VALUE_TYPE_UINT64;
    }
    if (strcmp(text, "float") == 0) {
        return ZR_VALUE_TYPE_DOUBLE;
    }
    if (strcmp(text, "string") == 0) {
        return ZR_VALUE_TYPE_STRING;
    }
    if (strcmp(text, "function") == 0) {
        return ZR_VALUE_TYPE_FUNCTION;
    }
    if (strcmp(text, "array") == 0) {
        return ZR_VALUE_TYPE_ARRAY;
    }
    if (strcmp(text, "null") == 0) {
        return ZR_VALUE_TYPE_NULL;
    }

    return ZR_VALUE_TYPE_OBJECT;
}

static void module_init_fill_type_ref_from_type_node(SZrFunctionTypedTypeRef *outType, SZrType *typeNode) {
    SZrString *simpleTypeName = ZR_NULL;

    if (outType == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(outType, 0, sizeof(*outType));
    outType->baseType = ZR_VALUE_TYPE_OBJECT;
    outType->elementBaseType = ZR_VALUE_TYPE_OBJECT;

    if (typeNode == ZR_NULL) {
        return;
    }

    if (typeNode->name != ZR_NULL && typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        simpleTypeName = typeNode->name->data.identifier.name;
    }

    outType->isNullable = ZR_FALSE;
    outType->ownershipQualifier = (TZrUInt32)typeNode->ownershipQualifier;
    outType->typeName = simpleTypeName;
    outType->baseType = module_init_base_type_from_name(simpleTypeName);

    if (typeNode->dimensions > 0) {
        outType->isArray = ZR_TRUE;
        outType->elementBaseType = outType->baseType;
        outType->elementTypeName = outType->typeName;
        outType->baseType = ZR_VALUE_TYPE_ARRAY;
        outType->typeName = ZR_NULL;
    }
}

static TZrSize module_init_binary_metadata_count_indent(const TZrChar *line) {
    TZrSize indent = 0;

    if (line == ZR_NULL) {
        return 0;
    }

    while (line[indent] == ' ') {
        indent++;
    }
    return indent;
}

static TZrBool module_init_binary_metadata_copy_trimmed_slice(const TZrChar *start,
                                                              TZrSize length,
                                                              TZrChar *buffer,
                                                              TZrSize bufferSize) {
    TZrSize trimmedStart = 0;
    TZrSize trimmedLength = length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (start == ZR_NULL) {
        return ZR_FALSE;
    }

    while (trimmedStart < length &&
           (start[trimmedStart] == ' ' || start[trimmedStart] == '\t' || start[trimmedStart] == '\r')) {
        trimmedStart++;
    }
    while (trimmedLength > trimmedStart &&
           (start[trimmedLength - 1] == ' ' || start[trimmedLength - 1] == '\t' ||
            start[trimmedLength - 1] == '\r')) {
        trimmedLength--;
    }

    if (trimmedLength <= trimmedStart) {
        return ZR_TRUE;
    }

    trimmedLength -= trimmedStart;
    if (trimmedLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, start + trimmedStart, trimmedLength);
    buffer[trimmedLength] = '\0';
    return ZR_TRUE;
}

static TZrBool module_init_binary_metadata_copy_trimmed_text(const TZrChar *text,
                                                             TZrChar *buffer,
                                                             TZrSize bufferSize) {
    return text != ZR_NULL
               ? module_init_binary_metadata_copy_trimmed_slice(text, strlen(text), buffer, bufferSize)
               : ZR_FALSE;
}

static SZrString *module_init_binary_metadata_create_string(SZrState *state, const TZrChar *text) {
    return (state != ZR_NULL && text != ZR_NULL && text[0] != '\0')
               ? ZrCore_String_Create(state, (TZrNativeString)text, strlen(text))
               : ZR_NULL;
}

static void module_init_binary_metadata_fill_line_span(const TZrChar *line,
                                                       TZrUInt32 lineNumber,
                                                       TZrUInt32 *outStartLine,
                                                       TZrUInt32 *outStartColumn,
                                                       TZrUInt32 *outEndLine,
                                                       TZrUInt32 *outEndColumn) {
    TZrSize indent = module_init_binary_metadata_count_indent(line);
    TZrSize length = line != ZR_NULL ? strlen(line) : 0;

    if (outStartLine != ZR_NULL) {
        *outStartLine = lineNumber;
    }
    if (outStartColumn != ZR_NULL) {
        *outStartColumn = (TZrUInt32)indent + 1;
    }
    if (outEndLine != ZR_NULL) {
        *outEndLine = lineNumber;
    }
    if (outEndColumn != ZR_NULL) {
        *outEndColumn = (TZrUInt32)(length > indent ? length + 1 : indent + 1);
    }
}

static TZrBool module_init_binary_metadata_find_suffix_value(const TZrChar *line,
                                                             const TZrChar *key,
                                                             TZrChar *buffer,
                                                             TZrSize bufferSize) {
    const TZrChar *open;
    const TZrChar *close;
    const TZrChar *found;
    TZrSize length;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (line == ZR_NULL || key == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    open = strchr(line, '[');
    close = open != ZR_NULL ? strrchr(open, ']') : ZR_NULL;
    if (open == ZR_NULL || close == ZR_NULL || close <= open) {
        return ZR_FALSE;
    }

    found = strstr(open, key);
    if (found == ZR_NULL || found >= close) {
        return ZR_FALSE;
    }

    found += strlen(key);
    length = 0;
    while (found + length < close && found[length] != ' ' && found[length] != ']') {
        length++;
    }
    return module_init_binary_metadata_copy_trimmed_slice(found, length, buffer, bufferSize);
}

static TZrUInt32 module_init_binary_metadata_suffix_uint(const TZrChar *line,
                                                         const TZrChar *key,
                                                         TZrUInt32 defaultValue) {
    TZrChar valueBuffer[32];
    char *end = ZR_NULL;
    unsigned long value;

    if (!module_init_binary_metadata_find_suffix_value(line, key, valueBuffer, sizeof(valueBuffer)) ||
        valueBuffer[0] == '\0') {
        return defaultValue;
    }

    value = strtoul(valueBuffer, &end, 10);
    return end != ZR_NULL && *end == '\0' ? (TZrUInt32)value : defaultValue;
}

static TZrUInt8 module_init_binary_metadata_export_kind_from_text(const TZrChar *text, TZrUInt8 defaultKind) {
    if (text == ZR_NULL || text[0] == '\0') {
        return defaultKind;
    }
    if (strcmp(text, "value") == 0) {
        return ZR_MODULE_EXPORT_KIND_VALUE;
    }
    if (strcmp(text, "function") == 0) {
        return ZR_MODULE_EXPORT_KIND_FUNCTION;
    }
    if (strcmp(text, "type") == 0) {
        return ZR_MODULE_EXPORT_KIND_TYPE;
    }
    return defaultKind;
}

static TZrUInt8 module_init_binary_metadata_readiness_from_text(const TZrChar *text, TZrUInt8 defaultReadiness) {
    if (text == ZR_NULL || text[0] == '\0') {
        return defaultReadiness;
    }
    if (strcmp(text, "declaration") == 0) {
        return ZR_MODULE_EXPORT_READY_DECLARATION;
    }
    if (strcmp(text, "entry") == 0) {
        return ZR_MODULE_EXPORT_READY_ENTRY;
    }
    return defaultReadiness;
}

static TZrUInt8 module_init_binary_metadata_effect_kind_from_text(const TZrChar *text) {
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_MODULE_ENTRY_EFFECT_DYNAMIC_UNKNOWN;
    }
    if (strcmp(text, "IMPORT_REF") == 0) {
        return ZR_MODULE_ENTRY_EFFECT_IMPORT_REF;
    }
    if (strcmp(text, "IMPORT_READ") == 0) {
        return ZR_MODULE_ENTRY_EFFECT_IMPORT_READ;
    }
    if (strcmp(text, "IMPORT_CALL") == 0) {
        return ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    }
    if (strcmp(text, "LOCAL_CALL") == 0) {
        return ZR_MODULE_ENTRY_EFFECT_LOCAL_CALL;
    }
    if (strcmp(text, "LOCAL_ENTRY_BINDING_READ") == 0) {
        return ZR_MODULE_ENTRY_EFFECT_LOCAL_ENTRY_BINDING_READ;
    }
    return ZR_MODULE_ENTRY_EFFECT_DYNAMIC_UNKNOWN;
}

static void module_init_binary_metadata_parse_type_ref(SZrState *state,
                                                       const TZrChar *text,
                                                       SZrIoFunctionTypedTypeRef *outType) {
    TZrChar typeBuffer[256];
    TZrSize length;

    if (outType == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(outType, 0, sizeof(*outType));
    outType->baseType = ZR_VALUE_TYPE_OBJECT;
    outType->elementBaseType = ZR_VALUE_TYPE_OBJECT;

    if (!module_init_binary_metadata_copy_trimmed_text(text, typeBuffer, sizeof(typeBuffer)) ||
        typeBuffer[0] == '\0') {
        return;
    }

    length = strlen(typeBuffer);
    if (length >= 2 && typeBuffer[length - 2] == '[' && typeBuffer[length - 1] == ']') {
        typeBuffer[length - 2] = '\0';
        outType->isArray = ZR_TRUE;
        outType->baseType = ZR_VALUE_TYPE_ARRAY;
        outType->elementTypeName = module_init_binary_metadata_create_string(state, typeBuffer);
        outType->elementBaseType = module_init_base_type_from_name(outType->elementTypeName);
        if (outType->elementBaseType != ZR_VALUE_TYPE_OBJECT) {
            outType->elementTypeName = ZR_NULL;
        }
        return;
    }

    outType->typeName = module_init_binary_metadata_create_string(state, typeBuffer);
    outType->baseType = module_init_base_type_from_name(outType->typeName);
    if (outType->baseType != ZR_VALUE_TYPE_OBJECT) {
        outType->typeName = ZR_NULL;
    }
}

static TZrBool module_init_binary_metadata_parse_parameter_types(SZrState *state,
                                                                 const TZrChar *text,
                                                                 SZrIoFunctionTypedTypeRef **outTypes,
                                                                 TZrSize *outCount) {
    SZrArray parameters;
    const TZrChar *cursor = text;
    const TZrChar *segmentStart = text;
    TZrInt32 genericDepth = 0;

    if (outTypes != ZR_NULL) {
        *outTypes = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (state == ZR_NULL || text == ZR_NULL || outTypes == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&parameters);
    if (*text == '\0') {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state,
                      &parameters,
                      sizeof(SZrIoFunctionTypedTypeRef),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    while (cursor != ZR_NULL && *cursor != '\0') {
        if (*cursor == '<') {
            genericDepth++;
        } else if (*cursor == '>' && genericDepth > 0) {
            genericDepth--;
        } else if (*cursor == ',' && genericDepth == 0) {
            SZrIoFunctionTypedTypeRef parameterType;

            module_init_binary_metadata_parse_type_ref(state, segmentStart, &parameterType);
            ZrCore_Array_Push(state, &parameters, &parameterType);
            segmentStart = cursor + 1;
        }
        cursor++;
    }

    if (segmentStart != ZR_NULL && *segmentStart != '\0') {
        SZrIoFunctionTypedTypeRef parameterType;

        module_init_binary_metadata_parse_type_ref(state, segmentStart, &parameterType);
        ZrCore_Array_Push(state, &parameters, &parameterType);
    }

    if (parameters.length > 0) {
        *outTypes = (SZrIoFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrIoFunctionTypedTypeRef) * parameters.length,
            ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (*outTypes == ZR_NULL) {
            ZrCore_Array_Free(state, &parameters);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawCopy(*outTypes,
                              parameters.head,
                              sizeof(SZrIoFunctionTypedTypeRef) * parameters.length);
        *outCount = parameters.length;
    }

    ZrCore_Array_Free(state, &parameters);
    return ZR_TRUE;
}

static TZrBool module_init_binary_metadata_parse_export_symbol_line(SZrState *state,
                                                                    const TZrChar *line,
                                                                    TZrUInt32 lineNumber,
                                                                    SZrIoFunctionTypedExportSymbol *outSymbol) {
    const TZrChar *trimmed;
    TZrChar suffixBuffer[32];

    if (outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outSymbol, 0, sizeof(*outSymbol));
    outSymbol->accessModifier = ZR_ACCESS_PUBLIC;
    outSymbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    module_init_binary_metadata_fill_line_span(line,
                                               lineNumber,
                                               &outSymbol->lineInSourceStart,
                                               &outSymbol->columnInSourceStart,
                                               &outSymbol->lineInSourceEnd,
                                               &outSymbol->columnInSourceEnd);

    trimmed = line != ZR_NULL ? line + module_init_binary_metadata_count_indent(line) : ZR_NULL;
    if (trimmed == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strncmp(trimmed, "fn ", 3) == 0) {
        const TZrChar *nameStart = trimmed + 3;
        const TZrChar *openParen = strchr(nameStart, '(');
        const TZrChar *closeParen = openParen != ZR_NULL ? strchr(openParen, ')') : ZR_NULL;
        const TZrChar *colon = closeParen != ZR_NULL ? strchr(closeParen, ':') : ZR_NULL;
        const TZrChar *suffixStart;
        TZrChar nameBuffer[256];
        TZrChar returnTypeBuffer[256];
        TZrChar parameterBuffer[256];

        if (openParen == ZR_NULL || closeParen == ZR_NULL || colon == ZR_NULL ||
            !module_init_binary_metadata_copy_trimmed_slice(nameStart,
                                                            (TZrSize)(openParen - nameStart),
                                                            nameBuffer,
                                                            sizeof(nameBuffer)) ||
            !module_init_binary_metadata_copy_trimmed_slice(openParen + 1,
                                                            (TZrSize)(closeParen - openParen - 1),
                                                            parameterBuffer,
                                                            sizeof(parameterBuffer))) {
            return ZR_FALSE;
        }

        suffixStart = strchr(colon + 1, '[');
        if (!module_init_binary_metadata_copy_trimmed_slice(colon + 1,
                                                            suffixStart != ZR_NULL
                                                                ? (TZrSize)(suffixStart - (colon + 1))
                                                                : strlen(colon + 1),
                                                            returnTypeBuffer,
                                                            sizeof(returnTypeBuffer))) {
            return ZR_FALSE;
        }

        outSymbol->name = module_init_binary_metadata_create_string(state, nameBuffer);
        outSymbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
        outSymbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
        outSymbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
        module_init_binary_metadata_parse_type_ref(state, returnTypeBuffer, &outSymbol->valueType);
        if (!module_init_binary_metadata_parse_parameter_types(state,
                                                              parameterBuffer,
                                                              &outSymbol->parameterTypes,
                                                              &outSymbol->parameterCount)) {
            return ZR_FALSE;
        }
    } else if (strncmp(trimmed, "var ", 4) == 0) {
        const TZrChar *nameStart = trimmed + 4;
        const TZrChar *colon = strchr(nameStart, ':');
        const TZrChar *suffixStart;
        TZrChar nameBuffer[256];
        TZrChar valueTypeBuffer[256];

        if (colon == ZR_NULL ||
            !module_init_binary_metadata_copy_trimmed_slice(nameStart,
                                                            (TZrSize)(colon - nameStart),
                                                            nameBuffer,
                                                            sizeof(nameBuffer))) {
            return ZR_FALSE;
        }

        suffixStart = strchr(colon + 1, '[');
        if (!module_init_binary_metadata_copy_trimmed_slice(colon + 1,
                                                            suffixStart != ZR_NULL
                                                                ? (TZrSize)(suffixStart - (colon + 1))
                                                                : strlen(colon + 1),
                                                            valueTypeBuffer,
                                                            sizeof(valueTypeBuffer))) {
            return ZR_FALSE;
        }

        outSymbol->name = module_init_binary_metadata_create_string(state, nameBuffer);
        outSymbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_VARIABLE;
        outSymbol->exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
        outSymbol->readiness = ZR_MODULE_EXPORT_READY_ENTRY;
        module_init_binary_metadata_parse_type_ref(state, valueTypeBuffer, &outSymbol->valueType);
    } else {
        return ZR_FALSE;
    }

    if (module_init_binary_metadata_find_suffix_value(trimmed, "kind=", suffixBuffer, sizeof(suffixBuffer))) {
        outSymbol->exportKind = module_init_binary_metadata_export_kind_from_text(suffixBuffer, outSymbol->exportKind);
    }
    if (module_init_binary_metadata_find_suffix_value(trimmed, "readiness=", suffixBuffer, sizeof(suffixBuffer))) {
        outSymbol->readiness =
            module_init_binary_metadata_readiness_from_text(suffixBuffer, outSymbol->readiness);
    }
    outSymbol->callableChildIndex =
        module_init_binary_metadata_suffix_uint(trimmed, "child=", outSymbol->callableChildIndex);
    return outSymbol->name != ZR_NULL;
}

static TZrBool module_init_binary_metadata_parse_effect_line(SZrState *state,
                                                             const TZrChar *line,
                                                             TZrUInt32 lineNumber,
                                                             SZrIoFunctionModuleEffect *outEffect) {
    const TZrChar *trimmed;
    const TZrChar *firstSpace;
    const TZrChar *suffixStart;
    const TZrChar *lastDot;
    TZrChar kindBuffer[64];
    TZrChar targetBuffer[256];
    TZrChar moduleBuffer[256];
    TZrChar symbolBuffer[256];
    TZrChar suffixBuffer[32];

    if (outEffect == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outEffect, 0, sizeof(*outEffect));
    module_init_binary_metadata_fill_line_span(line,
                                               lineNumber,
                                               &outEffect->lineInSourceStart,
                                               &outEffect->columnInSourceStart,
                                               &outEffect->lineInSourceEnd,
                                               &outEffect->columnInSourceEnd);

    trimmed = line != ZR_NULL ? line + module_init_binary_metadata_count_indent(line) : ZR_NULL;
    if (trimmed == ZR_NULL) {
        return ZR_FALSE;
    }

    firstSpace = strchr(trimmed, ' ');
    if (firstSpace == ZR_NULL ||
        !module_init_binary_metadata_copy_trimmed_slice(trimmed,
                                                        (TZrSize)(firstSpace - trimmed),
                                                        kindBuffer,
                                                        sizeof(kindBuffer))) {
        return ZR_FALSE;
    }

    suffixStart = strchr(firstSpace + 1, '[');
    if (!module_init_binary_metadata_copy_trimmed_slice(firstSpace + 1,
                                                        suffixStart != ZR_NULL
                                                            ? (TZrSize)(suffixStart - (firstSpace + 1))
                                                            : strlen(firstSpace + 1),
                                                        targetBuffer,
                                                        sizeof(targetBuffer))) {
        return ZR_FALSE;
    }

    lastDot = strrchr(targetBuffer, '.');
    if (lastDot == ZR_NULL ||
        !module_init_binary_metadata_copy_trimmed_slice(targetBuffer,
                                                        (TZrSize)(lastDot - targetBuffer),
                                                        moduleBuffer,
                                                        sizeof(moduleBuffer)) ||
        !module_init_binary_metadata_copy_trimmed_text(lastDot + 1, symbolBuffer, sizeof(symbolBuffer))) {
        return ZR_FALSE;
    }

    outEffect->kind = module_init_binary_metadata_effect_kind_from_text(kindBuffer);
    outEffect->exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
    outEffect->readiness = ZR_MODULE_EXPORT_READY_ENTRY;
    outEffect->moduleName = module_init_binary_metadata_create_string(state, moduleBuffer);
    outEffect->symbolName = module_init_binary_metadata_create_string(state, symbolBuffer);

    if (module_init_binary_metadata_find_suffix_value(trimmed, "kind=", suffixBuffer, sizeof(suffixBuffer))) {
        outEffect->exportKind = module_init_binary_metadata_export_kind_from_text(suffixBuffer, outEffect->exportKind);
    }
    if (module_init_binary_metadata_find_suffix_value(trimmed, "readiness=", suffixBuffer, sizeof(suffixBuffer))) {
        outEffect->readiness =
            module_init_binary_metadata_readiness_from_text(suffixBuffer, outEffect->readiness);
    }
    return outEffect->moduleName != ZR_NULL && outEffect->symbolName != ZR_NULL;
}

static void module_init_binary_metadata_callable_builder_reset(SZrBinaryMetadataCallableBuilder *builder) {
    if (builder == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(builder, 0, sizeof(*builder));
    ZrCore_Array_Construct(&builder->effects);
}

static TZrBool module_init_binary_metadata_finalize_callable_builder(SZrState *state,
                                                                     SZrBinaryMetadataCallableBuilder *builder,
                                                                     SZrArray *summaries) {
    if (builder == ZR_NULL || summaries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!builder->active) {
        return ZR_TRUE;
    }

    builder->summary.effectCount = builder->effects.length;
    if (builder->effects.length > 0) {
        builder->summary.effects = (SZrIoFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrIoFunctionModuleEffect) * builder->effects.length,
            ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (builder->summary.effects == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawCopy(builder->summary.effects,
                              builder->effects.head,
                              sizeof(SZrIoFunctionModuleEffect) * builder->effects.length);
    }

    ZrCore_Array_Push(state, summaries, &builder->summary);
    ZrCore_Array_Free(state, &builder->effects);
    module_init_binary_metadata_callable_builder_reset(builder);
    return ZR_TRUE;
}

static TZrBool module_init_binary_metadata_parse_callable_summary_header(
    SZrState *state,
    const TZrChar *line,
    SZrIoFunctionCallableSummary *outSummary) {
    const TZrChar *trimmed;
    const TZrChar *suffixStart;
    TZrChar nameBuffer[256];

    if (outSummary == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outSummary, 0, sizeof(*outSummary));
    trimmed = line != ZR_NULL ? line + module_init_binary_metadata_count_indent(line) : ZR_NULL;
    if (trimmed == ZR_NULL || strncmp(trimmed, "fn ", 3) != 0) {
        return ZR_FALSE;
    }

    suffixStart = strchr(trimmed + 3, '[');
    if (!module_init_binary_metadata_copy_trimmed_slice(trimmed + 3,
                                                        suffixStart != ZR_NULL
                                                            ? (TZrSize)(suffixStart - (trimmed + 3))
                                                            : strlen(trimmed + 3),
                                                        nameBuffer,
                                                        sizeof(nameBuffer))) {
        return ZR_FALSE;
    }

    outSummary->name = module_init_binary_metadata_create_string(state, nameBuffer);
    outSummary->callableChildIndex =
        module_init_binary_metadata_suffix_uint(trimmed, "child=", ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
    return outSummary->name != ZR_NULL;
}

static TZrBool module_init_binary_metadata_parse_top_level_binding_line(
    SZrState *state,
    const TZrChar *line,
    SZrIoFunctionTopLevelCallableBinding *outBinding) {
    const TZrChar *trimmed;
    const TZrChar *suffixStart;
    TZrChar nameBuffer[256];
    TZrChar suffixBuffer[32];

    if (outBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outBinding, 0, sizeof(*outBinding));
    outBinding->accessModifier = ZR_ACCESS_PUBLIC;
    outBinding->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    outBinding->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    outBinding->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;

    trimmed = line != ZR_NULL ? line + module_init_binary_metadata_count_indent(line) : ZR_NULL;
    if (trimmed == ZR_NULL || strncmp(trimmed, "fn ", 3) != 0) {
        return ZR_FALSE;
    }

    suffixStart = strchr(trimmed + 3, '[');
    if (!module_init_binary_metadata_copy_trimmed_slice(trimmed + 3,
                                                        suffixStart != ZR_NULL
                                                            ? (TZrSize)(suffixStart - (trimmed + 3))
                                                            : strlen(trimmed + 3),
                                                        nameBuffer,
                                                        sizeof(nameBuffer))) {
        return ZR_FALSE;
    }

    outBinding->name = module_init_binary_metadata_create_string(state, nameBuffer);
    outBinding->stackSlot = module_init_binary_metadata_suffix_uint(trimmed, "slot=", 0);
    outBinding->callableChildIndex =
        module_init_binary_metadata_suffix_uint(trimmed, "child=", outBinding->callableChildIndex);
    outBinding->accessModifier =
        (TZrUInt8)module_init_binary_metadata_suffix_uint(trimmed, "access=", outBinding->accessModifier);

    if (module_init_binary_metadata_find_suffix_value(trimmed, "kind=", suffixBuffer, sizeof(suffixBuffer))) {
        outBinding->exportKind = module_init_binary_metadata_export_kind_from_text(suffixBuffer, outBinding->exportKind);
    }
    if (module_init_binary_metadata_find_suffix_value(trimmed, "readiness=", suffixBuffer, sizeof(suffixBuffer))) {
        outBinding->readiness =
            module_init_binary_metadata_readiness_from_text(suffixBuffer, outBinding->readiness);
    }
    return outBinding->name != ZR_NULL;
}

static EZrBinaryMetadataSection module_init_binary_metadata_section_from_line(const TZrChar *line) {
    if (line == ZR_NULL) {
        return ZR_BINARY_METADATA_SECTION_NONE;
    }
    if (strncmp(line, "LOCAL_BINDINGS ", 15) == 0) {
        return ZR_BINARY_METADATA_SECTION_LOCAL_BINDINGS;
    }
    if (strncmp(line, "EXPORTED_SYMBOLS ", 17) == 0) {
        return ZR_BINARY_METADATA_SECTION_EXPORTED_SYMBOLS;
    }
    if (strncmp(line, "STATIC_IMPORTS ", 15) == 0) {
        return ZR_BINARY_METADATA_SECTION_STATIC_IMPORTS;
    }
    if (strncmp(line, "MODULE_ENTRY_EFFECTS ", 21) == 0) {
        return ZR_BINARY_METADATA_SECTION_MODULE_ENTRY_EFFECTS;
    }
    if (strncmp(line, "EXPORTED_CALLABLE_SUMMARIES ", 28) == 0) {
        return ZR_BINARY_METADATA_SECTION_EXPORTED_CALLABLE_SUMMARIES;
    }
    if (strncmp(line, "TOP_LEVEL_CALLABLE_BINDINGS ", 28) == 0) {
        return ZR_BINARY_METADATA_SECTION_TOP_LEVEL_CALLABLE_BINDINGS;
    }
    return ZR_BINARY_METADATA_SECTION_NONE;
}

static TZrBool module_init_binary_metadata_has_synthetic_marker(const SZrIoSource *source) {
    return source != ZR_NULL &&
           source->optional[0] == 'Z' &&
           source->optional[1] == 'R' &&
           source->optional[2] == 'I';
}

static TZrBool module_init_binary_metadata_build_sidecar_path(const TZrChar *binaryPath,
                                                              TZrChar *buffer,
                                                              TZrSize bufferSize) {
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (binaryPath == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(binaryPath);
    if (length >= ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION_LENGTH &&
        strcmp(binaryPath + length - ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION_LENGTH,
               ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION) == 0) {
        if (length + 1 > bufferSize) {
            return ZR_FALSE;
        }
        memcpy(buffer, binaryPath, length + 1);
        return ZR_TRUE;
    }

    if (length < ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH ||
        strcmp(binaryPath + length - ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH,
               ZR_VM_BINARY_MODULE_FILE_EXTENSION) != 0 ||
        length + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, binaryPath, length + 1);
    memcpy(buffer + length - ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH,
           ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
           ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION_LENGTH + 1);
    return ZR_TRUE;
}

static void module_init_binary_metadata_free_symbol_array_buffers(SZrGlobalState *global, SZrArray *symbols) {
    TZrSize index;

    if (global == ZR_NULL || symbols == ZR_NULL || !symbols->isValid) {
        return;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrIoFunctionTypedExportSymbol *symbol =
            (SZrIoFunctionTypedExportSymbol *)ZrCore_Array_Get(symbols, index);
        if (symbol != ZR_NULL && symbol->parameterTypes != ZR_NULL && symbol->parameterCount > 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          symbol->parameterTypes,
                                          sizeof(SZrIoFunctionTypedTypeRef) * symbol->parameterCount,
                                          ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            symbol->parameterTypes = ZR_NULL;
            symbol->parameterCount = 0;
        }
    }

    if (global->mainThreadState != ZR_NULL) {
        ZrCore_Array_Free(global->mainThreadState, symbols);
    }
}

static void module_init_binary_metadata_free_callable_summary_array_buffers(SZrGlobalState *global,
                                                                           SZrArray *summaries) {
    TZrSize index;

    if (global == ZR_NULL || summaries == ZR_NULL || !summaries->isValid) {
        return;
    }

    for (index = 0; index < summaries->length; index++) {
        SZrIoFunctionCallableSummary *summary =
            (SZrIoFunctionCallableSummary *)ZrCore_Array_Get(summaries, index);
        if (summary != ZR_NULL && summary->effects != ZR_NULL && summary->effectCount > 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          summary->effects,
                                          sizeof(SZrIoFunctionModuleEffect) * summary->effectCount,
                                          ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            summary->effects = ZR_NULL;
            summary->effectCount = 0;
        }
    }

    if (global->mainThreadState != ZR_NULL) {
        ZrCore_Array_Free(global->mainThreadState, summaries);
    }
}

static void module_init_binary_metadata_release_synthetic_source(SZrGlobalState *global,
                                                                 SZrBinaryMetadataSyntheticSource *synthetic) {
    SZrIoFunction *function;
    TZrSize index;

    if (global == ZR_NULL || synthetic == ZR_NULL) {
        return;
    }

    function = &synthetic->function;
    if (function->typedExportedSymbols != ZR_NULL) {
        for (index = 0; index < function->typedExportedSymbolsLength; index++) {
            if (function->typedExportedSymbols[index].parameterTypes != ZR_NULL &&
                function->typedExportedSymbols[index].parameterCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              function->typedExportedSymbols[index].parameterTypes,
                                              sizeof(SZrIoFunctionTypedTypeRef) *
                                                  function->typedExportedSymbols[index].parameterCount,
                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->typedExportedSymbols,
                                      sizeof(SZrIoFunctionTypedExportSymbol) * function->typedExportedSymbolsLength,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    }

    if (function->staticImports != ZR_NULL && function->staticImportsLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->staticImports,
                                      sizeof(SZrString *) * function->staticImportsLength,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    }

    if (function->moduleEntryEffects != ZR_NULL && function->moduleEntryEffectsLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->moduleEntryEffects,
                                      sizeof(SZrIoFunctionModuleEffect) * function->moduleEntryEffectsLength,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    }

    if (function->exportedCallableSummaries != ZR_NULL) {
        for (index = 0; index < function->exportedCallableSummariesLength; index++) {
            if (function->exportedCallableSummaries[index].effects != ZR_NULL &&
                function->exportedCallableSummaries[index].effectCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              function->exportedCallableSummaries[index].effects,
                                              sizeof(SZrIoFunctionModuleEffect) *
                                                  function->exportedCallableSummaries[index].effectCount,
                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->exportedCallableSummaries,
                                      sizeof(SZrIoFunctionCallableSummary) *
                                          function->exportedCallableSummariesLength,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    }

    if (function->topLevelCallableBindings != ZR_NULL && function->topLevelCallableBindingsLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->topLevelCallableBindings,
                                      sizeof(SZrIoFunctionTopLevelCallableBinding) *
                                          function->topLevelCallableBindingsLength,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    }

    ZrCore_Memory_RawFreeWithType(global,
                                  synthetic,
                                  sizeof(SZrBinaryMetadataSyntheticSource),
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
}

TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromPath(SZrState *state,
                                                                        const TZrChar *binaryPath,
                                                                        SZrIoSource **outSource) {
    TZrChar metadataPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *metadataText = ZR_NULL;
    TZrSize metadataLength = 0;
    const TZrChar *cursor;
    TZrUInt32 lineNumber = 0;
    TZrBool rootFound = ZR_FALSE;
    EZrBinaryMetadataSection currentSection = ZR_BINARY_METADATA_SECTION_NONE;
    SZrArray exports;
    SZrArray staticImports;
    SZrArray entryEffects;
    SZrArray callableSummaries;
    SZrArray topLevelCallableBindings;
    SZrBinaryMetadataCallableBuilder callableBuilder;
    SZrBinaryMetadataSyntheticSource *synthetic = ZR_NULL;

    ZrCore_Array_Construct(&exports);
    ZrCore_Array_Construct(&staticImports);
    ZrCore_Array_Construct(&entryEffects);
    ZrCore_Array_Construct(&callableSummaries);
    ZrCore_Array_Construct(&topLevelCallableBindings);
    module_init_binary_metadata_callable_builder_reset(&callableBuilder);

    if (outSource != ZR_NULL) {
        *outSource = ZR_NULL;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || binaryPath == ZR_NULL || outSource == ZR_NULL ||
        !module_init_binary_metadata_build_sidecar_path(binaryPath, metadataPath, sizeof(metadataPath))) {
        return ZR_FALSE;
    }

    metadataText = ZrLibrary_File_ReadAll(state->global, metadataPath);
    if (metadataText == ZR_NULL) {
        return ZR_FALSE;
    }

    metadataLength = strlen(metadataText);
    cursor = metadataText;
    ZrCore_Array_Init(state, &exports, sizeof(SZrIoFunctionTypedExportSymbol), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state, &staticImports, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state, &entryEffects, sizeof(SZrIoFunctionModuleEffect), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &callableSummaries,
                      sizeof(SZrIoFunctionCallableSummary),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &topLevelCallableBindings,
                      sizeof(SZrIoFunctionTopLevelCallableBinding),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);

    while (cursor != ZR_NULL && *cursor != '\0') {
        const TZrChar *lineStart = cursor;
        TZrSize lineLength = 0;
        TZrSize rawLineLength = 0;
        TZrSize indent;
        TZrChar lineBuffer[1024];
        TZrChar *trimmedLine;

        while (cursor[rawLineLength] != '\0' && cursor[rawLineLength] != '\n') {
            rawLineLength++;
        }
        lineLength = rawLineLength;
        if (lineLength >= sizeof(lineBuffer)) {
            lineLength = sizeof(lineBuffer) - 1;
        }

        memcpy(lineBuffer, lineStart, lineLength);
        lineBuffer[lineLength] = '\0';
        while (lineLength > 0 && lineBuffer[lineLength - 1] == '\r') {
            lineBuffer[--lineLength] = '\0';
        }

        cursor += lineStart[rawLineLength] == '\n' ? rawLineLength + 1 : rawLineLength;
        lineNumber++;
        indent = module_init_binary_metadata_count_indent(lineBuffer);
        trimmedLine = lineBuffer + indent;

        if (!rootFound) {
            if (indent == 0 && strcmp(trimmedLine, "TYPE_METADATA:") == 0) {
                rootFound = ZR_TRUE;
            }
            continue;
        }

        if (indent == 0) {
            if (trimmedLine[0] != '\0') {
                break;
            }
            continue;
        }

        if (indent == 2) {
            if (!module_init_binary_metadata_finalize_callable_builder(state, &callableBuilder, &callableSummaries)) {
                goto cleanup;
            }
            currentSection = module_init_binary_metadata_section_from_line(trimmedLine);
            continue;
        }

        if (currentSection == ZR_BINARY_METADATA_SECTION_EXPORTED_CALLABLE_SUMMARIES &&
            indent >= 6 &&
            callableBuilder.active) {
            SZrIoFunctionModuleEffect effect;

            if (!module_init_binary_metadata_parse_effect_line(state, lineBuffer, lineNumber, &effect)) {
                goto cleanup;
            }
            ZrCore_Array_Push(state, &callableBuilder.effects, &effect);
            continue;
        }

        if (indent < 4) {
            continue;
        }

        switch (currentSection) {
            case ZR_BINARY_METADATA_SECTION_EXPORTED_SYMBOLS: {
                SZrIoFunctionTypedExportSymbol symbol;
                if (!module_init_binary_metadata_parse_export_symbol_line(state, lineBuffer, lineNumber, &symbol)) {
                    goto cleanup;
                }
                ZrCore_Array_Push(state, &exports, &symbol);
                break;
            }
            case ZR_BINARY_METADATA_SECTION_STATIC_IMPORTS: {
                if (strncmp(trimmedLine, "import ", 7) == 0) {
                    TZrChar moduleBuffer[256];
                    SZrString *importName;

                    if (!module_init_binary_metadata_copy_trimmed_text(trimmedLine + 7,
                                                                       moduleBuffer,
                                                                       sizeof(moduleBuffer))) {
                        goto cleanup;
                    }
                    importName = module_init_binary_metadata_create_string(state, moduleBuffer);
                    ZrCore_Array_Push(state, &staticImports, &importName);
                }
                break;
            }
            case ZR_BINARY_METADATA_SECTION_MODULE_ENTRY_EFFECTS: {
                SZrIoFunctionModuleEffect effect;
                if (!module_init_binary_metadata_parse_effect_line(state, lineBuffer, lineNumber, &effect)) {
                    goto cleanup;
                }
                ZrCore_Array_Push(state, &entryEffects, &effect);
                break;
            }
            case ZR_BINARY_METADATA_SECTION_EXPORTED_CALLABLE_SUMMARIES: {
                if (!module_init_binary_metadata_finalize_callable_builder(state, &callableBuilder, &callableSummaries) ||
                    !module_init_binary_metadata_parse_callable_summary_header(state,
                                                                              lineBuffer,
                                                                              &callableBuilder.summary)) {
                    goto cleanup;
                }
                callableBuilder.active = ZR_TRUE;
                ZrCore_Array_Init(state,
                                  &callableBuilder.effects,
                                  sizeof(SZrIoFunctionModuleEffect),
                                  ZR_PARSER_INITIAL_CAPACITY_SMALL);
                break;
            }
            case ZR_BINARY_METADATA_SECTION_TOP_LEVEL_CALLABLE_BINDINGS: {
                SZrIoFunctionTopLevelCallableBinding binding;
                if (!module_init_binary_metadata_parse_top_level_binding_line(state, lineBuffer, &binding)) {
                    goto cleanup;
                }
                ZrCore_Array_Push(state, &topLevelCallableBindings, &binding);
                break;
            }
            default:
                break;
        }
    }

    if (!rootFound ||
        !module_init_binary_metadata_finalize_callable_builder(state, &callableBuilder, &callableSummaries)) {
        goto cleanup;
    }

    synthetic = (SZrBinaryMetadataSyntheticSource *)ZrCore_Memory_RawMallocWithType(
        state->global,
        sizeof(SZrBinaryMetadataSyntheticSource),
        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (synthetic == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(synthetic, 0, sizeof(*synthetic));
    memcpy(synthetic->source.signature, "ZRI", 4);
    synthetic->source.optional[0] = 'Z';
    synthetic->source.optional[1] = 'R';
    synthetic->source.optional[2] = 'I';
    synthetic->source.modulesLength = 1;
    synthetic->source.modules = &synthetic->module;
    synthetic->module.entryFunction = &synthetic->function;

    if (exports.length > 0) {
        synthetic->function.typedExportedSymbols =
            (SZrIoFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrIoFunctionTypedExportSymbol) * exports.length,
                ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (synthetic->function.typedExportedSymbols == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawCopy(synthetic->function.typedExportedSymbols,
                              exports.head,
                              sizeof(SZrIoFunctionTypedExportSymbol) * exports.length);
        synthetic->function.typedExportedSymbolsLength = exports.length;
        ZrCore_Array_Free(state, &exports);
    }

    if (staticImports.length > 0) {
        synthetic->function.staticImports = (SZrString **)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrString *) * staticImports.length,
            ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (synthetic->function.staticImports == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawCopy(synthetic->function.staticImports,
                              staticImports.head,
                              sizeof(SZrString *) * staticImports.length);
        synthetic->function.staticImportsLength = staticImports.length;
        ZrCore_Array_Free(state, &staticImports);
    }

    if (entryEffects.length > 0) {
        synthetic->function.moduleEntryEffects = (SZrIoFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrIoFunctionModuleEffect) * entryEffects.length,
            ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (synthetic->function.moduleEntryEffects == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawCopy(synthetic->function.moduleEntryEffects,
                              entryEffects.head,
                              sizeof(SZrIoFunctionModuleEffect) * entryEffects.length);
        synthetic->function.moduleEntryEffectsLength = entryEffects.length;
        ZrCore_Array_Free(state, &entryEffects);
    }

    if (callableSummaries.length > 0) {
        synthetic->function.exportedCallableSummaries =
            (SZrIoFunctionCallableSummary *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrIoFunctionCallableSummary) * callableSummaries.length,
                ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (synthetic->function.exportedCallableSummaries == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawCopy(synthetic->function.exportedCallableSummaries,
                              callableSummaries.head,
                              sizeof(SZrIoFunctionCallableSummary) * callableSummaries.length);
        synthetic->function.exportedCallableSummariesLength = callableSummaries.length;
        ZrCore_Array_Free(state, &callableSummaries);
    }

    if (topLevelCallableBindings.length > 0) {
        synthetic->function.topLevelCallableBindings =
            (SZrIoFunctionTopLevelCallableBinding *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrIoFunctionTopLevelCallableBinding) * topLevelCallableBindings.length,
                ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (synthetic->function.topLevelCallableBindings == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawCopy(synthetic->function.topLevelCallableBindings,
                              topLevelCallableBindings.head,
                              sizeof(SZrIoFunctionTopLevelCallableBinding) * topLevelCallableBindings.length);
        synthetic->function.topLevelCallableBindingsLength = topLevelCallableBindings.length;
        ZrCore_Array_Free(state, &topLevelCallableBindings);
    }

    *outSource = &synthetic->source;
    ZrCore_Memory_RawFreeWithType(state->global,
                                  metadataText,
                                  metadataLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return ZR_TRUE;

cleanup:
    if (synthetic != ZR_NULL) {
        module_init_binary_metadata_release_synthetic_source(state->global, synthetic);
    }
    if (callableBuilder.effects.isValid) {
        ZrCore_Array_Free(state, &callableBuilder.effects);
    }
    module_init_binary_metadata_free_symbol_array_buffers(state->global, &exports);
    module_init_binary_metadata_free_callable_summary_array_buffers(state->global, &callableSummaries);
    if (staticImports.isValid) {
        ZrCore_Array_Free(state, &staticImports);
    }
    if (entryEffects.isValid) {
        ZrCore_Array_Free(state, &entryEffects);
    }
    if (topLevelCallableBindings.isValid) {
        ZrCore_Array_Free(state, &topLevelCallableBindings);
    }
    if (metadataText != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      metadataText,
                                      metadataLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(SZrState *state,
                                                                      const SZrIo *io,
                                                                      SZrIoSource **outSource) {
    const SZrLibrary_File_Reader *reader = ZR_NULL;
    SZrIo directIo;

    if (outSource != ZR_NULL) {
        *outSource = ZR_NULL;
    }
    if (state == ZR_NULL || io == ZR_NULL || outSource == ZR_NULL) {
        return ZR_FALSE;
    }

    if (io->read == ZrLibrary_File_SourceReadImplementation &&
        io->close == ZrLibrary_File_SourceCloseImplementation &&
        io->customData != ZR_NULL) {
        reader = (const SZrLibrary_File_Reader *)io->customData;
        if (reader->normalizedPath[0] != '\0' &&
            ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromPath(state,
                                                                            reader->normalizedPath,
                                                                            outSource)) {
            return ZR_TRUE;
        }
        if (reader->normalizedPath[0] != '\0') {
            module_init_trace("binary metadata sidecar missing or invalid for '%s', falling back to direct .zro read",
                              reader->normalizedPath);
        }
    }

    if (io->read == ZR_NULL && io->remained == 0) {
        return ZR_FALSE;
    }

    directIo = *io;
    directIo.state = state;
    *outSource = ZrCore_Io_ReadSourceNew(&directIo);
    module_init_trace("direct binary metadata read result=%p", (void *)*outSource);
    return *outSource != ZR_NULL;
}

void ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(SZrGlobalState *global, SZrIoSource *source) {
    if (!module_init_binary_metadata_has_synthetic_marker(source)) {
        ZrCore_Io_ReadSourceFree(global, source);
        return;
    }

    module_init_binary_metadata_release_synthetic_source(
        global,
        (SZrBinaryMetadataSyntheticSource *)((TZrBytePtr)source -
                                            offsetof(SZrBinaryMetadataSyntheticSource, source)));
}

static SZrParserModuleInitCache *module_init_get_cache(SZrGlobalState *global, TZrBool createIfMissing) {
    SZrState *state;
    SZrParserModuleInitCache *cache;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    cache = (SZrParserModuleInitCache *)global->parserModuleInitState;
    if (cache != ZR_NULL || !createIfMissing) {
        return cache;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    cache = (SZrParserModuleInitCache *)ZrCore_Memory_RawMallocWithType(global,
                                                                        sizeof(SZrParserModuleInitCache),
                                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (cache == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(cache, 0, sizeof(*cache));
    ZrCore_Array_Init(state,
                      &cache->summaries,
                      sizeof(SZrParserModuleInitSummary),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    global->parserModuleInitState = cache;
    global->parserModuleInitStateCleanup = ZrParser_ModuleInitAnalysis_GlobalCleanup;
    return cache;
}

static SZrParserModuleInitSummary *module_init_find_summary_mutable(SZrGlobalState *global, SZrString *moduleName) {
    SZrParserModuleInitCache *cache;
    TZrSize index;

    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    cache = module_init_get_cache(global, ZR_FALSE);
    if (cache == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cache->summaries.length; ++index) {
        SZrParserModuleInitSummary *summary =
                (SZrParserModuleInitSummary *)ZrCore_Array_Get(&cache->summaries, index);
        if (summary != ZR_NULL &&
            summary->moduleName != ZR_NULL &&
            ZrCore_String_Equal(summary->moduleName, moduleName)) {
            return summary;
        }
    }

    return ZR_NULL;
}

static SZrParserModuleInitSummary *module_init_find_summary_by_ast_mutable(SZrGlobalState *global,
                                                                           const SZrAstNode *ast) {
    SZrParserModuleInitCache *cache;
    TZrSize index;

    if (global == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    cache = module_init_get_cache(global, ZR_FALSE);
    if (cache == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cache->summaries.length; ++index) {
        SZrParserModuleInitSummary *summary =
                (SZrParserModuleInitSummary *)ZrCore_Array_Get(&cache->summaries, index);
        if (summary != ZR_NULL && summary->astIdentity == ast) {
            return summary;
        }
    }

    return ZR_NULL;
}

static SZrParserModuleInitSummary *module_init_context_summary_mutable(SZrParserInitAnalysisContext *context) {
    if (context == ZR_NULL || context->cs == ZR_NULL || context->cs->state == ZR_NULL ||
        context->cs->state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (context->moduleName == ZR_NULL) {
        return context->summary;
    }

    context->summary = module_init_find_summary_mutable(context->cs->state->global, context->moduleName);
    return context->summary;
}

static void module_init_free_summary(SZrGlobalState *global, SZrParserModuleInitSummary *summary) {
    SZrState *state;
    TZrSize index;

    if (global == ZR_NULL || summary == ZR_NULL) {
        return;
    }

    state = global->mainThreadState;
    if (summary->exports.isValid && summary->exports.head != ZR_NULL) {
        for (index = 0; index < summary->exports.length; ++index) {
            SZrModuleInitExportInfo *info = (SZrModuleInitExportInfo *)ZrCore_Array_Get(&summary->exports, index);
            if (info != ZR_NULL && info->parameterTypes != ZR_NULL && info->parameterCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              info->parameterTypes,
                                              sizeof(SZrFunctionTypedTypeRef) * info->parameterCount,
                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
        }
        if (state != ZR_NULL) {
            ZrCore_Array_Free(state, &summary->exports);
        }
    }

    if (summary->bindings.isValid && summary->bindings.head != ZR_NULL && state != ZR_NULL) {
        ZrCore_Array_Free(state, &summary->bindings);
    }
    if (summary->staticImports.isValid && summary->staticImports.head != ZR_NULL && state != ZR_NULL) {
        ZrCore_Array_Free(state, &summary->staticImports);
    }
    if (summary->entryEffects.isValid && summary->entryEffects.head != ZR_NULL && state != ZR_NULL) {
        ZrCore_Array_Free(state, &summary->entryEffects);
    }
    if (summary->exportedCallableSummaries.isValid && summary->exportedCallableSummaries.head != ZR_NULL) {
        for (index = 0; index < summary->exportedCallableSummaries.length; ++index) {
            SZrModuleInitCallableSummary *callable =
                    (SZrModuleInitCallableSummary *)ZrCore_Array_Get(&summary->exportedCallableSummaries, index);
            if (callable != ZR_NULL && callable->effects != ZR_NULL && callable->effectCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              callable->effects,
                                              sizeof(SZrFunctionModuleEffect) * callable->effectCount,
                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
        }
        if (state != ZR_NULL) {
            ZrCore_Array_Free(state, &summary->exportedCallableSummaries);
        }
    }
}

static TZrBool module_init_reset_summary(SZrState *state,
                                         SZrParserModuleInitSummary *summary,
                                         SZrString *moduleName,
                                         const SZrAstNode *astIdentity) {
    if (state == ZR_NULL || state->global == ZR_NULL || summary == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    module_init_free_summary(state->global, summary);
    ZrCore_Memory_RawSet(summary, 0, sizeof(*summary));
    summary->moduleName = moduleName;
    summary->astIdentity = astIdentity;
    summary->state = ZR_PARSER_MODULE_INIT_SUMMARY_BUILDING;
    ZrCore_Array_Construct(&summary->staticImports);
    ZrCore_Array_Construct(&summary->exports);
    ZrCore_Array_Construct(&summary->bindings);
    ZrCore_Array_Construct(&summary->entryEffects);
    ZrCore_Array_Construct(&summary->exportedCallableSummaries);
    return ZR_TRUE;
}

static void module_init_summary_set_error(SZrParserModuleInitSummary *summary,
                                          const SZrFileRange *location,
                                          const TZrChar *message) {
    if (summary == ZR_NULL) {
        return;
    }

    summary->state = ZR_PARSER_MODULE_INIT_SUMMARY_FAILED;
    if (location != ZR_NULL) {
        summary->errorLocation = *location;
    } else {
        ZrCore_Memory_RawSet(&summary->errorLocation, 0, sizeof(summary->errorLocation));
    }

    if (message == ZR_NULL) {
        summary->errorMessage[0] = '\0';
        return;
    }

    snprintf(summary->errorMessage, sizeof(summary->errorMessage), "%s", message);
}

static TZrBool module_init_array_contains_string(const SZrArray *array, SZrString *value) {
    TZrSize index;

    if (array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < array->length; ++index) {
        SZrString **entry = (SZrString **)ZrCore_Array_Get((SZrArray *)array, index);
        if (entry != ZR_NULL && *entry != ZR_NULL && ZrCore_String_Equal(*entry, value)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void module_init_add_static_import(SZrState *state,
                                          SZrParserModuleInitSummary *summary,
                                          SZrString *moduleName) {
    if (state == ZR_NULL || summary == ZR_NULL || moduleName == ZR_NULL) {
        return;
    }

    if (!summary->staticImports.isValid) {
        ZrCore_Array_Init(state, &summary->staticImports, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    if (!module_init_array_contains_string(&summary->staticImports, moduleName)) {
        ZrCore_Array_Push(state, &summary->staticImports, &moduleName);
    }
}

static TZrBool module_init_find_export(const SZrParserModuleInitSummary *summary,
                                       SZrString *name,
                                       ZR_OUT const SZrModuleInitExportInfo **outInfo) {
    TZrSize index;

    if (outInfo != ZR_NULL) {
        *outInfo = ZR_NULL;
    }
    if (summary == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < summary->exports.length; ++index) {
        const SZrModuleInitExportInfo *info =
                (const SZrModuleInitExportInfo *)ZrCore_Array_Get((SZrArray *)&summary->exports, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, name)) {
            if (outInfo != ZR_NULL) {
                *outInfo = info;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void module_init_upsert_binding_info(SZrState *state,
                                            SZrParserModuleInitSummary *summary,
                                            SZrString *name,
                                            EZrParserInitBindingKind kind,
                                            SZrString *moduleName,
                                            SZrString *symbolName,
                                            EZrModuleExportKind exportKind,
                                            EZrModuleExportReadiness readiness,
                                            TZrUInt32 callableChildIndex) {
    TZrSize index;

    if (state == ZR_NULL || summary == ZR_NULL || name == ZR_NULL) {
        return;
    }

    if (!summary->bindings.isValid) {
        ZrCore_Array_Init(state, &summary->bindings, sizeof(SZrModuleInitBindingInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    for (index = 0; index < summary->bindings.length; ++index) {
        SZrModuleInitBindingInfo *info =
                (SZrModuleInitBindingInfo *)ZrCore_Array_Get(&summary->bindings, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, name)) {
            info->kind = (TZrUInt8)kind;
            info->moduleName = moduleName;
            info->symbolName = symbolName;
            info->exportKind = (TZrUInt8)exportKind;
            info->readiness = (TZrUInt8)readiness;
            info->callableChildIndex = callableChildIndex;
            return;
        }
    }

    {
        SZrModuleInitBindingInfo info;
        ZrCore_Memory_RawSet(&info, 0, sizeof(info));
        info.name = name;
        info.kind = (TZrUInt8)kind;
        info.moduleName = moduleName;
        info.symbolName = symbolName;
        info.exportKind = (TZrUInt8)exportKind;
        info.readiness = (TZrUInt8)readiness;
        info.callableChildIndex = callableChildIndex;
        ZrCore_Array_Push(state, &summary->bindings, &info);
    }
}

static TZrBool module_init_copy_parameter_type_refs(SZrState *state,
                                                    SZrAstNodeArray *params,
                                                    SZrFunctionTypedTypeRef **outTypes,
                                                    TZrUInt32 *outCount) {
    TZrUInt32 parameterCount = 0;
    TZrUInt32 index;
    SZrFunctionTypedTypeRef *types;

    if (outTypes != ZR_NULL) {
        *outTypes = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (state == ZR_NULL || params == ZR_NULL || params->count == 0 || outTypes == ZR_NULL || outCount == ZR_NULL) {
        return ZR_TRUE;
    }

    parameterCount = (TZrUInt32)params->count;
    types = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                       sizeof(SZrFunctionTypedTypeRef) * parameterCount,
                                                                       ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (types == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < parameterCount; ++index) {
        SZrAstNode *paramNode = params->nodes[index];
        ZrCore_Memory_RawSet(&types[index], 0, sizeof(types[index]));
        types[index].baseType = ZR_VALUE_TYPE_OBJECT;
        types[index].elementBaseType = ZR_VALUE_TYPE_OBJECT;
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            module_init_fill_type_ref_from_type_node(&types[index], paramNode->data.parameter.typeInfo);
        }
    }

    *outTypes = types;
    *outCount = parameterCount;
    return ZR_TRUE;
}

static TZrBool module_init_add_export_info(SZrState *state,
                                           SZrParserModuleInitSummary *summary,
                                           const SZrModuleInitExportInfo *info) {
    if (state == ZR_NULL || summary == ZR_NULL || info == ZR_NULL || info->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!summary->exports.isValid) {
        ZrCore_Array_Init(state, &summary->exports, sizeof(SZrModuleInitExportInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    if (module_init_find_export(summary, info->name, ZR_NULL)) {
        return ZR_TRUE;
    }

    ZrCore_Array_Push(state, &summary->exports, (TZrPtr)info);
    return ZR_TRUE;
}

static void module_init_collect_static_imports(SZrState *state,
                                               SZrParserModuleInitSummary *summary,
                                               SZrAstNode *node) {
    TZrSize index;

    if (state == ZR_NULL || summary == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (index = 0; index < node->data.script.statements->count; ++index) {
                    module_init_collect_static_imports(state, summary, node->data.script.statements->nodes[index]);
                }
            }
            return;
        case ZR_AST_IMPORT_EXPRESSION:
            if (node->data.importExpression.modulePath != ZR_NULL &&
                node->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                node->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL) {
                module_init_add_static_import(state,
                                              summary,
                                              node->data.importExpression.modulePath->data.stringLiteral.value);
            }
            return;
        default:
            break;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.primaryExpression.property);
        if (node->data.primaryExpression.members != ZR_NULL) {
            for (index = 0; index < node->data.primaryExpression.members->count; ++index) {
                module_init_collect_static_imports(state, summary, node->data.primaryExpression.members->nodes[index]);
            }
        }
        return;
    }

    if (node->type == ZR_AST_FUNCTION_CALL) {
        if (node->data.functionCall.args != ZR_NULL) {
            for (index = 0; index < node->data.functionCall.args->count; ++index) {
                module_init_collect_static_imports(state, summary, node->data.functionCall.args->nodes[index]);
            }
        }
        return;
    }

    if (node->type == ZR_AST_MEMBER_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.memberExpression.property);
        return;
    }

    if (node->type == ZR_AST_VARIABLE_DECLARATION) {
        module_init_collect_static_imports(state, summary, node->data.variableDeclaration.value);
        return;
    }

    if (node->type == ZR_AST_EXPRESSION_STATEMENT) {
        module_init_collect_static_imports(state, summary, node->data.expressionStatement.expr);
        return;
    }

    if (node->type == ZR_AST_BLOCK) {
        if (node->data.block.body != ZR_NULL) {
            for (index = 0; index < node->data.block.body->count; ++index) {
                module_init_collect_static_imports(state, summary, node->data.block.body->nodes[index]);
            }
        }
        return;
    }

    if (node->type == ZR_AST_RETURN_STATEMENT) {
        module_init_collect_static_imports(state, summary, node->data.returnStatement.expr);
        return;
    }

    if (node->type == ZR_AST_THROW_STATEMENT) {
        module_init_collect_static_imports(state, summary, node->data.throwStatement.expr);
        return;
    }

    if (node->type == ZR_AST_IF_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.ifExpression.condition);
        module_init_collect_static_imports(state, summary, node->data.ifExpression.thenExpr);
        module_init_collect_static_imports(state, summary, node->data.ifExpression.elseExpr);
        return;
    }

    if (node->type == ZR_AST_WHILE_LOOP) {
        module_init_collect_static_imports(state, summary, node->data.whileLoop.cond);
        module_init_collect_static_imports(state, summary, node->data.whileLoop.block);
        return;
    }

    if (node->type == ZR_AST_FOR_LOOP) {
        module_init_collect_static_imports(state, summary, node->data.forLoop.init);
        module_init_collect_static_imports(state, summary, node->data.forLoop.cond);
        module_init_collect_static_imports(state, summary, node->data.forLoop.step);
        module_init_collect_static_imports(state, summary, node->data.forLoop.block);
        return;
    }

    if (node->type == ZR_AST_FOREACH_LOOP) {
        module_init_collect_static_imports(state, summary, node->data.foreachLoop.expr);
        module_init_collect_static_imports(state, summary, node->data.foreachLoop.block);
        return;
    }

    if (node->type == ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        module_init_collect_static_imports(state, summary, node->data.tryCatchFinallyStatement.block);
        if (node->data.tryCatchFinallyStatement.catchClauses != ZR_NULL) {
            for (index = 0; index < node->data.tryCatchFinallyStatement.catchClauses->count; ++index) {
                module_init_collect_static_imports(state,
                                                   summary,
                                                   node->data.tryCatchFinallyStatement.catchClauses->nodes[index]);
            }
        }
        module_init_collect_static_imports(state, summary, node->data.tryCatchFinallyStatement.finallyBlock);
        return;
    }

    if (node->type == ZR_AST_CATCH_CLAUSE) {
        module_init_collect_static_imports(state, summary, node->data.catchClause.block);
        return;
    }

    if (node->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.assignmentExpression.left);
        module_init_collect_static_imports(state, summary, node->data.assignmentExpression.right);
        return;
    }

    if (node->type == ZR_AST_BINARY_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.binaryExpression.left);
        module_init_collect_static_imports(state, summary, node->data.binaryExpression.right);
        return;
    }

    if (node->type == ZR_AST_LOGICAL_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.logicalExpression.left);
        module_init_collect_static_imports(state, summary, node->data.logicalExpression.right);
        return;
    }

    if (node->type == ZR_AST_CONDITIONAL_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.conditionalExpression.test);
        module_init_collect_static_imports(state, summary, node->data.conditionalExpression.consequent);
        module_init_collect_static_imports(state, summary, node->data.conditionalExpression.alternate);
        return;
    }

    if (node->type == ZR_AST_UNARY_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.unaryExpression.argument);
        return;
    }

    if (node->type == ZR_AST_TYPE_CAST_EXPRESSION) {
        module_init_collect_static_imports(state, summary, node->data.typeCastExpression.expression);
        return;
    }

    if (node->type == ZR_AST_ARRAY_LITERAL && node->data.arrayLiteral.elements != ZR_NULL) {
        for (index = 0; index < node->data.arrayLiteral.elements->count; ++index) {
            module_init_collect_static_imports(state, summary, node->data.arrayLiteral.elements->nodes[index]);
        }
        return;
    }

    if (node->type == ZR_AST_OBJECT_LITERAL && node->data.objectLiteral.properties != ZR_NULL) {
        for (index = 0; index < node->data.objectLiteral.properties->count; ++index) {
            module_init_collect_static_imports(state, summary, node->data.objectLiteral.properties->nodes[index]);
        }
        return;
    }

    if (node->type == ZR_AST_KEY_VALUE_PAIR) {
        module_init_collect_static_imports(state, summary, node->data.keyValuePair.key);
        module_init_collect_static_imports(state, summary, node->data.keyValuePair.value);
        return;
    }
}

static TZrBool module_init_prescan_source_summary(SZrState *state,
                                                  SZrParserModuleInitSummary *summary,
                                                  SZrAstNode *ast) {
    TZrSize index;

    if (state == ZR_NULL || summary == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    if (!summary->staticImports.isValid) {
        ZrCore_Array_Init(state, &summary->staticImports, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->exports.isValid) {
        ZrCore_Array_Init(state, &summary->exports, sizeof(SZrModuleInitExportInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->bindings.isValid) {
        ZrCore_Array_Init(state, &summary->bindings, sizeof(SZrModuleInitBindingInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->entryEffects.isValid) {
        ZrCore_Array_Init(state, &summary->entryEffects, sizeof(SZrFunctionModuleEffect), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->exportedCallableSummaries.isValid) {
        ZrCore_Array_Init(state,
                          &summary->exportedCallableSummaries,
                          sizeof(SZrModuleInitCallableSummary),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    module_init_collect_static_imports(state, summary, ast);

    if (ast->data.script.statements == ZR_NULL) {
        summary->hasPrescan = ZR_TRUE;
        return ZR_TRUE;
    }

    for (index = 0; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement == ZR_NULL) {
            continue;
        }

        if (statement->type == ZR_AST_FUNCTION_DECLARATION &&
            statement->data.functionDeclaration.name != ZR_NULL &&
            statement->data.functionDeclaration.name->name != ZR_NULL) {
            SZrModuleInitExportInfo exportInfo;
            ZrCore_Memory_RawSet(&exportInfo, 0, sizeof(exportInfo));
            exportInfo.name = statement->data.functionDeclaration.name->name;
            exportInfo.accessModifier = ZR_ACCESS_PUBLIC;
            exportInfo.exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
            exportInfo.readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
            exportInfo.symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
            exportInfo.callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
            module_init_fill_type_ref_from_type_node(&exportInfo.valueType, statement->data.functionDeclaration.returnType);
            exportInfo.lineInSourceStart = (TZrUInt32)statement->location.start.line;
            exportInfo.columnInSourceStart = (TZrUInt32)statement->location.start.column;
            exportInfo.lineInSourceEnd = (TZrUInt32)statement->location.end.line;
            exportInfo.columnInSourceEnd = (TZrUInt32)statement->location.end.column;
            if (!module_init_copy_parameter_type_refs(state,
                                                      statement->data.functionDeclaration.params,
                                                      &exportInfo.parameterTypes,
                                                      &exportInfo.parameterCount) ||
                !module_init_add_export_info(state, summary, &exportInfo)) {
                if (exportInfo.parameterTypes != ZR_NULL && exportInfo.parameterCount > 0) {
                    ZrCore_Memory_RawFreeWithType(state->global,
                                                  exportInfo.parameterTypes,
                                                  sizeof(SZrFunctionTypedTypeRef) * exportInfo.parameterCount,
                                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                }
                return ZR_FALSE;
            }
            module_init_upsert_binding_info(state,
                                            summary,
                                            exportInfo.name,
                                            ZR_PARSER_INIT_BINDING_TOP_LEVEL_CALLABLE,
                                            summary->moduleName,
                                            ZR_NULL,
                                            ZR_MODULE_EXPORT_KIND_FUNCTION,
                                            ZR_MODULE_EXPORT_READY_DECLARATION,
                                            ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
            continue;
        }

        if (statement->type == ZR_AST_VARIABLE_DECLARATION &&
            statement->data.variableDeclaration.pattern != ZR_NULL) {
            SZrVariableDeclaration *declaration = &statement->data.variableDeclaration;

            if (declaration->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
                declaration->pattern->data.identifier.name != ZR_NULL) {
                SZrString *bindingName = declaration->pattern->data.identifier.name;
                EZrParserInitBindingKind bindingKind = ZR_PARSER_INIT_BINDING_TOP_LEVEL_ENTRY;
                SZrString *bindingModuleName = summary->moduleName;
                SZrString *bindingSymbolName = ZR_NULL;

                if (declaration->value != ZR_NULL &&
                    declaration->value->type == ZR_AST_IMPORT_EXPRESSION &&
                    declaration->value->data.importExpression.modulePath != ZR_NULL &&
                    declaration->value->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                    declaration->value->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL) {
                    bindingKind = ZR_PARSER_INIT_BINDING_MODULE_ALIAS;
                    bindingModuleName = declaration->value->data.importExpression.modulePath->data.stringLiteral.value;
                }

                module_init_upsert_binding_info(state,
                                                summary,
                                                bindingName,
                                                bindingKind,
                                                bindingModuleName,
                                                bindingSymbolName,
                                                ZR_MODULE_EXPORT_KIND_VALUE,
                                                ZR_MODULE_EXPORT_READY_ENTRY,
                                                ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);

                if (declaration->accessModifier == ZR_ACCESS_PUBLIC ||
                    declaration->accessModifier == ZR_ACCESS_PROTECTED) {
                    SZrModuleInitExportInfo exportInfo;
                    ZrCore_Memory_RawSet(&exportInfo, 0, sizeof(exportInfo));
                    exportInfo.name = bindingName;
                    exportInfo.accessModifier = (TZrUInt8)declaration->accessModifier;
                    exportInfo.exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
                    exportInfo.readiness = ZR_MODULE_EXPORT_READY_ENTRY;
                    exportInfo.symbolKind = ZR_FUNCTION_TYPED_SYMBOL_VARIABLE;
                    exportInfo.callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
                    module_init_fill_type_ref_from_type_node(&exportInfo.valueType, declaration->typeInfo);
                    exportInfo.lineInSourceStart = (TZrUInt32)statement->location.start.line;
                    exportInfo.columnInSourceStart = (TZrUInt32)statement->location.start.column;
                    exportInfo.lineInSourceEnd = (TZrUInt32)statement->location.end.line;
                    exportInfo.columnInSourceEnd = (TZrUInt32)statement->location.end.column;
                    if (!module_init_add_export_info(state, summary, &exportInfo)) {
                        return ZR_FALSE;
                    }
                }
            } else if ((declaration->pattern->type == ZR_AST_DESTRUCTURING_OBJECT ||
                        declaration->pattern->type == ZR_AST_DESTRUCTURING_ARRAY) &&
                       declaration->value != ZR_NULL) {
                SZrAstNodeArray *keys = declaration->pattern->type == ZR_AST_DESTRUCTURING_OBJECT
                                                ? declaration->pattern->data.destructuringObject.keys
                                                : declaration->pattern->data.destructuringArray.keys;
                SZrString *importModuleName = ZR_NULL;
                EZrParserInitBindingKind bindingKind = ZR_PARSER_INIT_BINDING_TOP_LEVEL_ENTRY;

                if (declaration->value->type == ZR_AST_IMPORT_EXPRESSION &&
                    declaration->value->data.importExpression.modulePath != ZR_NULL &&
                    declaration->value->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                    declaration->value->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL) {
                    importModuleName = declaration->value->data.importExpression.modulePath->data.stringLiteral.value;
                    bindingKind = ZR_PARSER_INIT_BINDING_IMPORTED_SYMBOL;
                }

                for (TZrSize keyIndex = 0; keys != ZR_NULL && keyIndex < keys->count; ++keyIndex) {
                    SZrAstNode *keyNode = keys->nodes[keyIndex];
                    if (keyNode == ZR_NULL || keyNode->type != ZR_AST_IDENTIFIER_LITERAL ||
                        keyNode->data.identifier.name == ZR_NULL) {
                        continue;
                    }

                    module_init_upsert_binding_info(state,
                                                    summary,
                                                    keyNode->data.identifier.name,
                                                    bindingKind,
                                                    bindingKind == ZR_PARSER_INIT_BINDING_IMPORTED_SYMBOL
                                                            ? importModuleName
                                                            : summary->moduleName,
                                                    bindingKind == ZR_PARSER_INIT_BINDING_IMPORTED_SYMBOL
                                                            ? keyNode->data.identifier.name
                                                            : ZR_NULL,
                                                    ZR_MODULE_EXPORT_KIND_VALUE,
                                                    ZR_MODULE_EXPORT_READY_ENTRY,
                                                    ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
                }
            }

            continue;
        }

        if (statement->type == ZR_AST_STRUCT_DECLARATION ||
            statement->type == ZR_AST_CLASS_DECLARATION ||
            statement->type == ZR_AST_INTERFACE_DECLARATION ||
            statement->type == ZR_AST_ENUM_DECLARATION) {
            SZrString *typeName = ZR_NULL;
            EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;
            EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
            SZrModuleInitExportInfo exportInfo;

            if (statement->type == ZR_AST_STRUCT_DECLARATION) {
                typeName = statement->data.structDeclaration.name != ZR_NULL ? statement->data.structDeclaration.name->name : ZR_NULL;
                accessModifier = statement->data.structDeclaration.accessModifier;
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            } else if (statement->type == ZR_AST_CLASS_DECLARATION) {
                typeName = statement->data.classDeclaration.name != ZR_NULL ? statement->data.classDeclaration.name->name : ZR_NULL;
                accessModifier = statement->data.classDeclaration.accessModifier;
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            } else if (statement->type == ZR_AST_INTERFACE_DECLARATION) {
                typeName = statement->data.interfaceDeclaration.name != ZR_NULL ? statement->data.interfaceDeclaration.name->name : ZR_NULL;
                accessModifier = statement->data.interfaceDeclaration.accessModifier;
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE;
            } else if (statement->type == ZR_AST_ENUM_DECLARATION) {
                typeName = statement->data.enumDeclaration.name != ZR_NULL ? statement->data.enumDeclaration.name->name : ZR_NULL;
                accessModifier = statement->data.enumDeclaration.accessModifier;
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
            }

            if (typeName == ZR_NULL) {
                continue;
            }

            module_init_upsert_binding_info(state,
                                            summary,
                                            typeName,
                                            ZR_PARSER_INIT_BINDING_TOP_LEVEL_TYPE,
                                            summary->moduleName,
                                            ZR_NULL,
                                            ZR_MODULE_EXPORT_KIND_TYPE,
                                            ZR_MODULE_EXPORT_READY_DECLARATION,
                                            ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);

            if (accessModifier != ZR_ACCESS_PUBLIC && accessModifier != ZR_ACCESS_PROTECTED) {
                continue;
            }

            ZrCore_Memory_RawSet(&exportInfo, 0, sizeof(exportInfo));
            exportInfo.name = typeName;
            exportInfo.accessModifier = (TZrUInt8)accessModifier;
            exportInfo.exportKind = ZR_MODULE_EXPORT_KIND_TYPE;
            exportInfo.readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
            exportInfo.prototypeType = (TZrUInt8)prototypeType;
            exportInfo.callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
            exportInfo.lineInSourceStart = (TZrUInt32)statement->location.start.line;
            exportInfo.columnInSourceStart = (TZrUInt32)statement->location.start.column;
            exportInfo.lineInSourceEnd = (TZrUInt32)statement->location.end.line;
            exportInfo.columnInSourceEnd = (TZrUInt32)statement->location.end.column;
            if (!module_init_add_export_info(state, summary, &exportInfo)) {
                return ZR_FALSE;
            }
        }
    }

    summary->hasPrescan = ZR_TRUE;
    return ZR_TRUE;
}

static void module_init_init_effect(SZrFunctionModuleEffect *effect,
                                    EZrModuleEntryEffectKind kind,
                                    SZrString *moduleName,
                                    SZrString *symbolName,
                                    EZrModuleExportKind exportKind,
                                    EZrModuleExportReadiness readiness,
                                    const SZrFileRange *location) {
    if (effect == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
    effect->kind = (TZrUInt8)kind;
    effect->exportKind = (TZrUInt8)exportKind;
    effect->readiness = (TZrUInt8)readiness;
    effect->moduleName = moduleName;
    effect->symbolName = symbolName;
    if (location != ZR_NULL) {
        effect->lineInSourceStart = (TZrUInt32)location->start.line;
        effect->columnInSourceStart = (TZrUInt32)location->start.column;
        effect->lineInSourceEnd = (TZrUInt32)location->end.line;
        effect->columnInSourceEnd = (TZrUInt32)location->end.column;
    }
}

static TZrBool module_init_effects_contain(const SZrArray *effects, const SZrFunctionModuleEffect *candidate) {
    TZrSize index;

    if (effects == ZR_NULL || candidate == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < effects->length; ++index) {
        const SZrFunctionModuleEffect *existing =
                (const SZrFunctionModuleEffect *)ZrCore_Array_Get((SZrArray *)effects, index);
        if (existing == ZR_NULL) {
            continue;
        }

        if (existing->kind == candidate->kind &&
            existing->exportKind == candidate->exportKind &&
            existing->readiness == candidate->readiness &&
            existing->moduleName == candidate->moduleName &&
            existing->symbolName == candidate->symbolName &&
            existing->lineInSourceStart == candidate->lineInSourceStart &&
            existing->columnInSourceStart == candidate->columnInSourceStart &&
            existing->lineInSourceEnd == candidate->lineInSourceEnd &&
            existing->columnInSourceEnd == candidate->columnInSourceEnd) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void module_init_append_effect(SZrState *state, SZrArray *effects, const SZrFunctionModuleEffect *effect) {
    if (state == ZR_NULL || effects == ZR_NULL || effect == ZR_NULL) {
        return;
    }

    if (!module_init_effects_contain(effects, effect)) {
        ZrCore_Array_Push(state, effects, (TZrPtr)effect);
    }
}

static SZrParserInitBinding *module_init_find_binding(SZrArray *bindings, SZrString *name) {
    TZrSize index;

    if (bindings == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = bindings->length; index > 0; --index) {
        SZrParserInitBinding *binding = (SZrParserInitBinding *)ZrCore_Array_Get(bindings, index - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }

    return ZR_NULL;
}

static void module_init_push_binding(SZrState *state,
                                     SZrArray *bindings,
                                     SZrString *name,
                                     EZrParserInitBindingKind kind,
                                     SZrString *moduleName,
                                     SZrString *symbolName,
                                     EZrModuleExportKind exportKind,
                                     EZrModuleExportReadiness readiness,
                                     TZrUInt32 callableIndex) {
    SZrParserInitBinding binding;

    if (state == ZR_NULL || bindings == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&binding, 0, sizeof(binding));
    binding.name = name;
    binding.kind = (TZrUInt8)kind;
    binding.moduleName = moduleName;
    binding.symbolName = symbolName;
    binding.exportKind = (TZrUInt8)exportKind;
    binding.readiness = (TZrUInt8)readiness;
    binding.callableIndex = callableIndex;
    ZrCore_Array_Push(state, bindings, &binding);
}

static const SZrModuleInitBindingInfo *module_init_find_binding_info(const SZrParserModuleInitSummary *summary,
                                                                     SZrString *name) {
    TZrSize index;

    if (summary == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < summary->bindings.length; ++index) {
        const SZrModuleInitBindingInfo *info =
                (const SZrModuleInitBindingInfo *)ZrCore_Array_Get((SZrArray *)&summary->bindings, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, name)) {
            return info;
        }
    }

    return ZR_NULL;
}

static void module_init_push_binding_from_info(SZrState *state,
                                               SZrArray *bindings,
                                               const SZrModuleInitBindingInfo *info) {
    if (state == ZR_NULL || bindings == ZR_NULL || info == ZR_NULL || info->name == ZR_NULL) {
        return;
    }

    module_init_push_binding(state,
                             bindings,
                             info->name,
                             (EZrParserInitBindingKind)info->kind,
                             info->moduleName,
                             info->symbolName,
                             (EZrModuleExportKind)info->exportKind,
                             (EZrModuleExportReadiness)info->readiness,
                             info->callableChildIndex);
}

static TZrBool module_init_resolve_import_export_info(SZrCompilerState *cs,
                                                      SZrString *moduleName,
                                                      SZrString *symbolName,
                                                      EZrModuleExportKind *outKind,
                                                      EZrModuleExportReadiness *outReadiness) {
    const SZrParserModuleInitSummary *summary;
    const SZrModuleInitExportInfo *info = ZR_NULL;

    if (outKind != ZR_NULL) {
        *outKind = ZR_MODULE_EXPORT_KIND_VALUE;
    }
    if (outReadiness != ZR_NULL) {
        *outReadiness = ZR_MODULE_EXPORT_READY_ENTRY;
    }

    if (cs == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, moduleName);
    if (summary == ZR_NULL) {
        if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, moduleName)) {
            return ZR_FALSE;
        }
        summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, moduleName);
    }

    if (summary == ZR_NULL || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED ||
        !module_init_find_export(summary, symbolName, &info) || info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outKind != ZR_NULL) {
        *outKind = (EZrModuleExportKind)info->exportKind;
    }
    if (outReadiness != ZR_NULL) {
        *outReadiness = (EZrModuleExportReadiness)info->readiness;
    }
    return ZR_TRUE;
}

static TZrBool module_init_lookup_callable_index(SZrParserInitAnalysisContext *context,
                                                 SZrString *name,
                                                 TZrUInt32 *outIndex) {
    TZrSize index;

    if (outIndex != ZR_NULL) {
        *outIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    }
    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < context->callables.length; ++index) {
        SZrParserInitCallableInfo *callable =
                (SZrParserInitCallableInfo *)ZrCore_Array_Get(&context->callables, index);
        if (callable != ZR_NULL && callable->name != ZR_NULL && ZrCore_String_Equal(callable->name, name)) {
            if (outIndex != ZR_NULL) {
                *outIndex = (TZrUInt32)index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

typedef enum EZrParserInitSubjectKind {
    ZR_PARSER_INIT_SUBJECT_UNKNOWN = 0,
    ZR_PARSER_INIT_SUBJECT_IMPORT_MODULE = 1,
    ZR_PARSER_INIT_SUBJECT_IMPORT_SYMBOL = 2,
    ZR_PARSER_INIT_SUBJECT_LOCAL_CALLABLE = 3
} EZrParserInitSubjectKind;

typedef struct SZrParserInitSubject {
    TZrUInt8 kind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    TZrUInt32 callableIndex;
    SZrString *moduleName;
    SZrString *symbolName;
} SZrParserInitSubject;

static TZrBool module_init_analyze_expression(SZrParserInitAnalysisContext *context,
                                              SZrAstNode *node,
                                              SZrArray *effects,
                                              ZR_OUT SZrParserInitSubject *outSubject);
static TZrBool module_init_analyze_statement(SZrParserInitAnalysisContext *context,
                                             SZrAstNode *node,
                                             SZrArray *effects);
static TZrBool module_init_expand_callable_effects(SZrParserInitAnalysisContext *context,
                                                   TZrUInt32 callableIndex,
                                                   SZrArray *effects);

static void module_init_subject_reset(SZrParserInitSubject *subject) {
    if (subject == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(subject, 0, sizeof(*subject));
    subject->kind = ZR_PARSER_INIT_SUBJECT_UNKNOWN;
    subject->callableIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
}

static void module_init_record_import_symbol_effect(SZrParserInitAnalysisContext *context,
                                                    SZrArray *effects,
                                                    SZrString *moduleName,
                                                    SZrString *symbolName,
                                                    EZrModuleEntryEffectKind defaultKind,
                                                    const SZrFileRange *location) {
    EZrModuleExportKind exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
    EZrModuleExportReadiness readiness = ZR_MODULE_EXPORT_READY_ENTRY;
    SZrFunctionModuleEffect effect;

    if (context == ZR_NULL || effects == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return;
    }

    if (!module_init_resolve_import_export_info(context->cs, moduleName, symbolName, &exportKind, &readiness)) {
        exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
        readiness = ZR_MODULE_EXPORT_READY_ENTRY;
    }

    if (defaultKind == ZR_MODULE_ENTRY_EFFECT_IMPORT_REF &&
        !(exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION || exportKind == ZR_MODULE_EXPORT_KIND_TYPE)) {
        defaultKind = ZR_MODULE_ENTRY_EFFECT_IMPORT_READ;
    }

    module_init_init_effect(&effect, defaultKind, moduleName, symbolName, exportKind, readiness, location);
    module_init_append_effect(context->cs->state, effects, &effect);
}

static void module_init_record_dynamic_unknown(SZrParserInitAnalysisContext *context,
                                               SZrArray *effects,
                                               const SZrFileRange *location) {
    SZrFunctionModuleEffect effect;

    if (context == ZR_NULL || effects == ZR_NULL) {
        return;
    }

    module_init_init_effect(&effect,
                            ZR_MODULE_ENTRY_EFFECT_DYNAMIC_UNKNOWN,
                            ZR_NULL,
                            ZR_NULL,
                            ZR_MODULE_EXPORT_KIND_VALUE,
                            ZR_MODULE_EXPORT_READY_ENTRY,
                            location);
    module_init_append_effect(context->cs->state, effects, &effect);
}

static void module_init_record_local_entry_binding_read(SZrParserInitAnalysisContext *context,
                                                        SZrArray *effects,
                                                        SZrString *name,
                                                        const SZrFileRange *location) {
    SZrFunctionModuleEffect effect;
    SZrParserModuleInitSummary *summary;

    if (context == ZR_NULL || effects == ZR_NULL || name == ZR_NULL) {
        return;
    }
    summary = module_init_context_summary_mutable(context);
    if (summary == ZR_NULL) {
        return;
    }

    module_init_init_effect(&effect,
                            ZR_MODULE_ENTRY_EFFECT_LOCAL_ENTRY_BINDING_READ,
                            summary->moduleName,
                            name,
                            ZR_MODULE_EXPORT_KIND_VALUE,
                            ZR_MODULE_EXPORT_READY_ENTRY,
                            location);
    module_init_append_effect(context->cs->state, effects, &effect);
}

static void module_init_register_import_binding_from_variable(SZrParserInitAnalysisContext *context, SZrAstNode *node) {
    SZrVariableDeclaration *declaration;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    declaration = &node->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL || declaration->value == ZR_NULL) {
        return;
    }

    if (declaration->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
        declaration->pattern->data.identifier.name != ZR_NULL &&
        declaration->value->type == ZR_AST_IMPORT_EXPRESSION &&
        declaration->value->data.importExpression.modulePath != ZR_NULL &&
        declaration->value->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
        declaration->value->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL) {
        module_init_push_binding(context->cs->state,
                                 &context->bindings,
                                 declaration->pattern->data.identifier.name,
                                 ZR_PARSER_INIT_BINDING_MODULE_ALIAS,
                                 declaration->value->data.importExpression.modulePath->data.stringLiteral.value,
                                 ZR_NULL,
                                 ZR_MODULE_EXPORT_KIND_VALUE,
                                 ZR_MODULE_EXPORT_READY_ENTRY,
                                 ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
        return;
    }

    if (declaration->pattern->type == ZR_AST_DESTRUCTURING_OBJECT &&
        declaration->value->type == ZR_AST_IMPORT_EXPRESSION &&
        declaration->value->data.importExpression.modulePath != ZR_NULL &&
        declaration->value->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
        declaration->value->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL &&
        declaration->pattern->data.destructuringObject.keys != ZR_NULL) {
        TZrSize index;
        SZrString *moduleName = declaration->value->data.importExpression.modulePath->data.stringLiteral.value;
        for (index = 0; index < declaration->pattern->data.destructuringObject.keys->count; ++index) {
            SZrAstNode *keyNode = declaration->pattern->data.destructuringObject.keys->nodes[index];
            if (keyNode != ZR_NULL && keyNode->type == ZR_AST_IDENTIFIER_LITERAL &&
                keyNode->data.identifier.name != ZR_NULL) {
                module_init_push_binding(context->cs->state,
                                         &context->bindings,
                                         keyNode->data.identifier.name,
                                         ZR_PARSER_INIT_BINDING_IMPORTED_SYMBOL,
                                         moduleName,
                                         keyNode->data.identifier.name,
                                         ZR_MODULE_EXPORT_KIND_VALUE,
                                         ZR_MODULE_EXPORT_READY_ENTRY,
                                         ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
            }
        }
        return;
    }

    if (declaration->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
        declaration->pattern->data.identifier.name != ZR_NULL) {
        SZrParserModuleInitSummary *summary = module_init_context_summary_mutable(context);
        const SZrModuleInitBindingInfo *bindingInfo =
                module_init_find_binding_info(summary, declaration->pattern->data.identifier.name);
        EZrParserInitBindingKind kind = ZR_PARSER_INIT_BINDING_LOCAL;
        EZrModuleExportKind exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
        EZrModuleExportReadiness readiness = ZR_MODULE_EXPORT_READY_ENTRY;
        SZrString *moduleName = summary != ZR_NULL ? summary->moduleName : ZR_NULL;
        SZrString *symbolName = ZR_NULL;
        TZrUInt32 callableIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;

        if (summary == ZR_NULL) {
            return;
        }

        if (bindingInfo != ZR_NULL) {
            kind = (EZrParserInitBindingKind)bindingInfo->kind;
            exportKind = (EZrModuleExportKind)bindingInfo->exportKind;
            readiness = (EZrModuleExportReadiness)bindingInfo->readiness;
            moduleName = bindingInfo->moduleName;
            symbolName = bindingInfo->symbolName;
            callableIndex = bindingInfo->callableChildIndex;
        }

        module_init_push_binding(context->cs->state,
                                 &context->bindings,
                                 declaration->pattern->data.identifier.name,
                                 kind,
                                 moduleName,
                                 symbolName,
                                 exportKind,
                                 readiness,
                                 callableIndex);
    }
}

static TZrBool module_init_analyze_primary_expression(SZrParserInitAnalysisContext *context,
                                                      SZrAstNode *node,
                                                      SZrArray *effects,
                                                      ZR_OUT SZrParserInitSubject *outSubject) {
    TZrSize index;
    SZrParserInitSubject current;

    module_init_subject_reset(outSubject);
    module_init_subject_reset(&current);
    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_TRUE;
    }

    if (!module_init_analyze_expression(context, node->data.primaryExpression.property, effects, &current)) {
        return ZR_FALSE;
    }

    if (node->data.primaryExpression.members == ZR_NULL || node->data.primaryExpression.members->count == 0) {
        if (outSubject != ZR_NULL) {
            *outSubject = current;
        }
        return ZR_TRUE;
    }

    for (index = 0; index < node->data.primaryExpression.members->count; ++index) {
        SZrAstNode *memberNode = node->data.primaryExpression.members->nodes[index];
        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
            if (memberNode->data.memberExpression.computed) {
                (void)module_init_analyze_expression(context, memberNode->data.memberExpression.property, effects, ZR_NULL);
                if (current.kind == ZR_PARSER_INIT_SUBJECT_IMPORT_MODULE ||
                    current.kind == ZR_PARSER_INIT_SUBJECT_IMPORT_SYMBOL) {
                    module_init_record_dynamic_unknown(context, effects, &node->location);
                }
                module_init_subject_reset(&current);
                continue;
            }

            if (current.kind == ZR_PARSER_INIT_SUBJECT_IMPORT_MODULE &&
                memberNode->data.memberExpression.property != ZR_NULL &&
                memberNode->data.memberExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
                memberNode->data.memberExpression.property->data.identifier.name != ZR_NULL) {
                current.kind = ZR_PARSER_INIT_SUBJECT_IMPORT_SYMBOL;
                current.symbolName = memberNode->data.memberExpression.property->data.identifier.name;
                continue;
            }

            (void)module_init_analyze_expression(context, memberNode->data.memberExpression.property, effects, ZR_NULL);
            module_init_subject_reset(&current);
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            SZrFunctionCall *call = &memberNode->data.functionCall;
            TZrSize argIndex;
            for (argIndex = 0; call->args != ZR_NULL && argIndex < call->args->count; ++argIndex) {
                (void)module_init_analyze_expression(context, call->args->nodes[argIndex], effects, ZR_NULL);
            }

            if (current.kind == ZR_PARSER_INIT_SUBJECT_IMPORT_SYMBOL &&
                current.moduleName != ZR_NULL &&
                current.symbolName != ZR_NULL) {
                module_init_record_import_symbol_effect(context,
                                                        effects,
                                                        current.moduleName,
                                                        current.symbolName,
                                                        ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL,
                                                        &node->location);
            } else if (current.kind == ZR_PARSER_INIT_SUBJECT_LOCAL_CALLABLE &&
                       current.callableIndex != ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE) {
                if (!module_init_expand_callable_effects(context, current.callableIndex, effects)) {
                    return ZR_FALSE;
                }
            }

            module_init_subject_reset(&current);
            continue;
        }

        (void)module_init_analyze_expression(context, memberNode, effects, ZR_NULL);
        module_init_subject_reset(&current);
    }

    if (current.kind == ZR_PARSER_INIT_SUBJECT_IMPORT_SYMBOL &&
        current.moduleName != ZR_NULL &&
        current.symbolName != ZR_NULL) {
        module_init_record_import_symbol_effect(context,
                                                effects,
                                                current.moduleName,
                                                current.symbolName,
                                                ZR_MODULE_ENTRY_EFFECT_IMPORT_REF,
                                                &node->location);
        module_init_subject_reset(&current);
    }

    if (outSubject != ZR_NULL) {
        *outSubject = current;
    }
    return ZR_TRUE;
}

static TZrBool module_init_analyze_expression(SZrParserInitAnalysisContext *context,
                                              SZrAstNode *node,
                                              SZrArray *effects,
                                              ZR_OUT SZrParserInitSubject *outSubject) {
    module_init_subject_reset(outSubject);

    if (context == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_PRIMARY_EXPRESSION:
            return module_init_analyze_primary_expression(context, node, effects, outSubject);
        case ZR_AST_IMPORT_EXPRESSION:
            if (node->data.importExpression.modulePath != ZR_NULL &&
                node->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                node->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL &&
                outSubject != ZR_NULL) {
                outSubject->kind = ZR_PARSER_INIT_SUBJECT_IMPORT_MODULE;
                outSubject->moduleName = node->data.importExpression.modulePath->data.stringLiteral.value;
            }
            return ZR_TRUE;
        case ZR_AST_IDENTIFIER_LITERAL: {
            SZrParserInitBinding *binding = module_init_find_binding(&context->bindings, node->data.identifier.name);
            if (binding == ZR_NULL) {
                return ZR_TRUE;
            }

            if (binding->kind == ZR_PARSER_INIT_BINDING_MODULE_ALIAS && outSubject != ZR_NULL) {
                outSubject->kind = ZR_PARSER_INIT_SUBJECT_IMPORT_MODULE;
                outSubject->moduleName = binding->moduleName;
                return ZR_TRUE;
            }

            if (binding->kind == ZR_PARSER_INIT_BINDING_IMPORTED_SYMBOL && outSubject != ZR_NULL) {
                outSubject->kind = ZR_PARSER_INIT_SUBJECT_IMPORT_SYMBOL;
                outSubject->moduleName = binding->moduleName;
                outSubject->symbolName = binding->symbolName;
                return ZR_TRUE;
            }

            if (binding->kind == ZR_PARSER_INIT_BINDING_TOP_LEVEL_CALLABLE && outSubject != ZR_NULL) {
                if (!module_init_lookup_callable_index(context, binding->name, &outSubject->callableIndex)) {
                    outSubject->callableIndex = binding->callableIndex;
                }
                outSubject->kind = ZR_PARSER_INIT_SUBJECT_LOCAL_CALLABLE;
                return ZR_TRUE;
            }

            if (context->inCallableBody && binding->kind == ZR_PARSER_INIT_BINDING_TOP_LEVEL_ENTRY) {
                module_init_record_local_entry_binding_read(context, effects, binding->name, &node->location);
            }
            return ZR_TRUE;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (!module_init_analyze_expression(context, node->data.assignmentExpression.left, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_expression(context, node->data.assignmentExpression.right, effects, ZR_NULL);
        case ZR_AST_BINARY_EXPRESSION:
            if (!module_init_analyze_expression(context, node->data.binaryExpression.left, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_expression(context, node->data.binaryExpression.right, effects, ZR_NULL);
        case ZR_AST_LOGICAL_EXPRESSION:
            if (!module_init_analyze_expression(context, node->data.logicalExpression.left, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_expression(context, node->data.logicalExpression.right, effects, ZR_NULL);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            if (!module_init_analyze_expression(context, node->data.conditionalExpression.test, effects, ZR_NULL) ||
                !module_init_analyze_expression(context, node->data.conditionalExpression.consequent, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_expression(context, node->data.conditionalExpression.alternate, effects, ZR_NULL);
        case ZR_AST_UNARY_EXPRESSION:
            return module_init_analyze_expression(context, node->data.unaryExpression.argument, effects, ZR_NULL);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return module_init_analyze_expression(context, node->data.typeCastExpression.expression, effects, ZR_NULL);
        case ZR_AST_FUNCTION_CALL: {
            TZrSize argIndex;
            if (node->data.functionCall.args != ZR_NULL) {
                for (argIndex = 0; argIndex < node->data.functionCall.args->count; ++argIndex) {
                    if (!module_init_analyze_expression(context, node->data.functionCall.args->nodes[argIndex], effects, ZR_NULL)) {
                        return ZR_FALSE;
                    }
                }
            }
            module_init_record_dynamic_unknown(context, effects, &node->location);
            return ZR_TRUE;
        }
        case ZR_AST_ARRAY_LITERAL: {
            TZrSize index;
            for (index = 0; node->data.arrayLiteral.elements != ZR_NULL && index < node->data.arrayLiteral.elements->count; ++index) {
                if (!module_init_analyze_expression(context, node->data.arrayLiteral.elements->nodes[index], effects, ZR_NULL)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        }
        case ZR_AST_OBJECT_LITERAL: {
            TZrSize index;
            for (index = 0; node->data.objectLiteral.properties != ZR_NULL && index < node->data.objectLiteral.properties->count; ++index) {
                if (!module_init_analyze_expression(context, node->data.objectLiteral.properties->nodes[index], effects, ZR_NULL)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        }
        case ZR_AST_KEY_VALUE_PAIR:
            if (!module_init_analyze_expression(context, node->data.keyValuePair.key, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_expression(context, node->data.keyValuePair.value, effects, ZR_NULL);
        case ZR_AST_GENERATOR_EXPRESSION:
            return module_init_analyze_statement(context, node->data.generatorExpression.block, effects);
        case ZR_AST_IF_EXPRESSION:
            if (!module_init_analyze_expression(context, node->data.ifExpression.condition, effects, ZR_NULL) ||
                !module_init_analyze_expression(context, node->data.ifExpression.thenExpr, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_expression(context, node->data.ifExpression.elseExpr, effects, ZR_NULL);
        default:
            return ZR_TRUE;
    }
}

static TZrBool module_init_expand_callable_effects(SZrParserInitAnalysisContext *context,
                                                   TZrUInt32 callableIndex,
                                                   SZrArray *effects) {
    SZrParserInitCallableInfo *callable;
    TZrSize index;

    if (context == ZR_NULL || effects == ZR_NULL || callableIndex >= context->callables.length) {
        return ZR_TRUE;
    }

    callable = (SZrParserInitCallableInfo *)ZrCore_Array_Get(&context->callables, callableIndex);
    if (callable == ZR_NULL) {
        return ZR_TRUE;
    }

    if (callable->isAnalyzed) {
        if (effects == &callable->effects) {
            return ZR_TRUE;
        }
        for (index = 0; index < callable->effects.length; ++index) {
            SZrFunctionModuleEffect *effect =
                    (SZrFunctionModuleEffect *)ZrCore_Array_Get(&callable->effects, index);
            if (effect != ZR_NULL) {
                module_init_append_effect(context->cs->state, effects, effect);
            }
        }
        return ZR_TRUE;
    }

    if (callable->isAnalyzing) {
        return ZR_TRUE;
    }

    callable->isAnalyzing = ZR_TRUE;
    {
        TZrSize bindingsLength = context->bindings.length;
        TZrBool oldInCallableBody = context->inCallableBody;
        TZrSize paramIndex;

        context->inCallableBody = ZR_TRUE;
        if (callable->params != ZR_NULL) {
            for (paramIndex = 0; paramIndex < callable->params->count; ++paramIndex) {
                SZrAstNode *paramNode = callable->params->nodes[paramIndex];
                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER &&
                    paramNode->data.parameter.name != ZR_NULL &&
                    paramNode->data.parameter.name->name != ZR_NULL) {
                    module_init_push_binding(context->cs->state,
                                             &context->bindings,
                                             paramNode->data.parameter.name->name,
                                             ZR_PARSER_INIT_BINDING_LOCAL,
                                             ZR_NULL,
                                             ZR_NULL,
                                             ZR_MODULE_EXPORT_KIND_VALUE,
                                             ZR_MODULE_EXPORT_READY_ENTRY,
                                             ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
                }
            }
        }

        if (!module_init_analyze_statement(context, callable->body, &callable->effects)) {
            callable->isAnalyzing = ZR_FALSE;
            context->bindings.length = bindingsLength;
            context->inCallableBody = oldInCallableBody;
            return ZR_FALSE;
        }

        context->bindings.length = bindingsLength;
        context->inCallableBody = oldInCallableBody;
    }

    callable->isAnalyzing = ZR_FALSE;
    callable->isAnalyzed = ZR_TRUE;
    if (effects == &callable->effects) {
        return ZR_TRUE;
    }
    for (index = 0; index < callable->effects.length; ++index) {
        SZrFunctionModuleEffect *effect =
                (SZrFunctionModuleEffect *)ZrCore_Array_Get(&callable->effects, index);
        if (effect != ZR_NULL) {
            module_init_append_effect(context->cs->state, effects, effect);
        }
    }
    return ZR_TRUE;
}

static TZrBool module_init_analyze_statement(SZrParserInitAnalysisContext *context,
                                             SZrAstNode *node,
                                             SZrArray *effects) {
    TZrSize index;
    TZrSize scopeBindingsLength;

    if (context == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_BLOCK:
            scopeBindingsLength = context->bindings.length;
            if (node->data.block.body != ZR_NULL) {
                for (index = 0; index < node->data.block.body->count; ++index) {
                    if (!module_init_analyze_statement(context, node->data.block.body->nodes[index], effects)) {
                        context->bindings.length = scopeBindingsLength;
                        return ZR_FALSE;
                    }
                }
            }
            context->bindings.length = scopeBindingsLength;
            return ZR_TRUE;
        case ZR_AST_VARIABLE_DECLARATION:
            if (!module_init_analyze_expression(context, node->data.variableDeclaration.value, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            module_init_register_import_binding_from_variable(context, node);
            return ZR_TRUE;
        case ZR_AST_EXPRESSION_STATEMENT:
            return module_init_analyze_expression(context, node->data.expressionStatement.expr, effects, ZR_NULL);
        case ZR_AST_RETURN_STATEMENT:
            return module_init_analyze_expression(context, node->data.returnStatement.expr, effects, ZR_NULL);
        case ZR_AST_THROW_STATEMENT:
            return module_init_analyze_expression(context, node->data.throwStatement.expr, effects, ZR_NULL);
        case ZR_AST_USING_STATEMENT:
            if (!module_init_analyze_expression(context, node->data.usingStatement.resource, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_statement(context, node->data.usingStatement.body, effects);
        case ZR_AST_IF_EXPRESSION:
            if (!module_init_analyze_expression(context, node->data.ifExpression.condition, effects, ZR_NULL) ||
                !module_init_analyze_statement(context, node->data.ifExpression.thenExpr, effects)) {
                return ZR_FALSE;
            }
            return module_init_analyze_statement(context, node->data.ifExpression.elseExpr, effects);
        case ZR_AST_WHILE_LOOP:
            if (!module_init_analyze_expression(context, node->data.whileLoop.cond, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_statement(context, node->data.whileLoop.block, effects);
        case ZR_AST_FOR_LOOP:
            if (!module_init_analyze_statement(context, node->data.forLoop.init, effects) ||
                !module_init_analyze_statement(context, node->data.forLoop.cond, effects) ||
                !module_init_analyze_expression(context, node->data.forLoop.step, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_statement(context, node->data.forLoop.block, effects);
        case ZR_AST_FOREACH_LOOP:
            if (!module_init_analyze_expression(context, node->data.foreachLoop.expr, effects, ZR_NULL)) {
                return ZR_FALSE;
            }
            return module_init_analyze_statement(context, node->data.foreachLoop.block, effects);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            if (!module_init_analyze_statement(context, node->data.tryCatchFinallyStatement.block, effects)) {
                return ZR_FALSE;
            }
            if (node->data.tryCatchFinallyStatement.catchClauses != ZR_NULL) {
                for (index = 0; index < node->data.tryCatchFinallyStatement.catchClauses->count; ++index) {
                    if (!module_init_analyze_statement(context,
                                                       node->data.tryCatchFinallyStatement.catchClauses->nodes[index],
                                                       effects)) {
                        return ZR_FALSE;
                    }
                }
            }
            return module_init_analyze_statement(context, node->data.tryCatchFinallyStatement.finallyBlock, effects);
        case ZR_AST_CATCH_CLAUSE:
            return module_init_analyze_statement(context, node->data.catchClause.block, effects);
        case ZR_AST_FUNCTION_DECLARATION:
            if (node->data.functionDeclaration.name != ZR_NULL && node->data.functionDeclaration.name->name != ZR_NULL) {
                module_init_push_binding(context->cs->state,
                                         &context->bindings,
                                         node->data.functionDeclaration.name->name,
                                         ZR_PARSER_INIT_BINDING_LOCAL,
                                         ZR_NULL,
                                         ZR_NULL,
                                         ZR_MODULE_EXPORT_KIND_FUNCTION,
                                         ZR_MODULE_EXPORT_READY_DECLARATION,
                                         ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE);
            }
            return ZR_TRUE;
        default:
            return module_init_analyze_expression(context, node, effects, ZR_NULL);
    }
}

static TZrBool module_init_build_source_callable_catalog(SZrParserInitAnalysisContext *context) {
    TZrSize index;
    SZrParserModuleInitSummary *summary;

    if (context == ZR_NULL || context->scriptAst == ZR_NULL || context->scriptAst->type != ZR_AST_SCRIPT ||
        context->scriptAst->data.script.statements == ZR_NULL) {
        return ZR_TRUE;
    }

    summary = module_init_context_summary_mutable(context);
    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < summary->bindings.length; ++index) {
        const SZrModuleInitBindingInfo *binding =
                (const SZrModuleInitBindingInfo *)ZrCore_Array_Get(&summary->bindings, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL) {
            module_init_push_binding_from_info(context->cs->state, &context->bindings, binding);
        }
    }

    for (index = 0; index < context->scriptAst->data.script.statements->count; ++index) {
        SZrAstNode *statement = context->scriptAst->data.script.statements->nodes[index];
        const SZrModuleInitExportInfo *exportInfo = ZR_NULL;

        if (statement == ZR_NULL || statement->type != ZR_AST_FUNCTION_DECLARATION ||
            statement->data.functionDeclaration.name == ZR_NULL ||
            statement->data.functionDeclaration.name->name == ZR_NULL) {
            continue;
        }

        summary = module_init_context_summary_mutable(context);
        if (summary == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!module_init_find_export(summary,
                                     statement->data.functionDeclaration.name->name,
                                     &exportInfo)) {
            continue;
        }

        {
            SZrParserInitCallableInfo callable;
            ZrCore_Memory_RawSet(&callable, 0, sizeof(callable));
            callable.name = statement->data.functionDeclaration.name->name;
            callable.body = statement->data.functionDeclaration.body;
            callable.params = statement->data.functionDeclaration.params;
            callable.isExported = exportInfo != ZR_NULL &&
                                  exportInfo->exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION ? ZR_TRUE : ZR_FALSE;
            ZrCore_Array_Init(context->cs->state,
                              &callable.effects,
                              sizeof(SZrFunctionModuleEffect),
                              ZR_PARSER_INITIAL_CAPACITY_SMALL);
            ZrCore_Array_Push(context->cs->state, &context->callables, &callable);
        }
    }

    return ZR_TRUE;
}

static void module_init_free_callable_catalog(SZrState *state, SZrArray *callables) {
    TZrSize index;

    if (state == ZR_NULL || callables == ZR_NULL || !callables->isValid || callables->head == ZR_NULL) {
        return;
    }

    for (index = 0; index < callables->length; ++index) {
        SZrParserInitCallableInfo *callable =
                (SZrParserInitCallableInfo *)ZrCore_Array_Get(callables, index);
        if (callable != ZR_NULL && callable->effects.isValid && callable->effects.head != ZR_NULL) {
            ZrCore_Array_Free(state, &callable->effects);
        }
    }

    ZrCore_Array_Free(state, callables);
}

static TZrBool module_init_copy_effect_array_to_raw(SZrCompilerState *cs,
                                                    SZrArray *source,
                                                    SZrFunctionModuleEffect **outEffects,
                                                    TZrUInt32 *outCount) {
    if (outEffects != ZR_NULL) {
        *outEffects = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (cs == ZR_NULL || source == ZR_NULL || outEffects == ZR_NULL || outCount == ZR_NULL || source->length == 0) {
        return ZR_TRUE;
    }

    *outEffects = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                              sizeof(SZrFunctionModuleEffect) * source->length,
                                                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (*outEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawCopy(*outEffects, source->head, sizeof(SZrFunctionModuleEffect) * source->length);
    *outCount = (TZrUInt32)source->length;
    return ZR_TRUE;
}

static TZrBool module_init_analyze_source_summary(SZrCompilerState *cs,
                                                  SZrParserModuleInitSummary *summary,
                                                  SZrAstNode *ast) {
    SZrParserInitAnalysisContext context;
    SZrParserModuleInitSummary *currentSummary;
    TZrSize index;

    if (cs == ZR_NULL || summary == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }
    module_init_trace("analyze source summary '%s' summary=%p ast=%p",
                      summary->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(summary->moduleName) : "<null>",
                      (void *)summary,
                      (void *)ast);

    if (summary->hasAnalysis) {
        return ZR_TRUE;
    }

    ZrCore_Memory_RawSet(&context, 0, sizeof(context));
    context.cs = cs;
    context.summary = summary;
    context.moduleName = summary->moduleName;
    context.scriptAst = ast;
    ZrCore_Array_Init(cs->state, &context.callables, sizeof(SZrParserInitCallableInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(cs->state, &context.bindings, sizeof(SZrParserInitBinding), ZR_PARSER_INITIAL_CAPACITY_SMALL);

    if (!module_init_build_source_callable_catalog(&context)) {
        module_init_free_callable_catalog(cs->state, &context.callables);
        ZrCore_Array_Free(cs->state, &context.bindings);
        return ZR_FALSE;
    }

    if (ast->data.script.statements != ZR_NULL) {
        for (index = 0; index < ast->data.script.statements->count; ++index) {
            SZrAstNode *statement = ast->data.script.statements->nodes[index];
            if (statement == ZR_NULL) {
                continue;
            }

            if (statement->type == ZR_AST_FUNCTION_DECLARATION ||
                statement->type == ZR_AST_STRUCT_DECLARATION ||
                statement->type == ZR_AST_CLASS_DECLARATION ||
                statement->type == ZR_AST_INTERFACE_DECLARATION ||
                statement->type == ZR_AST_ENUM_DECLARATION) {
                continue;
            }

            currentSummary = module_init_context_summary_mutable(&context);
            if (currentSummary == ZR_NULL ||
                !module_init_analyze_statement(&context, statement, &currentSummary->entryEffects)) {
                module_init_free_callable_catalog(cs->state, &context.callables);
                ZrCore_Array_Free(cs->state, &context.bindings);
                return ZR_FALSE;
            }
        }
    }

    for (index = 0; index < context.callables.length; ++index) {
        SZrParserInitCallableInfo *callable =
                (SZrParserInitCallableInfo *)ZrCore_Array_Get(&context.callables, index);
        if (callable == ZR_NULL || !callable->isExported) {
            continue;
        }

        if (!module_init_expand_callable_effects(&context, (TZrUInt32)index, &callable->effects)) {
            module_init_free_callable_catalog(cs->state, &context.callables);
            ZrCore_Array_Free(cs->state, &context.bindings);
            return ZR_FALSE;
        }

        {
            SZrModuleInitCallableSummary exportedSummary;
            ZrCore_Memory_RawSet(&exportedSummary, 0, sizeof(exportedSummary));
            exportedSummary.name = callable->name;
            exportedSummary.callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
            if (!module_init_copy_effect_array_to_raw(cs,
                                                      &callable->effects,
                                                      &exportedSummary.effects,
                                                      &exportedSummary.effectCount)) {
                module_init_free_callable_catalog(cs->state, &context.callables);
                ZrCore_Array_Free(cs->state, &context.bindings);
                return ZR_FALSE;
            }
            currentSummary = module_init_context_summary_mutable(&context);
            if (currentSummary == ZR_NULL) {
                module_init_free_callable_catalog(cs->state, &context.callables);
                ZrCore_Array_Free(cs->state, &context.bindings);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &currentSummary->exportedCallableSummaries, &exportedSummary);
        }
    }

    currentSummary = module_init_context_summary_mutable(&context);
    if (currentSummary == ZR_NULL) {
        module_init_free_callable_catalog(cs->state, &context.callables);
        ZrCore_Array_Free(cs->state, &context.bindings);
        return ZR_FALSE;
    }
    currentSummary->hasAnalysis = ZR_TRUE;
    currentSummary->state = ZR_PARSER_MODULE_INIT_SUMMARY_READY;
    module_init_trace("analyze source summary done '%s' summary=%p",
                      currentSummary->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(currentSummary->moduleName) : "<null>",
                      (void *)currentSummary);
    module_init_free_callable_catalog(cs->state, &context.callables);
    ZrCore_Array_Free(cs->state, &context.bindings);
    return ZR_TRUE;
}

static TZrBool module_init_analyze_binary_summary(SZrCompilerState *cs,
                                                  SZrParserModuleInitSummary *summary,
                                                  const SZrIoFunction *function) {
    TZrSize index;

    if (cs == ZR_NULL || summary == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!summary->exports.isValid) {
        ZrCore_Array_Init(cs->state, &summary->exports, sizeof(SZrModuleInitExportInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->bindings.isValid) {
        ZrCore_Array_Init(cs->state, &summary->bindings, sizeof(SZrModuleInitBindingInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->staticImports.isValid) {
        ZrCore_Array_Init(cs->state, &summary->staticImports, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->entryEffects.isValid) {
        ZrCore_Array_Init(cs->state, &summary->entryEffects, sizeof(SZrFunctionModuleEffect), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
    if (!summary->exportedCallableSummaries.isValid) {
        ZrCore_Array_Init(cs->state,
                          &summary->exportedCallableSummaries,
                          sizeof(SZrModuleInitCallableSummary),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    for (index = 0; index < function->staticImportsLength; ++index) {
        module_init_add_static_import(cs->state, summary, function->staticImports[index]);
    }

    for (index = 0; index < function->typedExportedSymbolsLength; ++index) {
        const SZrIoFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        SZrModuleInitExportInfo exportInfo;
        if (symbol->name == ZR_NULL) {
            continue;
        }

        ZrCore_Memory_RawSet(&exportInfo, 0, sizeof(exportInfo));
        exportInfo.name = symbol->name;
        exportInfo.accessModifier = symbol->accessModifier;
        exportInfo.exportKind = symbol->exportKind;
        exportInfo.readiness = symbol->readiness;
        exportInfo.symbolKind = symbol->symbolKind;
        exportInfo.prototypeType = 0;
        exportInfo.callableChildIndex = symbol->callableChildIndex;
        exportInfo.valueType.baseType = symbol->valueType.baseType;
        exportInfo.valueType.isNullable = symbol->valueType.isNullable;
        exportInfo.valueType.ownershipQualifier = symbol->valueType.ownershipQualifier;
        exportInfo.valueType.isArray = symbol->valueType.isArray;
        exportInfo.valueType.typeName = symbol->valueType.typeName;
        exportInfo.valueType.elementBaseType = symbol->valueType.elementBaseType;
        exportInfo.valueType.elementTypeName = symbol->valueType.elementTypeName;
        exportInfo.parameterCount = (TZrUInt32)symbol->parameterCount;
        exportInfo.lineInSourceStart = symbol->lineInSourceStart;
        exportInfo.columnInSourceStart = symbol->columnInSourceStart;
        exportInfo.lineInSourceEnd = symbol->lineInSourceEnd;
        exportInfo.columnInSourceEnd = symbol->columnInSourceEnd;
        if (symbol->parameterCount > 0 && symbol->parameterTypes != ZR_NULL) {
            TZrSize bytes = sizeof(SZrFunctionTypedTypeRef) * symbol->parameterCount;
            exportInfo.parameterTypes =
                    (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                               bytes,
                                                                               ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (exportInfo.parameterTypes == ZR_NULL) {
                return ZR_FALSE;
            }
            for (TZrSize paramIndex = 0; paramIndex < symbol->parameterCount; ++paramIndex) {
                exportInfo.parameterTypes[paramIndex].baseType = symbol->parameterTypes[paramIndex].baseType;
                exportInfo.parameterTypes[paramIndex].isNullable = symbol->parameterTypes[paramIndex].isNullable;
                exportInfo.parameterTypes[paramIndex].ownershipQualifier = symbol->parameterTypes[paramIndex].ownershipQualifier;
                exportInfo.parameterTypes[paramIndex].isArray = symbol->parameterTypes[paramIndex].isArray;
                exportInfo.parameterTypes[paramIndex].typeName = symbol->parameterTypes[paramIndex].typeName;
                exportInfo.parameterTypes[paramIndex].elementBaseType = symbol->parameterTypes[paramIndex].elementBaseType;
                exportInfo.parameterTypes[paramIndex].elementTypeName = symbol->parameterTypes[paramIndex].elementTypeName;
            }
        }
        if (!module_init_add_export_info(cs->state, summary, &exportInfo)) {
            return ZR_FALSE;
        }
        module_init_upsert_binding_info(cs->state,
                                        summary,
                                        exportInfo.name,
                                        exportInfo.exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION
                                                ? ZR_PARSER_INIT_BINDING_TOP_LEVEL_CALLABLE
                                        : exportInfo.exportKind == ZR_MODULE_EXPORT_KIND_TYPE
                                                ? ZR_PARSER_INIT_BINDING_TOP_LEVEL_TYPE
                                                : ZR_PARSER_INIT_BINDING_TOP_LEVEL_ENTRY,
                                        summary->moduleName,
                                        ZR_NULL,
                                        (EZrModuleExportKind)exportInfo.exportKind,
                                        (EZrModuleExportReadiness)exportInfo.readiness,
                                        exportInfo.callableChildIndex);
    }

    for (index = 0; index < function->moduleEntryEffectsLength; ++index) {
        ZrCore_Array_Push(cs->state, &summary->entryEffects, &function->moduleEntryEffects[index]);
    }

    for (index = 0; index < function->exportedCallableSummariesLength; ++index) {
        const SZrIoFunctionCallableSummary *ioSummary = &function->exportedCallableSummaries[index];
        SZrModuleInitCallableSummary callableSummary;
        ZrCore_Memory_RawSet(&callableSummary, 0, sizeof(callableSummary));
        callableSummary.name = ioSummary->name;
        callableSummary.callableChildIndex = ioSummary->callableChildIndex;
        callableSummary.effectCount = (TZrUInt32)ioSummary->effectCount;
        if (ioSummary->effectCount > 0 && ioSummary->effects != ZR_NULL) {
            callableSummary.effects =
                    (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                               sizeof(SZrFunctionModuleEffect) * ioSummary->effectCount,
                                                                               ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (callableSummary.effects == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Memory_RawCopy(callableSummary.effects,
                                  ioSummary->effects,
                                  sizeof(SZrFunctionModuleEffect) * ioSummary->effectCount);
        }
        ZrCore_Array_Push(cs->state, &summary->exportedCallableSummaries, &callableSummary);
    }

    for (index = 0; index < function->topLevelCallableBindingsLength; ++index) {
        const SZrIoFunctionTopLevelCallableBinding *binding = &function->topLevelCallableBindings[index];

        if (binding->name == ZR_NULL) {
            continue;
        }

        module_init_upsert_binding_info(cs->state,
                                        summary,
                                        binding->name,
                                        ZR_PARSER_INIT_BINDING_TOP_LEVEL_CALLABLE,
                                        summary->moduleName,
                                        ZR_NULL,
                                        (EZrModuleExportKind)binding->exportKind,
                                        (EZrModuleExportReadiness)binding->readiness,
                                        binding->callableChildIndex);
    }

    summary->hasPrescan = ZR_TRUE;
    summary->hasAnalysis = ZR_TRUE;
    summary->state = ZR_PARSER_MODULE_INIT_SUMMARY_READY;
    return ZR_TRUE;
}

static TZrBool module_init_ensure_summary_analyzed(SZrCompilerState *cs, SZrString *moduleName) {
    SZrParserModuleInitSummary *summary;

    if (cs == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary != ZR_NULL) {
        if (summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
            return ZR_FALSE;
        }
        if (summary->hasAnalysis) {
            return ZR_TRUE;
        }
        if (summary->astIdentity != ZR_NULL &&
            !summary->isBinary &&
            summary->hasPrescan &&
            !module_init_analyze_source_summary(cs, summary, (SZrAstNode *)summary->astIdentity)) {
            return ZR_FALSE;
        }
        summary = module_init_find_summary_mutable(cs->state->global, moduleName);
        if (summary == ZR_NULL || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
            return ZR_FALSE;
        }
        if (summary->hasAnalysis && summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED &&
            !summary->validating && !module_init_validate_summary(cs, summary)) {
            return ZR_FALSE;
        }
        summary = module_init_find_summary_mutable(cs->state->global, moduleName);
        if (summary == ZR_NULL || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
            return ZR_FALSE;
        }
        if (summary->hasAnalysis) {
            return summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED;
        }
    }

    if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, moduleName)) {
        return ZR_FALSE;
    }

    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary == ZR_NULL || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
        return ZR_FALSE;
    }

    if (!summary->hasAnalysis &&
        summary->astIdentity != ZR_NULL &&
        !summary->isBinary &&
        summary->hasPrescan &&
        !module_init_analyze_source_summary(cs, summary, (SZrAstNode *)summary->astIdentity)) {
        return ZR_FALSE;
    }
    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary == ZR_NULL || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
        return ZR_FALSE;
    }
    if (summary->hasAnalysis && summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED &&
        !summary->validating && !module_init_validate_summary(cs, summary)) {
        return ZR_FALSE;
    }
    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    return summary->hasAnalysis && summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED;
}

static TZrBytePtr module_init_read_all_bytes(SZrState *state, SZrIo *io, TZrSize *outSize) {
    SZrGlobalState *global;
    TZrSize totalSize = 0;
    TZrSize capacity;
    TZrBytePtr buffer;
    TZrSize readSize;
    TZrBytePtr readBuffer;

    if (outSize != ZR_NULL) {
        *outSize = 0;
    }
    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    capacity = io->remained > 0 ? io->remained : ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY;
    buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    if (io->remained == 0 && io->read != ZR_NULL) {
        readSize = 0;
        readBuffer = io->read(state, io->customData, &readSize);
        if (readBuffer != ZR_NULL && readSize > 0) {
            io->pointer = readBuffer;
            io->remained = readSize;
        }
    }

    while (io->remained > 0) {
        if (io->pointer == ZR_NULL) {
            io->remained = 0;
            break;
        }

        if (totalSize + io->remained + 1 > capacity) {
            TZrSize newCapacity = capacity;
            TZrBytePtr newBuffer;
            while (totalSize + io->remained + 1 > newCapacity) {
                newCapacity *= 2;
            }

            newBuffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global,
                                                                    newCapacity + 1,
                                                                    ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (newBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(newBuffer, buffer, totalSize);
            }
            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = newBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, io->remained);
        totalSize += io->remained;
        io->pointer += io->remained;
        io->remained = 0;

        if (io->read != ZR_NULL) {
            readSize = 0;
            readBuffer = io->read(state, io->customData, &readSize);
            if (readBuffer != ZR_NULL && readSize > 0) {
                io->pointer = readBuffer;
                io->remained = readSize;
            }
        }
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static TZrBool module_init_module_reaches_internal(SZrCompilerState *cs,
                                                   SZrString *fromModule,
                                                   SZrString *targetModule,
                                                   SZrParserSummaryPathBuffer *path,
                                                   TZrUInt32 depth) {
    const SZrParserModuleInitSummary *summary;
    TZrSize index;

    if (cs == ZR_NULL || fromModule == ZR_NULL || targetModule == ZR_NULL || path == ZR_NULL ||
        depth >= ZR_ARRAY_COUNT(path->modules)) {
        return ZR_FALSE;
    }

    for (index = 0; index < depth; ++index) {
        if (path->modules[index] != ZR_NULL && ZrCore_String_Equal(path->modules[index], fromModule)) {
            return ZR_FALSE;
        }
    }

    path->modules[depth] = fromModule;
    path->length = depth + 1;
    if (ZrCore_String_Equal(fromModule, targetModule)) {
        return ZR_TRUE;
    }

    summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, fromModule);
    if (summary == ZR_NULL) {
        if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, fromModule)) {
            return ZR_FALSE;
        }
        summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, fromModule);
    }

    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < summary->staticImports.length; ++index) {
        SZrString **importNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&summary->staticImports, index);
        if (importNamePtr != ZR_NULL &&
            *importNamePtr != ZR_NULL &&
            module_init_module_reaches_internal(cs, *importNamePtr, targetModule, path, depth + 1)) {
            return ZR_TRUE;
        }
    }

    if (path->length == depth + 1) {
        path->length = depth;
    }
    return ZR_FALSE;
}

static TZrBool module_init_modules_are_same_scc(SZrCompilerState *cs, SZrString *left, SZrString *right) {
    SZrParserSummaryPathBuffer path;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_String_Equal(left, right)) {
        return ZR_TRUE;
    }

    ZrCore_Memory_RawSet(&path, 0, sizeof(path));
    if (!module_init_module_reaches_internal(cs, left, right, &path, 0)) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(&path, 0, sizeof(path));
    return module_init_module_reaches_internal(cs, right, left, &path, 0);
}

static void module_init_format_cycle_path(const SZrParserSummaryPathBuffer *forward,
                                          const SZrParserSummaryPathBuffer *backward,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    TZrSize offset = 0;
    TZrUInt32 index;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (forward == ZR_NULL || forward->length == 0) {
        return;
    }

    for (index = 0; index < forward->length; ++index) {
        const TZrChar *name = ZrCore_String_GetNativeString(forward->modules[index]);
        offset += (TZrSize)snprintf(buffer + offset,
                                    bufferSize > offset ? bufferSize - offset : 0,
                                    "%s%s",
                                    index == 0 ? "" : " -> ",
                                    name != ZR_NULL ? name : "<null>");
        if (offset >= bufferSize) {
            return;
        }
    }

    if (backward != ZR_NULL && backward->length > 1) {
        for (index = 1; index < backward->length; ++index) {
            const TZrChar *name = ZrCore_String_GetNativeString(backward->modules[index]);
            offset += (TZrSize)snprintf(buffer + offset,
                                        bufferSize > offset ? bufferSize - offset : 0,
                                        " -> %s",
                                        name != ZR_NULL ? name : "<null>");
            if (offset >= bufferSize) {
                return;
            }
        }
    }
}

static const SZrModuleInitCallableSummary *module_init_find_exported_callable_summary(
        const SZrParserModuleInitSummary *summary,
        SZrString *name) {
    TZrSize index;

    if (summary == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < summary->exportedCallableSummaries.length; ++index) {
        const SZrModuleInitCallableSummary *callable =
                (const SZrModuleInitCallableSummary *)ZrCore_Array_Get((SZrArray *)&summary->exportedCallableSummaries,
                                                                       index);
        if (callable != ZR_NULL && callable->name != ZR_NULL && ZrCore_String_Equal(callable->name, name)) {
            return callable;
        }
    }

    return ZR_NULL;
}

static TZrBool module_init_validate_callable_summary_recursive(SZrCompilerState *cs,
                                                               SZrString *rootModuleName,
                                                               const SZrModuleInitCallableSummary *callableSummary,
                                                               SZrArray *visiting,
                                                               SZrFileRange *outLocation,
                                                               TZrChar *outMessage,
                                                               TZrSize outMessageSize) {
    TZrSize index;

    if (cs == ZR_NULL || rootModuleName == ZR_NULL || callableSummary == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; visiting != ZR_NULL && index < visiting->length; ++index) {
        SZrString **existing = (SZrString **)ZrCore_Array_Get(visiting, index);
        if (existing != ZR_NULL && *existing != ZR_NULL && callableSummary->name != ZR_NULL &&
            ZrCore_String_Equal(*existing, callableSummary->name)) {
            return ZR_TRUE;
        }
    }

    if (visiting != ZR_NULL && callableSummary->name != ZR_NULL) {
        SZrString *callableName = callableSummary->name;
        ZrCore_Array_Push(cs->state, visiting, &callableName);
    }

    for (index = 0; index < callableSummary->effectCount; ++index) {
        const SZrFunctionModuleEffect *effect = &callableSummary->effects[index];
        const TZrChar *moduleNameText;
        const TZrChar *symbolNameText;
        if (effect->kind == ZR_MODULE_ENTRY_EFFECT_LOCAL_ENTRY_BINDING_READ ||
            effect->kind == ZR_MODULE_ENTRY_EFFECT_DYNAMIC_UNKNOWN) {
            if (outLocation != ZR_NULL) {
                outLocation->start.line = effect->lineInSourceStart;
                outLocation->start.column = effect->columnInSourceStart;
                outLocation->end.line = effect->lineInSourceEnd;
                outLocation->end.column = effect->columnInSourceEnd;
            }
            snprintf(outMessage,
                     outMessageSize,
                     "circular import initialization: imported callable '%s' is not declaration-safe",
                     callableSummary->name != ZR_NULL ? ZrCore_String_GetNativeString(callableSummary->name) : "<anonymous>");
            if (visiting != ZR_NULL && visiting->length > 0) {
                visiting->length--;
            }
            return ZR_FALSE;
        }

        if (effect->moduleName == ZR_NULL || !module_init_modules_are_same_scc(cs, rootModuleName, effect->moduleName)) {
            continue;
        }

        moduleNameText = ZrCore_String_GetNativeString(effect->moduleName);
        symbolNameText = ZrCore_String_GetNativeString(effect->symbolName);
        if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_READ &&
            effect->readiness == ZR_MODULE_EXPORT_READY_ENTRY) {
            if (outLocation != ZR_NULL) {
                outLocation->start.line = effect->lineInSourceStart;
                outLocation->start.column = effect->columnInSourceStart;
                outLocation->end.line = effect->lineInSourceEnd;
                outLocation->end.column = effect->columnInSourceEnd;
            }
            snprintf(outMessage,
                     outMessageSize,
                     "circular import initialization: imported callable reads entry-initialized export '%s.%s'",
                     moduleNameText != ZR_NULL ? moduleNameText : "<module>",
                     symbolNameText != ZR_NULL ? symbolNameText : "<symbol>");
            if (visiting != ZR_NULL && visiting->length > 0) {
                visiting->length--;
            }
            return ZR_FALSE;
        }

        if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_REF &&
            !(effect->exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION || effect->exportKind == ZR_MODULE_EXPORT_KIND_TYPE)) {
            if (outLocation != ZR_NULL) {
                outLocation->start.line = effect->lineInSourceStart;
                outLocation->start.column = effect->columnInSourceStart;
                outLocation->end.line = effect->lineInSourceEnd;
                outLocation->end.column = effect->columnInSourceEnd;
            }
            snprintf(outMessage,
                     outMessageSize,
                     "circular import initialization: imported callable references non-declaration export '%s.%s'",
                     moduleNameText != ZR_NULL ? moduleNameText : "<module>",
                     symbolNameText != ZR_NULL ? symbolNameText : "<symbol>");
            if (visiting != ZR_NULL && visiting->length > 0) {
                visiting->length--;
            }
            return ZR_FALSE;
        }

        if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL) {
            const SZrParserModuleInitSummary *targetSummary;
            const SZrModuleInitCallableSummary *targetCallable;
            if (!module_init_ensure_summary_analyzed(cs, effect->moduleName) ||
                (targetSummary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, effect->moduleName)) == ZR_NULL ||
                !targetSummary->hasAnalysis) {
                if (outLocation != ZR_NULL) {
                    outLocation->start.line = effect->lineInSourceStart;
                    outLocation->start.column = effect->columnInSourceStart;
                    outLocation->end.line = effect->lineInSourceEnd;
                    outLocation->end.column = effect->columnInSourceEnd;
                }
                snprintf(outMessage,
                         outMessageSize,
                         "circular import initialization: cross-module call '%s.%s()' inside __entry__ is not allowed without summary",
                         moduleNameText != ZR_NULL ? moduleNameText : "<module>",
                         symbolNameText != ZR_NULL ? symbolNameText : "<symbol>");
                if (visiting != ZR_NULL && visiting->length > 0) {
                    visiting->length--;
                }
                return ZR_FALSE;
            }

            targetCallable = module_init_find_exported_callable_summary(targetSummary, effect->symbolName);
            if (targetCallable == ZR_NULL ||
                !module_init_validate_callable_summary_recursive(cs,
                                                                rootModuleName,
                                                                targetCallable,
                                                                visiting,
                                                                outLocation,
                                                                outMessage,
                                                                outMessageSize)) {
                if (outMessage[0] == '\0') {
                    snprintf(outMessage,
                             outMessageSize,
                             "circular import initialization: cross-module call '%s.%s()' inside __entry__ is not declaration-safe",
                             moduleNameText != ZR_NULL ? moduleNameText : "<module>",
                             symbolNameText != ZR_NULL ? symbolNameText : "<symbol>");
                }
                if (visiting != ZR_NULL && visiting->length > 0) {
                    visiting->length--;
                }
                return ZR_FALSE;
            }
        }
    }

    if (visiting != ZR_NULL && visiting->length > 0) {
        visiting->length--;
    }
    return ZR_TRUE;
}

static TZrBool module_init_validate_summary(SZrCompilerState *cs, SZrParserModuleInitSummary *summary) {
    TZrSize index;
    SZrArray visiting;
    TZrBool validationResult = ZR_TRUE;
    SZrString *summaryModuleName;
    SZrParserModuleInitSummary *currentSummary;

    if (cs == ZR_NULL || summary == ZR_NULL) {
        return ZR_FALSE;
    }
    summaryModuleName = summary->moduleName;
    module_init_trace("validate summary '%s' summary=%p",
                      summaryModuleName != ZR_NULL ? ZrCore_String_GetNativeString(summaryModuleName) : "<null>",
                      (void *)summary);

    currentSummary = module_init_find_summary_mutable(cs->state->global, summaryModuleName);
    if (currentSummary == ZR_NULL) {
        return ZR_FALSE;
    }

    if (currentSummary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
        return ZR_FALSE;
    }

    if (currentSummary->validating) {
        return ZR_TRUE;
    }

    currentSummary->validating = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &visiting, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0;; ++index) {
        const SZrFunctionModuleEffect *effect;
        SZrParserSummaryPathBuffer forwardPath;
        SZrParserSummaryPathBuffer backwardPath;
        TZrChar cyclePath[256];
        TZrChar detail[ZR_PARSER_DETAIL_BUFFER_LENGTH];
        const TZrChar *moduleNameText;
        const TZrChar *symbolNameText;
        SZrFileRange location;

        currentSummary = module_init_find_summary_mutable(cs->state->global, summaryModuleName);
        if (currentSummary == ZR_NULL) {
            validationResult = ZR_FALSE;
            break;
        }
        if (index >= currentSummary->entryEffects.length) {
            break;
        }
        effect = (const SZrFunctionModuleEffect *)ZrCore_Array_Get(&currentSummary->entryEffects, index);

        if (effect == ZR_NULL || effect->moduleName == ZR_NULL ||
            !module_init_modules_are_same_scc(cs, currentSummary->moduleName, effect->moduleName)) {
            continue;
        }

        ZrCore_Memory_RawSet(&location, 0, sizeof(location));
        location.start.line = effect->lineInSourceStart;
        location.start.column = effect->columnInSourceStart;
        location.end.line = effect->lineInSourceEnd;
        location.end.column = effect->columnInSourceEnd;
        ZrCore_Memory_RawSet(&forwardPath, 0, sizeof(forwardPath));
        ZrCore_Memory_RawSet(&backwardPath, 0, sizeof(backwardPath));
        (void)module_init_module_reaches_internal(cs, currentSummary->moduleName, effect->moduleName, &forwardPath, 0);
        (void)module_init_module_reaches_internal(cs, effect->moduleName, currentSummary->moduleName, &backwardPath, 0);
        module_init_format_cycle_path(&forwardPath, &backwardPath, cyclePath, sizeof(cyclePath));
        moduleNameText = ZrCore_String_GetNativeString(effect->moduleName);
        symbolNameText = ZrCore_String_GetNativeString(effect->symbolName);
        if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_READ &&
            effect->readiness == ZR_MODULE_EXPORT_READY_ENTRY) {
            snprintf(detail,
                     sizeof(detail),
                     "circular import initialization: module '%s' reads entry-initialized export '%s.%s' while SCC '%s' is still in __entry__",
                     currentSummary->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(currentSummary->moduleName) : "<module>",
                     moduleNameText != ZR_NULL ? moduleNameText : "<module>",
                     symbolNameText != ZR_NULL ? symbolNameText : "<symbol>",
                     cyclePath);
            module_init_summary_set_error(currentSummary, &location, detail);
            validationResult = ZR_FALSE;
            break;
        }

        if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL) {
            const SZrParserModuleInitSummary *targetSummary = ZR_NULL;
            const SZrModuleInitCallableSummary *callableSummary = ZR_NULL;
            TZrBool callableSafe = ZR_FALSE;
            detail[0] = '\0';
            if (module_init_ensure_summary_analyzed(cs, effect->moduleName)) {
                targetSummary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, effect->moduleName);
                if (targetSummary != ZR_NULL && targetSummary->hasAnalysis) {
                    callableSummary = module_init_find_exported_callable_summary(targetSummary, effect->symbolName);
                }
            }
            currentSummary = module_init_find_summary_mutable(cs->state->global, summaryModuleName);
            if (currentSummary == ZR_NULL) {
                validationResult = ZR_FALSE;
                break;
            }
            if (callableSummary != ZR_NULL) {
                callableSafe = module_init_validate_callable_summary_recursive(cs,
                                                                               currentSummary->moduleName,
                                                                               callableSummary,
                                                                               &visiting,
                                                                               &location,
                                                                               detail,
                                                                               sizeof(detail));
            }
            if (callableSummary == ZR_NULL || !callableSafe) {
                if (detail[0] == '\0') {
                    snprintf(detail,
                             sizeof(detail),
                             "circular import initialization: cross-module call '%s.%s()' inside __entry__ is not allowed in import SCC '%s'",
                             moduleNameText != ZR_NULL ? moduleNameText : "<module>",
                             symbolNameText != ZR_NULL ? symbolNameText : "<symbol>",
                             cyclePath);
                }
                module_init_summary_set_error(currentSummary, &location, detail);
                validationResult = ZR_FALSE;
                break;
            }
        }
    }

    ZrCore_Array_Free(cs->state, &visiting);
    currentSummary = module_init_find_summary_mutable(cs->state->global, summaryModuleName);
    if (currentSummary != ZR_NULL) {
        currentSummary->validating = ZR_FALSE;
        module_init_trace("validate summary done '%s' result=%d summary=%p",
                          summaryModuleName != ZR_NULL ? ZrCore_String_GetNativeString(summaryModuleName) : "<null>",
                          (int)validationResult,
                          (void *)currentSummary);
    }
    return validationResult;
}

static TZrBool module_init_load_summary_from_source_loader(SZrCompilerState *cs,
                                                           SZrParserModuleInitSummary *summary,
                                                           SZrString *moduleName) {
    SZrIo io;
    TZrChar importError[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrFileRange importErrorLocation;
    TZrNativeString moduleNameText;
    TZrSize sourceSize = 0;
    TZrBytePtr sourceBuffer = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;

    if (cs == ZR_NULL || summary == ZR_NULL || moduleName == ZR_NULL || cs->state->global->sourceLoader == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleNameText = ZrCore_String_GetNativeString(moduleName);
    if (moduleNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(cs->state, &io, ZR_NULL, ZR_NULL, ZR_NULL);
    if (!cs->state->global->sourceLoader(cs->state, moduleNameText, ZR_NULL, &io)) {
        return ZR_FALSE;
    }

    if (io.isBinary) {
        SZrIoSource *ioSource = ZR_NULL;

        if (!ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(cs->state, &io, &ioSource)) {
            if (io.close != ZR_NULL) {
                io.close(cs->state, io.customData);
            }
            return ZR_FALSE;
        }
        if (io.close != ZR_NULL) {
            io.close(cs->state, io.customData);
        }
        if (ioSource == ZR_NULL || ioSource->modulesLength == 0 || ioSource->modules == ZR_NULL ||
            ioSource->modules[0].entryFunction == ZR_NULL) {
            if (ioSource != ZR_NULL) {
                ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(cs->state->global, ioSource);
            }
            return ZR_FALSE;
        }
        summary->isBinary = ZR_TRUE;
        if (!module_init_analyze_binary_summary(cs, summary, ioSource->modules[0].entryFunction) ||
            !module_init_validate_summary(cs, summary)) {
            ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(cs->state->global, ioSource);
            return ZR_FALSE;
        }
        ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(cs->state->global, ioSource);
        return ZR_TRUE;
    }

    sourceBuffer = module_init_read_all_bytes(cs->state, &io, &sourceSize);
    if (io.close != ZR_NULL) {
        io.close(cs->state, io.customData);
    }
    if (sourceBuffer == ZR_NULL || sourceSize == 0) {
        if (sourceBuffer != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          sourceBuffer,
                                          sourceSize + 1,
                                          ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        }
        return ZR_FALSE;
    }

    ast = ZrParser_Parse(cs->state, (const TZrChar *)sourceBuffer, sourceSize, moduleName);
    ZrCore_Memory_RawFreeWithType(cs->state->global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (ast == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_ProjectImports_CanonicalizeAst(cs->state,
                                                 ast,
                                                 moduleName,
                                                 ZR_NULL,
                                                 importError,
                                                 sizeof(importError),
                                                 &importErrorLocation)) {
        module_init_summary_set_error(summary,
                                      &importErrorLocation,
                                      importError[0] != '\0' ? importError : "failed to canonicalize imported module imports");
        ZrParser_Ast_Free(cs->state, ast);
        return ZR_FALSE;
    }

    summary->astIdentity = ast;
    summary->ownsAst = ZR_TRUE;
    if (!module_init_prescan_source_summary(cs->state, summary, ast) ||
        !module_init_analyze_source_summary(cs, summary, ast) ||
        !module_init_validate_summary(cs, summary)) {
        summary = module_init_find_summary_mutable(cs->state->global, moduleName);
        if (summary == ZR_NULL) {
            ZrParser_Ast_Free(cs->state, ast);
            return ZR_FALSE;
        }
        if (summary->astIdentity == ast) {
            summary->astIdentity = ZR_NULL;
        }
        summary->ownsAst = ZR_FALSE;
        ZrParser_Ast_Free(cs->state, ast);
        return ZR_FALSE;
    }

    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary == ZR_NULL) {
        ZrParser_Ast_Free(cs->state, ast);
        return ZR_FALSE;
    }
    if (summary->astIdentity == ast) {
        summary->astIdentity = ZR_NULL;
    }
    summary->ownsAst = ZR_FALSE;
    ZrParser_Ast_Free(cs->state, ast);
    return ZR_TRUE;
}

const SZrParserModuleInitSummary *ZrParser_ModuleInitAnalysis_FindSummary(SZrGlobalState *global, SZrString *moduleName) {
    return module_init_find_summary_mutable(global, moduleName);
}

const SZrParserModuleInitSummary *ZrParser_ModuleInitAnalysis_FindSummaryByAst(SZrGlobalState *global,
                                                                               const SZrAstNode *ast) {
    return module_init_find_summary_by_ast_mutable(global, ast);
}

TZrBool ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(SZrState *state,
                                                               SZrString *moduleName,
                                                               SZrAstNode *ast) {
    SZrParserModuleInitCache *cache;
    SZrParserModuleInitSummary *summary;

    if (state == ZR_NULL || moduleName == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }
    module_init_trace("prepare current module '%s' ast=%p",
                      ZrCore_String_GetNativeString(moduleName),
                      (void *)ast);

    cache = module_init_get_cache(state->global, ZR_TRUE);
    if (cache == ZR_NULL) {
        return ZR_FALSE;
    }

    summary = module_init_find_summary_mutable(state->global, moduleName);
    if (summary == ZR_NULL) {
        SZrParserModuleInitSummary newSummary;
        ZrCore_Memory_RawSet(&newSummary, 0, sizeof(newSummary));
        newSummary.moduleName = moduleName;
        newSummary.astIdentity = ast;
        newSummary.state = ZR_PARSER_MODULE_INIT_SUMMARY_BUILDING;
        ZrCore_Array_Construct(&newSummary.staticImports);
        ZrCore_Array_Construct(&newSummary.exports);
        ZrCore_Array_Construct(&newSummary.bindings);
        ZrCore_Array_Construct(&newSummary.entryEffects);
        ZrCore_Array_Construct(&newSummary.exportedCallableSummaries);
        ZrCore_Array_Push(state, &cache->summaries, &newSummary);
        summary = module_init_find_summary_mutable(state->global, moduleName);
    }

    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    if (summary->astIdentity != ast || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED ||
        summary->hasAnalysis) {
        if (!module_init_reset_summary(state, summary, moduleName, ast)) {
            return ZR_FALSE;
        }
    }

    summary->moduleName = moduleName;
    summary->astIdentity = ast;
    summary->ownsAst = ZR_FALSE;
    if (!summary->hasPrescan && !module_init_prescan_source_summary(state, summary, ast)) {
        module_init_summary_set_error(summary, &ast->location, "failed to prescan source module for init analysis");
        return ZR_FALSE;
    }

    module_init_trace("prepare current module ok '%s' summary=%p",
                      ZrCore_String_GetNativeString(moduleName),
                      (void *)summary);
    return ZR_TRUE;
}

void ZrParser_ModuleInitAnalysis_ClearAstIdentity(SZrGlobalState *global, const SZrAstNode *ast) {
    SZrParserModuleInitCache *cache;
    TZrSize index;

    if (global == ZR_NULL || ast == ZR_NULL) {
        return;
    }

    cache = module_init_get_cache(global, ZR_FALSE);
    if (cache == ZR_NULL) {
        return;
    }

    for (index = 0; index < cache->summaries.length; ++index) {
        SZrParserModuleInitSummary *summary =
                (SZrParserModuleInitSummary *)ZrCore_Array_Get(&cache->summaries, index);
        if (summary != ZR_NULL && summary->astIdentity == ast) {
            summary->astIdentity = ZR_NULL;
        }
    }
}

TZrBool ZrParser_ModuleInitAnalysis_EnsureSummary(SZrCompilerState *cs, SZrString *moduleName) {
    SZrParserModuleInitSummary *summary;
    SZrParserModuleInitCache *cache;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }
    module_init_trace("ensure summary '%s'", ZrCore_String_GetNativeString(moduleName));

    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary != ZR_NULL) {
        if (summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
            return ZR_FALSE;
        }
        if (summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_READY || summary->hasAnalysis || summary->hasPrescan) {
            return ZR_TRUE;
        }
    }

    cache = module_init_get_cache(cs->state->global, ZR_TRUE);
    if (cache == ZR_NULL) {
        return ZR_FALSE;
    }

    if (summary == ZR_NULL) {
        SZrParserModuleInitSummary newSummary;
        ZrCore_Memory_RawSet(&newSummary, 0, sizeof(newSummary));
        newSummary.moduleName = moduleName;
        newSummary.state = ZR_PARSER_MODULE_INIT_SUMMARY_BUILDING;
        ZrCore_Array_Construct(&newSummary.staticImports);
        ZrCore_Array_Construct(&newSummary.exports);
        ZrCore_Array_Construct(&newSummary.bindings);
        ZrCore_Array_Construct(&newSummary.entryEffects);
        ZrCore_Array_Construct(&newSummary.exportedCallableSummaries);
        ZrCore_Array_Push(cs->state, &cache->summaries, &newSummary);
        summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    }

    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!module_init_load_summary_from_source_loader(cs, summary, moduleName)) {
        module_init_trace("ensure summary load failed '%s'", ZrCore_String_GetNativeString(moduleName));
        summary = module_init_find_summary_mutable(cs->state->global, moduleName);
        if (summary != ZR_NULL && summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
            module_init_summary_set_error(summary, ZR_NULL, "failed to build module init summary");
        }
        return ZR_FALSE;
    }

    summary = module_init_find_summary_mutable(cs->state->global, moduleName);
    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    module_init_trace("ensure summary done '%s' state=%d hasAnalysis=%d hasPrescan=%d",
                      ZrCore_String_GetNativeString(moduleName),
                      (int)summary->state,
                      (int)summary->hasAnalysis,
                      (int)summary->hasPrescan);
    return summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED;
}

static TZrBool module_init_copy_summary_to_function(SZrCompilerState *cs,
                                                    SZrFunction *function,
                                                    const SZrParserModuleInitSummary *summary) {
    TZrSize index;

    if (cs == ZR_NULL || function == ZR_NULL || summary == ZR_NULL) {
        return ZR_FALSE;
    }

    if (summary->staticImports.length > 0) {
        function->staticImports =
                (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                              sizeof(SZrString *) * summary->staticImports.length,
                                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->staticImports == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0; index < summary->staticImports.length; ++index) {
            function->staticImports[index] =
                    *(SZrString **)ZrCore_Array_Get((SZrArray *)&summary->staticImports, index);
        }
        function->staticImportLength = (TZrUInt32)summary->staticImports.length;
    }

    if (summary->entryEffects.length > 0) {
        function->moduleEntryEffects =
                (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                           sizeof(SZrFunctionModuleEffect) * summary->entryEffects.length,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->moduleEntryEffects == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->moduleEntryEffects,
                              summary->entryEffects.head,
                              sizeof(SZrFunctionModuleEffect) * summary->entryEffects.length);
        function->moduleEntryEffectLength = (TZrUInt32)summary->entryEffects.length;
    }

    if (summary->exportedCallableSummaries.length > 0) {
        function->exportedCallableSummaries =
                (SZrFunctionCallableSummary *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                              sizeof(SZrFunctionCallableSummary) * summary->exportedCallableSummaries.length,
                                                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exportedCallableSummaries == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(function->exportedCallableSummaries,
                             0,
                             sizeof(SZrFunctionCallableSummary) * summary->exportedCallableSummaries.length);
        for (index = 0; index < summary->exportedCallableSummaries.length; ++index) {
            const SZrModuleInitCallableSummary *sourceSummary =
                    (const SZrModuleInitCallableSummary *)ZrCore_Array_Get((SZrArray *)&summary->exportedCallableSummaries,
                                                                           index);
            SZrFunctionCallableSummary *targetSummary = &function->exportedCallableSummaries[index];
            targetSummary->name = sourceSummary->name;
            targetSummary->callableChildIndex = sourceSummary->callableChildIndex;
            targetSummary->effectCount = sourceSummary->effectCount;
            if (sourceSummary->effectCount > 0 && sourceSummary->effects != ZR_NULL) {
                targetSummary->effects =
                        (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                   sizeof(SZrFunctionModuleEffect) * sourceSummary->effectCount,
                                                                                   ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                if (targetSummary->effects == ZR_NULL) {
                    return ZR_FALSE;
                }
                ZrCore_Memory_RawCopy(targetSummary->effects,
                                      sourceSummary->effects,
                                      sizeof(SZrFunctionModuleEffect) * sourceSummary->effectCount);
            }
        }
        function->exportedCallableSummaryLength = (TZrUInt32)summary->exportedCallableSummaries.length;
    }

    return ZR_TRUE;
}

static TZrBool module_init_build_top_level_callable_bindings(SZrCompilerState *cs,
                                                             SZrFunction *function,
                                                             SZrAstNode *ast) {
    TZrSize functionCount = 0;
    TZrSize index;

    if (cs == ZR_NULL || function == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement != ZR_NULL && statement->type == ZR_AST_FUNCTION_DECLARATION &&
            statement->data.functionDeclaration.name != ZR_NULL &&
            statement->data.functionDeclaration.name->name != ZR_NULL) {
            functionCount++;
        }
    }

    if (functionCount == 0) {
        return ZR_TRUE;
    }

    function->topLevelCallableBindings =
            (SZrFunctionTopLevelCallableBinding *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                  sizeof(SZrFunctionTopLevelCallableBinding) * functionCount,
                                                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->topLevelCallableBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(function->topLevelCallableBindings,
                         0,
                         sizeof(SZrFunctionTopLevelCallableBinding) * functionCount);
    function->topLevelCallableBindingLength = (TZrUInt32)functionCount;

    functionCount = 0;
    for (index = 0; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        SZrString *callableName;
        TZrUInt32 slotIndex = ZR_PARSER_SLOT_NONE;
        TZrUInt32 childIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        TZrUInt32 localIndex;
        TZrUInt32 exportedIndex;

        if (statement == ZR_NULL || statement->type != ZR_AST_FUNCTION_DECLARATION ||
            statement->data.functionDeclaration.name == ZR_NULL ||
            statement->data.functionDeclaration.name->name == ZR_NULL) {
            continue;
        }

        callableName = statement->data.functionDeclaration.name->name;
        for (localIndex = 0; localIndex < function->localVariableLength; ++localIndex) {
            SZrFunctionLocalVariable *local = &function->localVariableList[localIndex];
            if (local->name != ZR_NULL && ZrCore_String_Equal(local->name, callableName)) {
                slotIndex = local->stackSlot;
                break;
            }
        }
        for (childIndex = 0; childIndex < function->childFunctionLength; ++childIndex) {
            if (function->childFunctionList[childIndex].functionName != ZR_NULL &&
                ZrCore_String_Equal(function->childFunctionList[childIndex].functionName, callableName)) {
                break;
            }
        }
        if (childIndex >= function->childFunctionLength) {
            childIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        }

        function->topLevelCallableBindings[functionCount].name = callableName;
        function->topLevelCallableBindings[functionCount].stackSlot = slotIndex;
        function->topLevelCallableBindings[functionCount].callableChildIndex = childIndex;
        function->topLevelCallableBindings[functionCount].accessModifier = ZR_ACCESS_PUBLIC;
        function->topLevelCallableBindings[functionCount].exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
        function->topLevelCallableBindings[functionCount].readiness = ZR_MODULE_EXPORT_READY_DECLARATION;

        for (exportedIndex = 0; exportedIndex < function->exportedVariableLength; ++exportedIndex) {
            SZrFunctionExportedVariable *exported = &function->exportedVariables[exportedIndex];
            if (exported->name != ZR_NULL && ZrCore_String_Equal(exported->name, callableName)) {
                function->topLevelCallableBindings[functionCount].accessModifier = exported->accessModifier;
                exported->callableChildIndex = childIndex;
                break;
            }
        }
        functionCount++;
    }

    return ZR_TRUE;
}

TZrBool ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(SZrCompilerState *cs,
                                                                SZrString *moduleName,
                                                                SZrFunction *function) {
    SZrParserModuleInitSummary *summary;

    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    summary = moduleName != ZR_NULL
                      ? module_init_find_summary_mutable(cs->state->global, moduleName)
                      : module_init_find_summary_by_ast_mutable(cs->state->global, cs->currentAst);
    if (summary == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!summary->hasAnalysis) {
        if (!summary->hasPrescan && !module_init_prescan_source_summary(cs->state, summary, cs->currentAst)) {
            module_init_summary_set_error(summary,
                                          cs->currentAst != ZR_NULL ? &cs->currentAst->location : ZR_NULL,
                                          "failed to prescan current source module");
            return ZR_FALSE;
        }
        if (!module_init_analyze_source_summary(cs, summary, cs->currentAst) ||
            !module_init_validate_summary(cs, summary)) {
            return ZR_FALSE;
        }
        summary = moduleName != ZR_NULL
                          ? module_init_find_summary_mutable(cs->state->global, moduleName)
                          : module_init_find_summary_by_ast_mutable(cs->state->global, cs->currentAst);
        if (summary == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    if (summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
        ZrParser_Compiler_Error(cs,
                                summary->errorMessage[0] != '\0' ? summary->errorMessage : "module init analysis failed",
                                summary->errorLocation);
        return ZR_FALSE;
    }

    if (!module_init_copy_summary_to_function(cs, function, summary) ||
        !module_init_build_top_level_callable_bindings(cs, function, cs->currentAst)) {
        ZrParser_Compiler_Error(cs, "failed to attach module init analysis metadata", cs->currentAst->location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

void ZrParser_ModuleInitAnalysis_GlobalCleanup(SZrGlobalState *global, TZrPtr opaqueState) {
    SZrParserModuleInitCache *cache = (SZrParserModuleInitCache *)opaqueState;
    TZrSize index;
    SZrState *state;

    if (global == ZR_NULL || cache == ZR_NULL) {
        return;
    }

    state = global->mainThreadState;
    for (index = 0; index < cache->summaries.length; ++index) {
        SZrParserModuleInitSummary *summary =
                (SZrParserModuleInitSummary *)ZrCore_Array_Get(&cache->summaries, index);
        if (summary != ZR_NULL) {
            module_init_free_summary(global, summary);
        }
    }

    if (state != ZR_NULL && cache->summaries.isValid && cache->summaries.head != ZR_NULL) {
        ZrCore_Array_Free(state, &cache->summaries);
    }
    ZrCore_Memory_RawFreeWithType(global, cache, sizeof(*cache), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
}
