//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_object_conf.h"

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/constant_reference.h"

static const TZrChar *writer_intermediate_primitive_type_name(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
            return "i8";
        case ZR_VALUE_TYPE_INT16:
            return "i16";
        case ZR_VALUE_TYPE_INT32:
            return "i32";
        case ZR_VALUE_TYPE_INT64:
            return "int";
        case ZR_VALUE_TYPE_UINT8:
            return "u8";
        case ZR_VALUE_TYPE_UINT16:
            return "u16";
        case ZR_VALUE_TYPE_UINT32:
            return "u32";
        case ZR_VALUE_TYPE_UINT64:
            return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return "function";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        default:
            return "object";
    }
}

static void writer_intermediate_write_indent(FILE *file, TZrUInt32 indentLevel) {
    if (file == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < indentLevel; index++) {
        fputc(' ', file);
    }
}

static void writer_intermediate_format_type_ref(const SZrFunctionTypedTypeRef *typeRef,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    const TZrChar *baseName;
    TZrNativeString userTypeName;
    TZrNativeString elementTypeName;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (typeRef == ZR_NULL) {
        snprintf(buffer, bufferSize, "object");
        return;
    }

    if (typeRef->isArray) {
        if (typeRef->elementTypeName != ZR_NULL) {
            elementTypeName = ZrCore_String_GetNativeString(typeRef->elementTypeName);
            snprintf(buffer, bufferSize, "%s[]", elementTypeName != ZR_NULL ? elementTypeName : "object");
        } else {
            snprintf(buffer, bufferSize, "%s[]", writer_intermediate_primitive_type_name(typeRef->elementBaseType));
        }
        return;
    }

    userTypeName = typeRef->typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeRef->typeName) : ZR_NULL;
    if (userTypeName != ZR_NULL) {
        snprintf(buffer, bufferSize, "%s", userTypeName);
        return;
    }

    baseName = writer_intermediate_primitive_type_name(typeRef->baseType);
    snprintf(buffer, bufferSize, "%s", baseName);
}

static void writer_intermediate_write_metadata_parameter_default_value(FILE *file,
                                                                      SZrState *state,
                                                                      const SZrFunctionMetadataParameter *parameter) {
    SZrTypeValue defaultValueCopy;
    SZrString *debugString;
    TZrNativeString nativeDebugString;

    if (file == ZR_NULL || state == ZR_NULL || parameter == ZR_NULL || !parameter->hasDefaultValue) {
        return;
    }

    defaultValueCopy = parameter->defaultValue;
    debugString = ZrCore_Value_ToDebugString(state, &defaultValueCopy);
    nativeDebugString = debugString != ZR_NULL ? ZrCore_String_GetNativeString(debugString) : ZR_NULL;
    fprintf(file, " = %s", nativeDebugString != ZR_NULL ? nativeDebugString : "<default>");
}

static void writer_intermediate_write_metadata_parameters(FILE *file,
                                                          SZrState *state,
                                                          const SZrFunctionMetadataParameter *parameters,
                                                          TZrUInt32 parameterCount) {
    if (file == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionMetadataParameter *parameter = &parameters[index];
        TZrNativeString name = parameter->name != ZR_NULL ? ZrCore_String_GetNativeString(parameter->name) : ZR_NULL;
        TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

        if (index > 0) {
            fprintf(file, ", ");
        }

        writer_intermediate_format_type_ref(&parameter->type, typeBuffer, sizeof(typeBuffer));
        if (name != ZR_NULL && name[0] != '\0') {
            fprintf(file, "%s: %s", name, typeBuffer);
        } else {
            fprintf(file, "%s", typeBuffer);
        }
        writer_intermediate_write_metadata_parameter_default_value(file, state, parameter);
    }
}

static const TZrChar *writer_intermediate_module_export_kind_name(TZrUInt8 exportKind) {
    switch ((EZrModuleExportKind)exportKind) {
        case ZR_MODULE_EXPORT_KIND_VALUE:
            return "value";
        case ZR_MODULE_EXPORT_KIND_FUNCTION:
            return "function";
        case ZR_MODULE_EXPORT_KIND_TYPE:
            return "type";
        default:
            return "unknown";
    }
}

static const TZrChar *writer_intermediate_module_export_readiness_name(TZrUInt8 readiness) {
    switch ((EZrModuleExportReadiness)readiness) {
        case ZR_MODULE_EXPORT_READY_DECLARATION:
            return "declaration";
        case ZR_MODULE_EXPORT_READY_ENTRY:
            return "entry";
        default:
            return "unknown";
    }
}

static const TZrChar *writer_intermediate_module_effect_kind_name(TZrUInt8 kind) {
    switch ((EZrModuleEntryEffectKind)kind) {
        case ZR_MODULE_ENTRY_EFFECT_IMPORT_REF:
            return "IMPORT_REF";
        case ZR_MODULE_ENTRY_EFFECT_IMPORT_READ:
            return "IMPORT_READ";
        case ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL:
            return "IMPORT_CALL";
        case ZR_MODULE_ENTRY_EFFECT_LOCAL_CALL:
            return "LOCAL_CALL";
        case ZR_MODULE_ENTRY_EFFECT_LOCAL_ENTRY_BINDING_READ:
            return "LOCAL_ENTRY_BINDING_READ";
        case ZR_MODULE_ENTRY_EFFECT_DYNAMIC_UNKNOWN:
            return "DYNAMIC_UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

static void writer_intermediate_write_type_metadata(FILE *file,
                                                    SZrState *state,
                                                    SZrFunction *function,
                                                    TZrUInt32 indentLevel) {
    ZR_UNUSED_PARAMETER(state);
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "TYPE_METADATA:\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  LOCAL_BINDINGS (%u):\n", function->typedLocalBindingLength);
    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        TZrNativeString name = binding->name != ZR_NULL ? ZrCore_String_GetNativeString(binding->name) : "<unnamed>";
        TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

        writer_intermediate_format_type_ref(&binding->type, typeBuffer, sizeof(typeBuffer));
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "    [%u] %s: %s\n", binding->stackSlot, name != ZR_NULL ? name : "<unnamed>", typeBuffer);
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  EXPORTED_SYMBOLS (%u):\n", function->typedExportedSymbolLength);
    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        TZrNativeString name = symbol->name != ZR_NULL ? ZrCore_String_GetNativeString(symbol->name) : "<unnamed>";
        TZrChar valueTypeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

        writer_intermediate_format_type_ref(&symbol->valueType, valueTypeBuffer, sizeof(valueTypeBuffer));
        if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file, "    fn %s(", name != ZR_NULL ? name : "<unnamed>");
            for (TZrUInt32 paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
                TZrChar paramBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
                const SZrFunctionTypedTypeRef *parameterType =
                        symbol->parameterTypes != ZR_NULL ? &symbol->parameterTypes[paramIndex] : ZR_NULL;
                if (paramIndex > 0) {
                    fprintf(file, ", ");
                }
                writer_intermediate_format_type_ref(parameterType,
                                                    paramBuffer,
                                                    sizeof(paramBuffer));
                fprintf(file, "%s", paramBuffer);
            }
            fprintf(file,
                    "): %s [kind=%s readiness=%s child=%u]\n",
                    valueTypeBuffer,
                    writer_intermediate_module_export_kind_name(symbol->exportKind),
                    writer_intermediate_module_export_readiness_name(symbol->readiness),
                    symbol->callableChildIndex);
        } else {
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file,
                    "    var %s: %s [kind=%s readiness=%s child=%u]\n",
                    name != ZR_NULL ? name : "<unnamed>",
                    valueTypeBuffer,
                    writer_intermediate_module_export_kind_name(symbol->exportKind),
                    writer_intermediate_module_export_readiness_name(symbol->readiness),
                    symbol->callableChildIndex);
        }
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  STATIC_IMPORTS (%u):\n", function->staticImportLength);
    for (TZrUInt32 index = 0; index < function->staticImportLength; index++) {
        TZrNativeString importName =
                function->staticImports[index] != ZR_NULL ? ZrCore_String_GetNativeString(function->staticImports[index]) : "<unnamed>";
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "    import %s\n", importName != ZR_NULL ? importName : "<unnamed>");
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  MODULE_ENTRY_EFFECTS (%u):\n", function->moduleEntryEffectLength);
    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        TZrNativeString moduleName =
                effect->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(effect->moduleName) : "<unnamed>";
        TZrNativeString symbolName =
                effect->symbolName != ZR_NULL ? ZrCore_String_GetNativeString(effect->symbolName) : "<unnamed>";
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "    %s %s.%s [kind=%s readiness=%s]\n",
                writer_intermediate_module_effect_kind_name(effect->kind),
                moduleName != ZR_NULL ? moduleName : "<unnamed>",
                symbolName != ZR_NULL ? symbolName : "<unnamed>",
                writer_intermediate_module_export_kind_name(effect->exportKind),
                writer_intermediate_module_export_readiness_name(effect->readiness));
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  EXPORTED_CALLABLE_SUMMARIES (%u):\n", function->exportedCallableSummaryLength);
    for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
        SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[index];
        TZrNativeString name = summary->name != ZR_NULL ? ZrCore_String_GetNativeString(summary->name) : "<unnamed>";
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "    fn %s [child=%u effects=%u]\n",
                name != ZR_NULL ? name : "<unnamed>",
                summary->callableChildIndex,
                summary->effectCount);
        for (TZrUInt32 effectIndex = 0; effectIndex < summary->effectCount; effectIndex++) {
            SZrFunctionModuleEffect *effect = &summary->effects[effectIndex];
            TZrNativeString moduleName =
                    effect->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(effect->moduleName) : "<unnamed>";
            TZrNativeString symbolName =
                    effect->symbolName != ZR_NULL ? ZrCore_String_GetNativeString(effect->symbolName) : "<unnamed>";
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file,
                    "      %s %s.%s [kind=%s readiness=%s]\n",
                    writer_intermediate_module_effect_kind_name(effect->kind),
                    moduleName != ZR_NULL ? moduleName : "<unnamed>",
                    symbolName != ZR_NULL ? symbolName : "<unnamed>",
                    writer_intermediate_module_export_kind_name(effect->exportKind),
                    writer_intermediate_module_export_readiness_name(effect->readiness));
        }
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  TOP_LEVEL_CALLABLE_BINDINGS (%u):\n", function->topLevelCallableBindingLength);
    for (TZrUInt32 index = 0; index < function->topLevelCallableBindingLength; index++) {
        SZrFunctionTopLevelCallableBinding *binding = &function->topLevelCallableBindings[index];
        TZrNativeString name = binding->name != ZR_NULL ? ZrCore_String_GetNativeString(binding->name) : "<unnamed>";
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "    fn %s [slot=%u child=%u kind=%s readiness=%s access=%u]\n",
                name != ZR_NULL ? name : "<unnamed>",
                binding->stackSlot,
                binding->callableChildIndex,
                writer_intermediate_module_export_kind_name(binding->exportKind),
                writer_intermediate_module_export_readiness_name(binding->readiness),
                binding->accessModifier);
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  COMPILE_TIME_VARIABLES (%u):\n", function->compileTimeVariableInfoLength);
    for (TZrUInt32 index = 0; index < function->compileTimeVariableInfoLength; index++) {
        SZrFunctionCompileTimeVariableInfo *info = &function->compileTimeVariableInfos[index];
        TZrNativeString name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "<unnamed>";
        TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

        writer_intermediate_format_type_ref(&info->type, typeBuffer, sizeof(typeBuffer));
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "    var %s: %s", name != ZR_NULL ? name : "<unnamed>", typeBuffer);
        if (info->pathBindingCount > 0) {
            fprintf(file, " [bindings:");
            for (TZrUInt32 bindingIndex = 0; bindingIndex < info->pathBindingCount; bindingIndex++) {
                SZrFunctionCompileTimePathBinding *binding = &info->pathBindings[bindingIndex];
                TZrNativeString path = binding->path != ZR_NULL ? ZrCore_String_GetNativeString(binding->path) : "";
                TZrNativeString targetName =
                        binding->targetName != ZR_NULL ? ZrCore_String_GetNativeString(binding->targetName) : "<unnamed>";
                const TZrChar *targetKind =
                        binding->targetKind == ZR_COMPILE_TIME_BINDING_TARGET_DECORATOR_CLASS ? "class" : "fn";

                fprintf(file,
                        "%s%s->%s:%s",
                        bindingIndex == 0 ? " " : ", ",
                        path != ZR_NULL ? path : "",
                        targetKind,
                        targetName != ZR_NULL ? targetName : "<unnamed>");
            }
            fprintf(file, "]");
        }
        fprintf(file, "\n");
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  COMPILE_TIME_FUNCTIONS (%u):\n", function->compileTimeFunctionInfoLength);
    for (TZrUInt32 index = 0; index < function->compileTimeFunctionInfoLength; index++) {
        SZrFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[index];
        TZrNativeString name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "<unnamed>";
        TZrChar returnTypeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

        writer_intermediate_format_type_ref(&info->returnType, returnTypeBuffer, sizeof(returnTypeBuffer));
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "    fn %s(", name != ZR_NULL ? name : "<unnamed>");
        writer_intermediate_write_metadata_parameters(file, state, info->parameters, info->parameterCount);
        fprintf(file, "): %s\n", returnTypeBuffer);
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "  TESTS (%u):\n", function->testInfoLength);
    for (TZrUInt32 index = 0; index < function->testInfoLength; index++) {
        SZrFunctionTestInfo *info = &function->testInfos[index];
        TZrNativeString name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "<unnamed>";

        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "    test %s(", name != ZR_NULL ? name : "<unnamed>");
        writer_intermediate_write_metadata_parameters(file, state, info->parameters, info->parameterCount);
        if (info->hasVariableArguments) {
            if (info->parameterCount > 0) {
                fprintf(file, ", ");
            }
            fprintf(file, "...");
        }
        fprintf(file, ")\n");
    }

    fprintf(file, "\n");
}

static const TZrChar *writer_intermediate_semir_opcode_name(TZrUInt32 opcode) {
    switch ((EZrSemIrOpcode)opcode) {
        case ZR_SEMIR_OPCODE_OWN_UNIQUE:
            return "OWN_UNIQUE";
        case ZR_SEMIR_OPCODE_OWN_BORROW:
            return "OWN_BORROW";
        case ZR_SEMIR_OPCODE_OWN_LOAN:
            return "OWN_LOAN";
        case ZR_SEMIR_OPCODE_OWN_SHARE:
            return "OWN_SHARE";
        case ZR_SEMIR_OPCODE_OWN_WEAK:
            return "OWN_WEAK";
        case ZR_SEMIR_OPCODE_OWN_DETACH:
            return "OWN_DETACH";
        case ZR_SEMIR_OPCODE_OWN_UPGRADE:
            return "OWN_UPGRADE";
        case ZR_SEMIR_OPCODE_OWN_RELEASE:
            return "OWN_RELEASE";
        case ZR_SEMIR_OPCODE_TYPEOF:
            return "TYPEOF";
        case ZR_SEMIR_OPCODE_DYN_CALL:
            return "DYN_CALL";
        case ZR_SEMIR_OPCODE_DYN_TAIL_CALL:
            return "DYN_TAIL_CALL";
        case ZR_SEMIR_OPCODE_META_CALL:
            return "META_CALL";
        case ZR_SEMIR_OPCODE_META_TAIL_CALL:
            return "META_TAIL_CALL";
        case ZR_SEMIR_OPCODE_META_GET:
            return "META_GET";
        case ZR_SEMIR_OPCODE_META_SET:
            return "META_SET";
        case ZR_SEMIR_OPCODE_DYN_ITER_INIT:
            return "DYN_ITER_INIT";
        case ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT:
            return "DYN_ITER_MOVE_NEXT";
        case ZR_SEMIR_OPCODE_NOP:
        default:
            return "NOP";
    }
}

static const TZrChar *writer_intermediate_semir_effect_name(TZrUInt32 kind) {
    switch ((EZrSemIrEffectKind)kind) {
        case ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION:
            return "OWNERSHIP_TRANSITION";
        case ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME:
            return "DYNAMIC_RUNTIME";
        case ZR_SEMIR_EFFECT_KIND_NONE:
        default:
            return "NONE";
    }
}

static const TZrChar *writer_intermediate_semir_ownership_name(TZrUInt32 state) {
    switch ((EZrSemIrOwnershipState)state) {
        case ZR_SEMIR_OWNERSHIP_STATE_UNIQUE:
            return "Unique";
        case ZR_SEMIR_OWNERSHIP_STATE_SHARED:
            return "Shared";
        case ZR_SEMIR_OWNERSHIP_STATE_WEAK:
            return "Weak";
        case ZR_SEMIR_OWNERSHIP_STATE_BORROW_SHARED:
            return "BorrowShared";
        case ZR_SEMIR_OWNERSHIP_STATE_BORROW_MUT:
            return "BorrowMut";
        case ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC:
        default:
            return "PlainGc";
    }
}

static void writer_intermediate_write_semir_metadata(FILE *file,
                                                     SZrState *state,
                                                     SZrFunction *function,
                                                     TZrUInt32 indentLevel) {
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "TYPE_TABLE (%u):\n", function->semIrTypeTableLength);
    for (TZrUInt32 index = 0; index < function->semIrTypeTableLength; index++) {
        TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
        writer_intermediate_format_type_ref(&function->semIrTypeTable[index], typeBuffer, sizeof(typeBuffer));
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "  [%u] %s\n", index, typeBuffer);
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "OWNERSHIP_TABLE (%u):\n", function->semIrOwnershipTableLength);
    for (TZrUInt32 index = 0; index < function->semIrOwnershipTableLength; index++) {
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] %s\n",
                index,
                writer_intermediate_semir_ownership_name(function->semIrOwnershipTable[index].state));
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "EFFECT_TABLE (%u):\n", function->semIrEffectTableLength);
    for (TZrUInt32 index = 0; index < function->semIrEffectTableLength; index++) {
        SZrSemIrEffectEntry *entry = &function->semIrEffectTable[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] %s inst=%u from=%u to=%u\n",
                index,
                writer_intermediate_semir_effect_name(entry->kind),
                entry->instructionIndex,
                entry->ownershipInputIndex,
                entry->ownershipOutputIndex);
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "BLOCK_GRAPH (%u):\n", function->semIrBlockTableLength);
    for (TZrUInt32 index = 0; index < function->semIrBlockTableLength; index++) {
        SZrSemIrBlockEntry *entry = &function->semIrBlockTable[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] block=%u first=%u count=%u\n",
                index,
                entry->blockId,
                entry->firstInstructionIndex,
                entry->instructionCount);
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "SEMIR (%u):\n", function->semIrInstructionLength);
    for (TZrUInt32 index = 0; index < function->semIrInstructionLength; index++) {
        SZrSemIrInstruction *entry = &function->semIrInstructions[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] %s exec=%u type=%u effect=%u dst=%u op0=%u op1=%u deopt=%u\n",
                index,
                writer_intermediate_semir_opcode_name(entry->opcode),
                entry->execInstructionIndex,
                entry->typeTableIndex,
                entry->effectTableIndex,
                entry->destinationSlot,
                entry->operand0,
                entry->operand1,
                entry->deoptId);
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "DEOPT_MAP (%u):\n", function->semIrDeoptTableLength);
    for (TZrUInt32 index = 0; index < function->semIrDeoptTableLength; index++) {
        SZrSemIrDeoptEntry *entry = &function->semIrDeoptTable[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "  [%u] deopt=%u exec=%u\n", index, entry->deoptId, entry->execInstructionIndex);
    }
    fprintf(file, "\n");

    ZR_UNUSED_PARAMETER(state);
}

static const TZrChar *writer_intermediate_callsite_cache_kind_name(TZrUInt32 kind) {
    switch ((EZrFunctionCallSiteCacheKind)kind) {
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET:
            return "META_GET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET:
            return "META_SET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC:
            return "META_GET_STATIC";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC:
            return "META_SET_STATIC";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL:
            return "META_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL:
            return "DYN_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL:
            return "META_TAIL_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL:
            return "DYN_TAIL_CALL";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET:
            return "MEMBER_GET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET:
            return "MEMBER_SET";
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE:
        default:
            return "NONE";
    }
}

static void writer_intermediate_write_callsite_cache_table(FILE *file, SZrFunction *function, TZrUInt32 indentLevel) {
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "CALLSITE_CACHE_TABLE (%u):\n", function->callSiteCacheLength);
    for (TZrUInt32 index = 0; index < function->callSiteCacheLength; index++) {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] kind=%s inst=%u member=%u deopt=%u args=%u hits=%u misses=%u\n",
                index,
                writer_intermediate_callsite_cache_kind_name(entry->kind),
                entry->instructionIndex,
                entry->memberEntryIndex,
                entry->deoptId,
                entry->argumentCount,
                entry->runtimeHitCount,
                entry->runtimeMissCount);
        for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
            SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file,
                    "    PIC[%u] receiver_version=%u owner_version=%u descriptor=%u static=%s has_function=%s\n",
                    slotIndex,
                    slot->cachedReceiverVersion,
                    slot->cachedOwnerVersion,
                    slot->cachedDescriptorIndex,
                    slot->cachedIsStatic ? "true" : "false",
                    slot->cachedFunction != ZR_NULL ? "true" : "false");
        }
    }
    fprintf(file, "\n");
}

static void writer_intermediate_write_eh_table(FILE *file, SZrFunction *function, TZrUInt32 indentLevel) {
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "EH_TABLE (%u):\n", function->exceptionHandlerCount);
    for (TZrUInt32 index = 0; index < function->exceptionHandlerCount; index++) {
        SZrFunctionExceptionHandlerInfo *handler = &function->exceptionHandlerList[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] protected_start=%u finally=%u after_finally=%u catches=%u count=%u has_finally=%s\n",
                index,
                (TZrUInt32)handler->protectedStartInstructionOffset,
                (TZrUInt32)handler->finallyTargetInstructionOffset,
                (TZrUInt32)handler->afterFinallyInstructionOffset,
                handler->catchClauseStartIndex,
                handler->catchClauseCount,
                handler->hasFinally ? "true" : "false");
    }
    fprintf(file, "\n");
}

static void writer_intermediate_write_constant(FILE *file, SZrState *state, const SZrTypeValue *constant) {
    if (file == ZR_NULL || constant == ZR_NULL) {
        return;
    }

    switch (constant->type) {
        case ZR_VALUE_TYPE_NULL:
            fprintf(file, "null\n");
            break;

        case ZR_VALUE_TYPE_BOOL:
            fprintf(file, "bool: %s\n", constant->value.nativeObject.nativeBool ? "true" : "false");
            break;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            fprintf(file, "int: %lld\n", (long long) constant->value.nativeObject.nativeInt64);
            break;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            fprintf(file, "float: %f\n", constant->value.nativeObject.nativeDouble);
            break;

        case ZR_VALUE_TYPE_STRING:
            if (constant->value.object == ZR_NULL) {
                fprintf(file, "string: \"\"\n");
            } else {
                SZrRawObject *rawObj = constant->value.object;
                if (rawObj->type == ZR_RAW_OBJECT_TYPE_STRING) {
                    SZrString *str = ZR_CAST_STRING(state, rawObj);
                    TZrNativeString strStr = ZrCore_String_GetNativeString(str);
                    fprintf(file, "string: \"%s\"\n", strStr != ZR_NULL ? strStr : "");
                } else {
                    fprintf(file, "string: \"\"\n");
                }
            }
            break;

        case ZR_VALUE_TYPE_FUNCTION:
            fprintf(file, "function\n");
            break;

        case ZR_VALUE_TYPE_CLOSURE:
            if (constant->value.object != ZR_NULL) {
                SZrRawObject *rawObj = constant->value.object;
                fprintf(file, "%sclosure\n", rawObj->isNative ? "native " : "");
            } else {
                fprintf(file, "closure\n");
            }
            break;

        case ZR_VALUE_TYPE_NATIVE_POINTER:
            fprintf(file, "native pointer\n");
            break;

        default:
            fprintf(file, "unknown type: %u\n", (TZrUInt32) constant->type);
            break;
    }
}

static void writer_intermediate_write_nested_function(FILE *file,
                                                      SZrState *state,
                                                      SZrFunction *function,
                                                      TZrUInt32 indentLevel) {
    if (file == ZR_NULL || function == ZR_NULL) {
        return;
    }

    if (function->functionName != ZR_NULL) {
        TZrNativeString funcNameStr = ZrCore_String_GetNativeString(function->functionName);
        if (funcNameStr != ZR_NULL) {
            TZrSize nameLen;
            if (function->functionName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                nameLen = function->functionName->shortStringLength;
            } else {
                nameLen = function->functionName->longStringLength;
            }
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file, "NAME: %.*s\n", (int)nameLen, funcNameStr);
        } else {
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file, "NAME: <unnamed>\n");
        }
    } else {
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "NAME: <anonymous>\n");
    }
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "START_LINE: %u\n", function->lineInSourceStart);
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "END_LINE: %u\n", function->lineInSourceEnd);
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "PARAMETERS: %u\n", function->parameterCount);
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "HAS_VAR_ARGS: %s\n", function->hasVariableArguments ? "true" : "false");
    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "STACK_SIZE: %u\n", function->stackSize);
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "CONSTANTS (%u):\n", function->constantValueLength);
    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        SZrTypeValue *constant = &function->constantValueList[index];
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "  [%u] ", index);
        writer_intermediate_write_constant(file, state, constant);
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "LOCAL_VARIABLES (%u):\n", function->localVariableLength);
    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[index];
        TZrNativeString nameStr = local->name != ZR_NULL ? ZrCore_String_GetNativeString(local->name) : "<unnamed>";
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] %s: offset_activate=%u, offset_dead=%u\n",
                index,
                nameStr,
                (TZrUInt32)local->offsetActivate,
                (TZrUInt32)local->offsetDead);
    }
    fprintf(file, "\n");

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "CLOSURE_VARIABLES (%u):\n", function->closureValueLength);
    for (TZrUInt32 index = 0; index < function->closureValueLength; index++) {
        SZrFunctionClosureVariable *closure = &function->closureValueList[index];
        TZrNativeString nameStr = closure->name != ZR_NULL ? ZrCore_String_GetNativeString(closure->name) : "<unnamed>";
        const TZrChar *typeName = "UNKNOWN";

        switch (closure->valueType) {
            case ZR_VALUE_TYPE_NULL: typeName = "NULL"; break;
            case ZR_VALUE_TYPE_BOOL: typeName = "BOOL"; break;
            case ZR_VALUE_TYPE_INT8: typeName = "INT8"; break;
            case ZR_VALUE_TYPE_INT16: typeName = "INT16"; break;
            case ZR_VALUE_TYPE_INT32: typeName = "INT32"; break;
            case ZR_VALUE_TYPE_INT64: typeName = "INT64"; break;
            case ZR_VALUE_TYPE_UINT8: typeName = "UINT8"; break;
            case ZR_VALUE_TYPE_UINT16: typeName = "UINT16"; break;
            case ZR_VALUE_TYPE_UINT32: typeName = "UINT32"; break;
            case ZR_VALUE_TYPE_UINT64: typeName = "UINT64"; break;
            case ZR_VALUE_TYPE_FLOAT: typeName = "FLOAT"; break;
            case ZR_VALUE_TYPE_DOUBLE: typeName = "DOUBLE"; break;
            case ZR_VALUE_TYPE_STRING: typeName = "STRING"; break;
            case ZR_VALUE_TYPE_FUNCTION: typeName = "FUNCTION"; break;
            case ZR_VALUE_TYPE_CLOSURE: typeName = "CLOSURE"; break;
            case ZR_VALUE_TYPE_OBJECT: typeName = "OBJECT"; break;
            case ZR_VALUE_TYPE_ARRAY: typeName = "ARRAY"; break;
            default: typeName = "UNKNOWN"; break;
        }

        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file,
                "  [%u] %s: in_stack=%s, index=%u, type=%s\n",
                index,
                nameStr,
                closure->inStack ? "true" : "false",
                closure->index,
                typeName);
    }
    fprintf(file, "\n");

    writer_intermediate_write_type_metadata(file, state, function, indentLevel);
    writer_intermediate_write_semir_metadata(file, state, function, indentLevel);
    writer_intermediate_write_callsite_cache_table(file, function, indentLevel);
    writer_intermediate_write_eh_table(file, function, indentLevel);

    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "PROTOTYPES (%u):\n", function->prototypeCount);
        ZrCore_Debug_PrintPrototypesFromData(state, function, file);
        fprintf(file, "\n");
    }

    writer_intermediate_write_indent(file, indentLevel);
    fprintf(file, "INSTRUCTIONS (%u):\n", function->instructionsLength);
    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *inst = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
        TZrUInt16 operandExtra = inst->instruction.operandExtra;

        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "  [%u] ", index);

        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT): fprintf(file, "GET_CONSTANT"); break;
            case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL): fprintf(file, "RESET_STACK_NULL"); break;
            case ZR_INSTRUCTION_ENUM(SET_STACK): fprintf(file, "SET_STACK"); break;
            case ZR_INSTRUCTION_ENUM(GET_STACK): fprintf(file, "GET_STACK"); break;
            case ZR_INSTRUCTION_ENUM(ADD_INT): fprintf(file, "ADD_INT"); break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST): fprintf(file, "ADD_INT_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST): fprintf(file, "ADD_INT_CONST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST): fprintf(file, "ADD_INT_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED): fprintf(file, "ADD_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST): fprintf(file, "ADD_SIGNED_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST): fprintf(file, "ADD_SIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST): fprintf(file, "ADD_SIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST): fprintf(file, "ADD_SIGNED_LOAD_CONST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST): fprintf(file, "ADD_SIGNED_LOAD_STACK_CONST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK): fprintf(file, "ADD_SIGNED_LOAD_STACK"); break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST): fprintf(file, "ADD_SIGNED_LOAD_STACK_LOAD_CONST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST): fprintf(file, "SUB_SIGNED_LOAD_CONST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST): fprintf(file, "SUB_SIGNED_LOAD_STACK_CONST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED): fprintf(file, "ADD_UNSIGNED"); break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST): fprintf(file, "ADD_UNSIGNED_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST): fprintf(file, "ADD_UNSIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST): fprintf(file, "ADD_UNSIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_INT): fprintf(file, "SUB_INT"); break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST): fprintf(file, "SUB_INT_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST): fprintf(file, "SUB_INT_CONST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST): fprintf(file, "SUB_INT_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED): fprintf(file, "SUB_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST): fprintf(file, "SUB_SIGNED_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST): fprintf(file, "SUB_SIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST): fprintf(file, "SUB_SIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED): fprintf(file, "SUB_UNSIGNED"); break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST): fprintf(file, "SUB_UNSIGNED_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST): fprintf(file, "SUB_UNSIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST): fprintf(file, "SUB_UNSIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED): fprintf(file, "MUL_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST): fprintf(file, "MUL_SIGNED_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST): fprintf(file, "MUL_SIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST): fprintf(file, "MUL_SIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST): fprintf(file, "MUL_SIGNED_LOAD_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST): fprintf(file, "MUL_SIGNED_LOAD_STACK_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST): fprintf(file, "MUL_UNSIGNED_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST): fprintf(file, "MUL_UNSIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST): fprintf(file, "MUL_UNSIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED): fprintf(file, "DIV_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST): fprintf(file, "DIV_SIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST): fprintf(file, "DIV_SIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST): fprintf(file, "DIV_SIGNED_LOAD_CONST"); break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST): fprintf(file, "DIV_SIGNED_LOAD_STACK_CONST"); break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST): fprintf(file, "DIV_UNSIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST): fprintf(file, "DIV_UNSIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED): fprintf(file, "MOD_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST): fprintf(file, "MOD_SIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST): fprintf(file, "MOD_SIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST): fprintf(file, "MOD_SIGNED_LOAD_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST): fprintf(file, "MOD_SIGNED_LOAD_STACK_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST): fprintf(file, "MOD_UNSIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST): fprintf(file, "MOD_UNSIGNED_CONST_PLAIN_DEST"); break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN): fprintf(file, "FUNCTION_RETURN"); break;
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT): fprintf(file, "CREATE_OBJECT"); break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY): fprintf(file, "CREATE_ARRAY"); break;
            case ZR_INSTRUCTION_ENUM(OWN_UNIQUE): fprintf(file, "OWN_UNIQUE"); break;
            case ZR_INSTRUCTION_ENUM(OWN_BORROW): fprintf(file, "OWN_BORROW"); break;
            case ZR_INSTRUCTION_ENUM(OWN_LOAN): fprintf(file, "OWN_LOAN"); break;
            case ZR_INSTRUCTION_ENUM(OWN_SHARE): fprintf(file, "OWN_SHARE"); break;
            case ZR_INSTRUCTION_ENUM(OWN_WEAK): fprintf(file, "OWN_WEAK"); break;
            case ZR_INSTRUCTION_ENUM(OWN_DETACH): fprintf(file, "OWN_DETACH"); break;
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE): fprintf(file, "OWN_UPGRADE"); break;
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE): fprintf(file, "OWN_RELEASE"); break;
            case ZR_INSTRUCTION_ENUM(TYPEOF): fprintf(file, "TYPEOF"); break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL): fprintf(file, "DYN_CALL"); break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL): fprintf(file, "DYN_TAIL_CALL"); break;
            case ZR_INSTRUCTION_ENUM(META_CALL): fprintf(file, "META_CALL"); break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL): fprintf(file, "META_TAIL_CALL"); break;
            case ZR_INSTRUCTION_ENUM(META_GET): fprintf(file, "META_GET"); break;
            case ZR_INSTRUCTION_ENUM(META_SET): fprintf(file, "META_SET"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED): fprintf(file, "SUPER_META_GET_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED): fprintf(file, "SUPER_META_SET_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED): fprintf(file, "SUPER_META_GET_STATIC_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED): fprintf(file, "SUPER_META_SET_STATIC_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT): fprintf(file, "DYN_ITER_INIT"); break;
            case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT): fprintf(file, "DYN_ITER_MOVE_NEXT"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS): fprintf(file, "SUPER_FUNCTION_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS): fprintf(file, "SUPER_DYN_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS): fprintf(file, "SUPER_META_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED): fprintf(file, "SUPER_DYN_CALL_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED): fprintf(file, "SUPER_META_CALL_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS): fprintf(file, "SUPER_FUNCTION_TAIL_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS): fprintf(file, "SUPER_DYN_TAIL_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS): fprintf(file, "SUPER_META_TAIL_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED): fprintf(file, "SUPER_DYN_TAIL_CALL_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED): fprintf(file, "SUPER_META_TAIL_CALL_CACHED"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL): fprintf(file, "KNOWN_VM_CALL"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL): fprintf(file, "KNOWN_VM_MEMBER_CALL"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8): fprintf(file, "KNOWN_VM_MEMBER_CALL_LOAD1_U8"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL): fprintf(file, "KNOWN_VM_TAIL_CALL"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL): fprintf(file, "KNOWN_NATIVE_CALL"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL): fprintf(file, "KNOWN_NATIVE_MEMBER_CALL"); break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL): fprintf(file, "KNOWN_NATIVE_TAIL_CALL"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS): fprintf(file, "SUPER_KNOWN_VM_CALL_NO_ARGS"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_NATIVE_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL): fprintf(file, "LOGICAL_EQUAL_BOOL"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL): fprintf(file, "LOGICAL_NOT_EQUAL_BOOL"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED): fprintf(file, "LOGICAL_EQUAL_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST): fprintf(file, "LOGICAL_EQUAL_SIGNED_CONST"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED): fprintf(file, "LOGICAL_NOT_EQUAL_SIGNED"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED): fprintf(file, "LOGICAL_EQUAL_UNSIGNED"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED): fprintf(file, "LOGICAL_NOT_EQUAL_UNSIGNED"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT): fprintf(file, "LOGICAL_EQUAL_FLOAT"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT): fprintf(file, "LOGICAL_NOT_EQUAL_FLOAT"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING): fprintf(file, "LOGICAL_EQUAL_STRING"); break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING): fprintf(file, "LOGICAL_NOT_EQUAL_STRING"); break;
            case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                fprintf(file, "SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                fprintf(file, "SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                fprintf(file, "JUMP_IF_GREATER_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
                fprintf(file, "JUMP_IF_NOT_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
                fprintf(file, "JUMP_IF_NOT_EQUAL_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(NOP):
                fprintf(file, "NOP");
                break;
            default:
                fprintf(file, "OPCODE_%u", (TZrUInt32)opcode);
                break;
        }

        fprintf(file, " (extra=%u", operandExtra);
        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            case ZR_INSTRUCTION_ENUM(JUMP):
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
            case ZR_INSTRUCTION_ENUM(THROW):
                fprintf(file, ", operand=%d", inst->instruction.operand.operand2[0]);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                fprintf(file,
                        ", left_slot=%u, right_slot=%u, jump_offset=%d",
                        inst->instruction.operandExtra,
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
                fprintf(file,
                        ", left_slot=%u, right_slot=%u, jump_offset=%d",
                        inst->instruction.operandExtra,
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
                fprintf(file,
                        ", left_slot=%u, constant_index=%u, jump_offset=%d",
                        inst->instruction.operandExtra,
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;

            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
                fprintf(file,
                        ", left_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand1[0],
                        inst->instruction.operand.operand1[1]);
                break;

            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
                fprintf(file,
                        ", left_slot=%u, materialized_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
                fprintf(file,
                        ", source_slot=%u, materialized_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
                fprintf(file,
                        ", source_slot=%u, materialized_stack_slot=%u, materialized_constant_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand0[2],
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
                fprintf(file,
                        ", source_slot=%u, right_slot=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1]);
                break;

            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
                fprintf(file, ", receiver_slot=%d", inst->instruction.operand.operand2[0]);
                break;

            case ZR_INSTRUCTION_ENUM(ADD_INT):
            case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_INT):
            case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
            case ZR_INSTRUCTION_ENUM(OWN_BORROW):
            case ZR_INSTRUCTION_ENUM(OWN_LOAN):
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            case ZR_INSTRUCTION_ENUM(OWN_DETACH):
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            case ZR_INSTRUCTION_ENUM(TYPEOF):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(META_GET):
            case ZR_INSTRUCTION_ENUM(META_SET):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                fprintf(file,
                        ", operand1=%u, operand2=%u",
                        inst->instruction.operand.operand1[0],
                        inst->instruction.operand.operand1[1]);
                break;

            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
                fprintf(file,
                        ", cache=%u, receiver_source=%u, arg0_source=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand0[2]);
                break;

            case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                fprintf(file,
                        ", iterator_slot=%u, jump_if_false_offset=%d",
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;

            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            case ZR_INSTRUCTION_ENUM(TRY):
            case ZR_INSTRUCTION_ENUM(CATCH):
                break;

            default:
                if (inst->instruction.operand.operand2[0] != 0) {
                    fprintf(file, ", operand2=%d", inst->instruction.operand.operand2[0]);
                }
                if (inst->instruction.operand.operand1[0] != 0 || inst->instruction.operand.operand1[1] != 0) {
                    fprintf(file,
                            ", operand1=%u, operand1_1=%u",
                            inst->instruction.operand.operand1[0],
                            inst->instruction.operand.operand1[1]);
                }
                break;
        }
        fprintf(file, ")\n");
    }
    fprintf(file, "\n");

    if (function->childFunctionLength > 0) {
        writer_intermediate_write_indent(file, indentLevel);
        fprintf(file, "CHILD_FUNCTIONS (%u):\n", function->childFunctionLength);
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            writer_intermediate_write_indent(file, indentLevel);
            fprintf(file, "  [%u] FUNCTION:\n", index);
            writer_intermediate_write_nested_function(file, state, &function->childFunctionList[index], indentLevel + 4);
        }
        fprintf(file, "\n");
    }
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteIntermediateFile(SZrState *state, SZrFunction *function, const TZrChar *filename) {
    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }
    
    FILE *file = fopen(filename, "w");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    
    fprintf(file, "// ZR Intermediate File (.zri)\n");
    fprintf(file, "// Generated from compiled function\n\n");
    
    // 输出函数名（如果存在，否则使用 "__entry"）
    if (function->functionName != ZR_NULL) {
        TZrNativeString funcNameStr = ZrCore_String_GetNativeString(function->functionName);
        if (funcNameStr != ZR_NULL) {
            TZrSize nameLen;
            if (function->functionName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                nameLen = function->functionName->shortStringLength;
            } else {
                nameLen = function->functionName->longStringLength;
            }
            fprintf(file, "FUNCTION: %.*s\n", (int)nameLen, funcNameStr);
        } else {
            fprintf(file, "FUNCTION: <unnamed>\n");
        }
    } else {
        fprintf(file, "FUNCTION: <anonymous>\n");
    }
    fprintf(file, "  START_LINE: %u\n", function->lineInSourceStart);
    fprintf(file, "  END_LINE: %u\n", function->lineInSourceEnd);
    fprintf(file, "  PARAMETERS: %u\n", function->parameterCount);
    fprintf(file, "  HAS_VAR_ARGS: %s\n", function->hasVariableArguments ? "true" : "false");
    fprintf(file, "  STACK_SIZE: %u\n", function->stackSize);
    fprintf(file, "\n");
    
    // 常量列表
    fprintf(file, "CONSTANTS (%u):\n", function->constantValueLength);
    for (TZrUInt32 i = 0; i < function->constantValueLength; i++) {
        SZrTypeValue *constant = &function->constantValueList[i];
        fprintf(file, "  [%u] ", i);
        writer_intermediate_write_constant(file, state, constant);
    }
    fprintf(file, "\n");
    
    // 局部变量列表
    fprintf(file, "LOCAL_VARIABLES (%u):\n", function->localVariableLength);
    for (TZrUInt32 i = 0; i < function->localVariableLength; i++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[i];
        TZrNativeString nameStr = local->name ? ZrCore_String_GetNativeString(local->name) : "<unnamed>";
        fprintf(file, "  [%u] %s: offset_activate=%u, offset_dead=%u\n", 
                i, nameStr, (TZrUInt32)local->offsetActivate, (TZrUInt32)local->offsetDead);
    }
    fprintf(file, "\n");
    
    // 闭包变量列表
    fprintf(file, "CLOSURE_VARIABLES (%u):\n", function->closureValueLength);
    for (TZrUInt32 i = 0; i < function->closureValueLength; i++) {
        SZrFunctionClosureVariable *closure = &function->closureValueList[i];
        TZrNativeString nameStr = closure->name ? ZrCore_String_GetNativeString(closure->name) : "<unnamed>";
        
        // 输出类型名称
        const TZrChar *typeName = "UNKNOWN";
        switch (closure->valueType) {
            case ZR_VALUE_TYPE_NULL: typeName = "NULL"; break;
            case ZR_VALUE_TYPE_BOOL: typeName = "BOOL"; break;
            case ZR_VALUE_TYPE_INT8: typeName = "INT8"; break;
            case ZR_VALUE_TYPE_INT16: typeName = "INT16"; break;
            case ZR_VALUE_TYPE_INT32: typeName = "INT32"; break;
            case ZR_VALUE_TYPE_INT64: typeName = "INT64"; break;
            case ZR_VALUE_TYPE_UINT8: typeName = "UINT8"; break;
            case ZR_VALUE_TYPE_UINT16: typeName = "UINT16"; break;
            case ZR_VALUE_TYPE_UINT32: typeName = "UINT32"; break;
            case ZR_VALUE_TYPE_UINT64: typeName = "UINT64"; break;
            case ZR_VALUE_TYPE_FLOAT: typeName = "FLOAT"; break;
            case ZR_VALUE_TYPE_DOUBLE: typeName = "DOUBLE"; break;
            case ZR_VALUE_TYPE_STRING: typeName = "STRING"; break;
            case ZR_VALUE_TYPE_FUNCTION: typeName = "FUNCTION"; break;
            case ZR_VALUE_TYPE_CLOSURE: typeName = "CLOSURE"; break;
            case ZR_VALUE_TYPE_OBJECT: typeName = "OBJECT"; break;
            case ZR_VALUE_TYPE_ARRAY: typeName = "ARRAY"; break;
            default: typeName = "UNKNOWN"; break;
        }
        
        fprintf(file, "  [%u] %s: in_stack=%s, index=%u, type=%s\n", 
                i, nameStr, closure->inStack ? "true" : "false", 
                closure->index, typeName);
    }
    fprintf(file, "\n");

    writer_intermediate_write_type_metadata(file, state, function, 0);
    writer_intermediate_write_semir_metadata(file, state, function, 0);
    writer_intermediate_write_callsite_cache_table(file, function, 0);
    writer_intermediate_write_eh_table(file, function, 0);
    
    // Prototype 数据列表
    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
        fprintf(file, "PROTOTYPES (%u):\n", function->prototypeCount);
        ZrCore_Debug_PrintPrototypesFromData(state, function, file);
        fprintf(file, "\n");
    }
    
    // 指令列表
    fprintf(file, "INSTRUCTIONS (%u):\n", function->instructionsLength);
    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        TZrInstruction *inst = &function->instructionsList[i];
        EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
        TZrUInt16 operandExtra = inst->instruction.operandExtra;
        
        fprintf(file, "  [%u] ", i);
        
        // 输出操作码名称
        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_STACK):
                fprintf(file, "GET_STACK");
                break;
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                fprintf(file, "SET_STACK");
                break;
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                fprintf(file, "GET_CONSTANT");
                break;
            case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
                fprintf(file, "RESET_STACK_NULL");
                break;
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
                fprintf(file, "SET_CONSTANT");
                break;
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                fprintf(file, "GET_CLOSURE");
                break;
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                fprintf(file, "SET_CLOSURE");
                break;
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                fprintf(file, "GETUPVAL");
                break;
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
                fprintf(file, "SETUPVAL");
                break;
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
                fprintf(file, "GET_GLOBAL");
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
                fprintf(file, "GET_MEMBER");
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
                fprintf(file, "SET_MEMBER");
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
                fprintf(file, "GET_MEMBER_SLOT");
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
                fprintf(file, "SET_MEMBER_SLOT");
                break;
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
                fprintf(file, "GET_BY_INDEX");
                break;
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
                fprintf(file, "SET_BY_INDEX");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
                fprintf(file, "SUPER_ARRAY_GET_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
                fprintf(file, "SUPER_ARRAY_GET_INT_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
                fprintf(file, "SUPER_ARRAY_BIND_ITEMS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
                fprintf(file, "SUPER_ARRAY_GET_INT_ITEMS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
                fprintf(file, "SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
                fprintf(file, "SUPER_ARRAY_SET_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
                fprintf(file, "SUPER_ARRAY_SET_INT_ITEMS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
                fprintf(file, "SUPER_ARRAY_ADD_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
                fprintf(file, "SUPER_ARRAY_ADD_INT4");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
                fprintf(file, "SUPER_ARRAY_ADD_INT4_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
                fprintf(file, "SUPER_ARRAY_FILL_INT4_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(NOP):
                fprintf(file, "NOP");
                break;
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
                fprintf(file, "ITER_INIT");
                break;
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
                fprintf(file, "ITER_MOVE_NEXT");
                break;
            case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
                fprintf(file, "DYN_ITER_INIT");
                break;
            case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
                fprintf(file, "DYN_ITER_MOVE_NEXT");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
                fprintf(file, "SUPER_FUNCTION_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
                fprintf(file, "SUPER_DYN_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
                fprintf(file, "SUPER_META_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
                fprintf(file, "SUPER_DYN_CALL_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
                fprintf(file, "SUPER_META_CALL_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
                fprintf(file, "SUPER_META_GET_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
                fprintf(file, "SUPER_META_SET_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
                fprintf(file, "SUPER_META_GET_STATIC_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
                fprintf(file, "SUPER_META_SET_STATIC_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_FUNCTION_TAIL_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_DYN_TAIL_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_META_TAIL_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
                fprintf(file, "SUPER_DYN_TAIL_CALL_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
                fprintf(file, "SUPER_META_TAIL_CALL_CACHED");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                fprintf(file, "SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                fprintf(file, "SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE");
                break;
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
                fprintf(file, "ITER_CURRENT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
                fprintf(file, "TO_BOOL");
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT):
                fprintf(file, "TO_INT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT):
                fprintf(file, "TO_UINT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
                fprintf(file, "TO_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRING):
                fprintf(file, "TO_STRING");
                break;
            case ZR_INSTRUCTION_ENUM(ADD):
                fprintf(file, "ADD");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
                fprintf(file, "ADD_INT");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
                fprintf(file, "ADD_INT_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
                fprintf(file, "ADD_INT_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
                fprintf(file, "ADD_INT_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
                fprintf(file, "ADD_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
                fprintf(file, "ADD_SIGNED_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
                fprintf(file, "ADD_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
                fprintf(file, "ADD_SIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
                fprintf(file, "ADD_SIGNED_LOAD_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
                fprintf(file, "ADD_SIGNED_LOAD_STACK_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
                fprintf(file, "ADD_SIGNED_LOAD_STACK");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
                fprintf(file, "ADD_SIGNED_LOAD_STACK_LOAD_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
                fprintf(file, "SUB_SIGNED_LOAD_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
                fprintf(file, "SUB_SIGNED_LOAD_STACK_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
                fprintf(file, "ADD_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
                fprintf(file, "ADD_UNSIGNED_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
                fprintf(file, "ADD_UNSIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
                fprintf(file, "ADD_UNSIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
                fprintf(file, "ADD_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
                fprintf(file, "ADD_STRING");
                break;
            case ZR_INSTRUCTION_ENUM(SUB):
                fprintf(file, "SUB");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT):
                fprintf(file, "SUB_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
                fprintf(file, "SUB_INT_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
                fprintf(file, "SUB_INT_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
                fprintf(file, "SUB_INT_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
                fprintf(file, "SUB_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
                fprintf(file, "SUB_SIGNED_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
                fprintf(file, "SUB_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
                fprintf(file, "SUB_SIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
                fprintf(file, "SUB_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
                fprintf(file, "SUB_UNSIGNED_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
                fprintf(file, "SUB_UNSIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
                fprintf(file, "SUB_UNSIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
                fprintf(file, "SUB_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(MUL):
                fprintf(file, "MUL");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                fprintf(file, "MUL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
                fprintf(file, "MUL_SIGNED_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
                fprintf(file, "MUL_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
                fprintf(file, "MUL_SIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
                fprintf(file, "MUL_SIGNED_LOAD_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
                fprintf(file, "MUL_SIGNED_LOAD_STACK_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
                fprintf(file, "MUL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
                fprintf(file, "MUL_UNSIGNED_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
                fprintf(file, "MUL_UNSIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
                fprintf(file, "MUL_UNSIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
                fprintf(file, "MUL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(NEG):
                fprintf(file, "NEG");
                break;
            case ZR_INSTRUCTION_ENUM(DIV):
                fprintf(file, "DIV");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                fprintf(file, "DIV_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
                fprintf(file, "DIV_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
                fprintf(file, "DIV_SIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
                fprintf(file, "DIV_SIGNED_LOAD_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
                fprintf(file, "DIV_SIGNED_LOAD_STACK_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
                fprintf(file, "DIV_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
                fprintf(file, "DIV_UNSIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
                fprintf(file, "DIV_UNSIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
                fprintf(file, "DIV_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(MOD):
                fprintf(file, "MOD");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                fprintf(file, "MOD_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
                fprintf(file, "MOD_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
                fprintf(file, "MOD_SIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
                fprintf(file, "MOD_SIGNED_LOAD_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
                fprintf(file, "MOD_SIGNED_LOAD_STACK_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
                fprintf(file, "MOD_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
                fprintf(file, "MOD_UNSIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
                fprintf(file, "MOD_UNSIGNED_CONST_PLAIN_DEST");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
                fprintf(file, "MOD_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(POW):
                fprintf(file, "POW");
                break;
            case ZR_INSTRUCTION_ENUM(POW_SIGNED):
                fprintf(file, "POW_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
                fprintf(file, "POW_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(POW_FLOAT):
                fprintf(file, "POW_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
                fprintf(file, "SHIFT_LEFT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
                fprintf(file, "SHIFT_LEFT_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
                fprintf(file, "SHIFT_RIGHT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
                fprintf(file, "SHIFT_RIGHT_INT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
                fprintf(file, "LOGICAL_NOT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
                fprintf(file, "LOGICAL_AND");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
                fprintf(file, "LOGICAL_OR");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
                fprintf(file, "LOGICAL_GREATER_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
                fprintf(file, "LOGICAL_GREATER_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
                fprintf(file, "LOGICAL_GREATER_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                fprintf(file, "LOGICAL_LESS_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
                fprintf(file, "LOGICAL_LESS_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
                fprintf(file, "LOGICAL_LESS_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                fprintf(file, "LOGICAL_EQUAL");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                fprintf(file, "LOGICAL_NOT_EQUAL");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
                fprintf(file, "LOGICAL_EQUAL_BOOL");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
                fprintf(file, "LOGICAL_NOT_EQUAL_BOOL");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
                fprintf(file, "LOGICAL_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
                fprintf(file, "LOGICAL_EQUAL_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
                fprintf(file, "LOGICAL_NOT_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
                fprintf(file, "LOGICAL_EQUAL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
                fprintf(file, "LOGICAL_NOT_EQUAL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
                fprintf(file, "LOGICAL_EQUAL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
                fprintf(file, "LOGICAL_NOT_EQUAL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
                fprintf(file, "LOGICAL_EQUAL_STRING");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
                fprintf(file, "LOGICAL_NOT_EQUAL_STRING");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
                fprintf(file, "LOGICAL_GREATER_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
                fprintf(file, "LOGICAL_GREATER_EQUAL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
                fprintf(file, "LOGICAL_GREATER_EQUAL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
                fprintf(file, "LOGICAL_LESS_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
                fprintf(file, "LOGICAL_LESS_EQUAL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
                fprintf(file, "LOGICAL_LESS_EQUAL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
                fprintf(file, "BITWISE_NOT");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
                fprintf(file, "BITWISE_AND");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
                fprintf(file, "BITWISE_OR");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
                fprintf(file, "BITWISE_XOR");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
                fprintf(file, "BITWISE_SHIFT_LEFT");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
                fprintf(file, "BITWISE_SHIFT_RIGHT");
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                fprintf(file, "FUNCTION_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
                fprintf(file, "KNOWN_VM_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
                fprintf(file, "KNOWN_VM_MEMBER_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
                fprintf(file, "KNOWN_VM_MEMBER_CALL_LOAD1_U8");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
                fprintf(file, "KNOWN_NATIVE_MEMBER_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
                fprintf(file, "KNOWN_VM_TAIL_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
                fprintf(file, "KNOWN_NATIVE_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
                fprintf(file, "KNOWN_NATIVE_TAIL_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
                fprintf(file, "FUNCTION_TAIL_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                fprintf(file, "FUNCTION_RETURN");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP):
                fprintf(file, "JUMP");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                fprintf(file, "JUMP_IF");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                fprintf(file, "JUMP_IF_GREATER_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
                fprintf(file, "JUMP_IF_NOT_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
                fprintf(file, "JUMP_IF_NOT_EQUAL_SIGNED_CONST");
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                fprintf(file, "CREATE_CLOSURE");
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                fprintf(file, "CREATE_OBJECT");
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                fprintf(file, "CREATE_ARRAY");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
                fprintf(file, "OWN_UNIQUE");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_BORROW):
                fprintf(file, "OWN_BORROW");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_LOAN):
                fprintf(file, "OWN_LOAN");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
                fprintf(file, "OWN_SHARE");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
                fprintf(file, "OWN_WEAK");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_DETACH):
                fprintf(file, "OWN_DETACH");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
                fprintf(file, "OWN_UPGRADE");
                break;
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
                fprintf(file, "OWN_RELEASE");
                break;
            case ZR_INSTRUCTION_ENUM(TYPEOF):
                fprintf(file, "TYPEOF");
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                fprintf(file, "DYN_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                fprintf(file, "DYN_TAIL_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
                fprintf(file, "META_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                fprintf(file, "META_TAIL_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(META_GET):
                fprintf(file, "META_GET");
                break;
            case ZR_INSTRUCTION_ENUM(META_SET):
                fprintf(file, "META_SET");
                break;
            case ZR_INSTRUCTION_ENUM(TRY):
                fprintf(file, "TRY");
                break;
            case ZR_INSTRUCTION_ENUM(THROW):
                fprintf(file, "THROW");
                break;
            case ZR_INSTRUCTION_ENUM(CATCH):
                fprintf(file, "CATCH");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_VM_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_NATIVE_CALL_NO_ARGS");
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
                fprintf(file, "SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS");
                break;
            default:
                fprintf(file, "OPCODE_%u", (TZrUInt32)opcode);
                break;
        }
        
        // 输出操作数（根据指令类型决定格式）
        // GET_CONSTANT, GET_STACK, SET_STACK 等使用 operandExtra + operand2[0]
        // ADD_INT, SUB_INT 等使用 operandExtra + operand1[0] + operand1[1]
        // FUNCTION_RETURN 使用 operandExtra + operand1[0] + operand1[1]
        fprintf(file, " (extra=%u", operandExtra);
        
        // 检查指令类型，决定使用哪种操作数格式
        switch (opcode) {
            // 使用 operand2[0] (TZrInt32) 的指令
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            case ZR_INSTRUCTION_ENUM(JUMP):
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
            case ZR_INSTRUCTION_ENUM(THROW):
                fprintf(file, ", operand=%d", inst->instruction.operand.operand2[0]);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                fprintf(file,
                        ", left_slot=%u, right_slot=%u, jump_offset=%d",
                        inst->instruction.operandExtra,
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
                fprintf(file,
                        ", left_slot=%u, right_slot=%u, jump_offset=%d",
                        inst->instruction.operandExtra,
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
                fprintf(file,
                        ", left_slot=%u, constant_index=%u, jump_offset=%d",
                        inst->instruction.operandExtra,
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;
                
            // 使用 operand1[0] (TZrUInt16) 的指令（单操作数）
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
            case ZR_INSTRUCTION_ENUM(TO_INT):
            case ZR_INSTRUCTION_ENUM(TO_UINT):
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            case ZR_INSTRUCTION_ENUM(TO_STRING):
            case ZR_INSTRUCTION_ENUM(NEG):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
            case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
            case ZR_INSTRUCTION_ENUM(OWN_BORROW):
            case ZR_INSTRUCTION_ENUM(OWN_LOAN):
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            case ZR_INSTRUCTION_ENUM(OWN_DETACH):
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
                fprintf(file, ", operand1=%u", inst->instruction.operand.operand1[0]);
                break;

            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
                fprintf(file,
                        ", left_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand1[0],
                        inst->instruction.operand.operand1[1]);
                break;

            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
                fprintf(file,
                        ", left_slot=%u, materialized_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
                fprintf(file,
                        ", source_slot=%u, materialized_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
                fprintf(file,
                        ", source_slot=%u, materialized_stack_slot=%u, materialized_constant_slot=%u, constant_index=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand0[2],
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
                fprintf(file,
                        ", source_slot=%u, right_slot=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1]);
                break;

            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
                fprintf(file, ", receiver_slot=%d", inst->instruction.operand.operand2[0]);
                break;
                
            // 使用 operand1[0] + operand1[1] (TZrUInt16) 的指令（双操作数）
            case ZR_INSTRUCTION_ENUM(ADD):
            case ZR_INSTRUCTION_ENUM(ADD_INT):
            case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
            case ZR_INSTRUCTION_ENUM(SUB):
            case ZR_INSTRUCTION_ENUM(SUB_INT):
            case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            case ZR_INSTRUCTION_ENUM(MUL):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            case ZR_INSTRUCTION_ENUM(DIV):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            case ZR_INSTRUCTION_ENUM(MOD):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            case ZR_INSTRUCTION_ENUM(POW):
            case ZR_INSTRUCTION_ENUM(POW_SIGNED):
            case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(POW_FLOAT):
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            case ZR_INSTRUCTION_ENUM(TYPEOF):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(META_GET):
            case ZR_INSTRUCTION_ENUM(META_SET):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
                fprintf(file, ", operand1=%u, operand2=%u", 
                        inst->instruction.operand.operand1[0], 
                        inst->instruction.operand.operand1[1]);
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
                fprintf(file,
                        ", cache=%u, receiver_source=%u, arg0_source=%u",
                        inst->instruction.operand.operand0[0],
                        inst->instruction.operand.operand0[1],
                        inst->instruction.operand.operand0[2]);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                fprintf(file,
                        ", iterator_slot=%u, jump_if_false_offset=%d",
                        inst->instruction.operand.operand1[0],
                        (TZrInt16)inst->instruction.operand.operand1[1]);
                break;
                
            // 只使用 operandExtra，不需要其他操作数
            case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            case ZR_INSTRUCTION_ENUM(TRY):
            case ZR_INSTRUCTION_ENUM(CATCH):
                break;
                
            default:
                // 对于未知指令，尝试输出所有可能的操作数格式
                if (inst->instruction.operand.operand2[0] != 0) {
                    fprintf(file, ", operand2=%d", inst->instruction.operand.operand2[0]);
                }
                if (inst->instruction.operand.operand1[0] != 0 || inst->instruction.operand.operand1[1] != 0) {
                    fprintf(file, ", operand1=%u, operand1_1=%u", 
                            inst->instruction.operand.operand1[0], 
                            inst->instruction.operand.operand1[1]);
                }
                break;
        }
        fprintf(file, ")\n");
    }
    fprintf(file, "\n");
    
    // 子函数列表（递归输出）
    if (function->childFunctionLength > 0) {
        fprintf(file, "CHILD_FUNCTIONS (%u):\n", function->childFunctionLength);
        for (TZrUInt32 i = 0; i < function->childFunctionLength; i++) {
            fprintf(file, "  [%u] FUNCTION:\n", i);
            writer_intermediate_write_nested_function(file, state, &function->childFunctionList[i], 4);
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return ZR_TRUE;
}

// 获取 AST 节点类型名称
