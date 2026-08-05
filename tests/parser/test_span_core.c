#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "test_span_gc_cases.h"
#include "test_span_semantic_ir_cases.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/type_inference.h"

static const ZrLibTypeDescriptor *find_type_descriptor(
        const ZrLibModuleDescriptor *module,
        const char *name) {
    if (module == NULL || name == NULL) {
        return NULL;
    }

    for (TZrSize index = 0; index < module->typeCount; index++) {
        if (module->types[index].name != NULL &&
            strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return NULL;
}

static const ZrLibFieldDescriptor *find_field_descriptor(
        const ZrLibTypeDescriptor *type,
        TZrUInt32 contractRole) {
    if (type == NULL) {
        return NULL;
    }

    for (TZrSize index = 0; index < type->fieldCount; index++) {
        if (type->fields[index].contractRole == contractRole) {
            return &type->fields[index];
        }
    }
    return NULL;
}

static const ZrLibMethodDescriptor *find_method_descriptor(
        const ZrLibTypeDescriptor *type,
        TZrUInt32 contractRole) {
    if (type == NULL) {
        return NULL;
    }

    for (TZrSize index = 0; index < type->methodCount; index++) {
        if (type->methods[index].contractRole == contractRole) {
            return &type->methods[index];
        }
    }
    return NULL;
}

static const ZrLibMetaMethodDescriptor *find_meta_descriptor(
        const ZrLibTypeDescriptor *type,
        EZrMetaType metaType) {
    if (type == NULL) {
        return NULL;
    }

    for (TZrSize index = 0; index < type->metaMethodCount; index++) {
        if (type->metaMethods[index].metaType == metaType) {
            return &type->metaMethods[index];
        }
    }
    return NULL;
}

static const SZrTypeMemberInfo *find_member_by_role(
        const SZrTypePrototypeInfo *type,
        TZrUInt32 contractRole) {
    if (type == NULL) {
        return NULL;
    }

    for (TZrSize index = 0; index < type->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&type->members, index);
        if (member != NULL && member->contractRole == contractRole) {
            return member;
        }
    }
    return NULL;
}

static SZrAstNode *parse_source(
        SZrState *state,
        const char *path,
        const char *source) {
    SZrString *sourceName;

    if (state == NULL || path == NULL || source == NULL) {
        return NULL;
    }

    sourceName = ZrCore_String_Create(
            state, (TZrNativeString)path, strlen(path));
    if (sourceName == NULL) {
        return NULL;
    }
    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_source(
        SZrState *state,
        const char *path,
        const char *source) {
    SZrString *sourceName;

    if (state == NULL || path == NULL || source == NULL) {
        return NULL;
    }

    sourceName = ZrCore_String_Create(
            state, (TZrNativeString)path, strlen(path));
    if (sourceName == NULL) {
        return NULL;
    }
    return ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
}

static TZrSize count_opcode_on_source_line(
        SZrFunction *function,
        EZrInstructionCode opcode,
        TZrUInt32 sourceLine) {
    TZrSize count = 0u;

    if (function == NULL || function->instructionsList == NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        if ((EZrInstructionCode)instruction->instruction.operationCode == opcode &&
            ZrCore_Exception_FindSourceLine(function, index) == sourceLine) {
            count++;
        }
    }
    return count;
}

static TZrSize count_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    TZrSize count = 0u;

    if (function == NULL || function->instructionsList == NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index]
                    .instruction.operationCode == opcode) {
            count++;
        }
    }
    return count;
}

static TZrSize count_inline_frame_slots(const SZrFunction *function) {
    TZrSize count = 0u;

    if (function == NULL || function->frameSlotLayouts == NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        if (function->frameSlotLayouts[index].slotKind ==
            (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            count++;
        }
    }
    return count;
}

static const SZrCompiledPrototypeInfo *compiled_prototype_at(
        const SZrFunction *function,
        TZrUInt32 targetIndex) {
    const TZrByte *cursor;
    TZrSize remaining;

    if (function == NULL || function->prototypeData == NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32) ||
        targetIndex >= function->prototypeCount) {
        return NULL;
    }

    cursor = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 index = 0u; index < function->prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *prototype;
        TZrSize recordSize;

        if (remaining < sizeof(SZrCompiledPrototypeInfo)) {
            return NULL;
        }
        prototype = (const SZrCompiledPrototypeInfo *)cursor;
        recordSize = sizeof(*prototype) +
                     (TZrSize)prototype->inheritsCount * sizeof(TZrUInt32) +
                     (TZrSize)prototype->decoratorsCount * sizeof(TZrUInt32) +
                     (TZrSize)prototype->membersCount * sizeof(SZrCompiledMemberInfo);
        if (recordSize > remaining) {
            return NULL;
        }
        if (index == targetIndex) {
            return prototype;
        }
        cursor += recordSize;
        remaining -= recordSize;
    }
    return NULL;
}

static void test_span_descriptors_publish_ref_like_contiguous_view_contracts(void) {
    const ZrLibModuleDescriptor *module = ZrVmLibContainer_GetModuleDescriptor();
    const ZrLibTypeDescriptor *span;
    const ZrLibTypeDescriptor *readOnlySpan;
    const ZrLibTypeDescriptor *array;
    const ZrLibFieldDescriptor *spanSource;
    const ZrLibFieldDescriptor *spanStart;
    const ZrLibFieldDescriptor *spanLength;
    const ZrLibMethodDescriptor *spanSlice;
    const ZrLibMethodDescriptor *readOnlyConversion;

    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_STRING("zr.container", module->moduleName);
    TEST_ASSERT_EQUAL_UINT64(8, module->typeCount);

    span = find_type_descriptor(module, "Span");
    readOnlySpan = find_type_descriptor(module, "ReadOnlySpan");
    array = find_type_descriptor(module, "Array");
    TEST_ASSERT_NOT_NULL(span);
    TEST_ASSERT_NOT_NULL(readOnlySpan);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_NOT_NULL(find_method_descriptor(
            array, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE));

    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, span->prototypeType);
    TEST_ASSERT_TRUE(span->allowValueConstruction);
    TEST_ASSERT_FALSE(span->allowBoxedConstruction);
    TEST_ASSERT_EQUAL_UINT64(1, span->genericParameterCount);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                    ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_MUTABLE),
            span->protocolMask);

    TEST_ASSERT_EQUAL_INT(
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, readOnlySpan->prototypeType);
    TEST_ASSERT_TRUE(readOnlySpan->allowValueConstruction);
    TEST_ASSERT_FALSE(readOnlySpan->allowBoxedConstruction);
    TEST_ASSERT_EQUAL_UINT64(1, readOnlySpan->genericParameterCount);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                    ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_READONLY),
            readOnlySpan->protocolMask);

    spanSource = find_field_descriptor(
            span, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SOURCE);
    spanStart = find_field_descriptor(
            span, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_START);
    spanLength = find_field_descriptor(span, ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH);
    spanSlice = find_method_descriptor(
            span, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE);
    readOnlyConversion = find_method_descriptor(
            span, ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION);

    TEST_ASSERT_NOT_NULL(spanSource);
    TEST_ASSERT_NOT_NULL(spanStart);
    TEST_ASSERT_NOT_NULL(spanLength);
    TEST_ASSERT_NOT_NULL(spanSlice);
    TEST_ASSERT_NOT_NULL(readOnlyConversion);
    TEST_ASSERT_EQUAL_STRING("object", spanSource->typeName);
    TEST_ASSERT_EQUAL_STRING("int", spanStart->typeName);
    TEST_ASSERT_EQUAL_STRING("int", spanLength->typeName);
    TEST_ASSERT_EQUAL_STRING("Span<T>", spanSlice->returnTypeName);
    TEST_ASSERT_EQUAL_STRING("ReadOnlySpan<T>", readOnlyConversion->returnTypeName);
    TEST_ASSERT_EQUAL_STRING(
            "Span<T>",
            find_method_descriptor(
                    array,
                    ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE)
                    ->returnTypeName);

    TEST_ASSERT_NOT_NULL(find_meta_descriptor(span, ZR_META_GET_ITEM));
    TEST_ASSERT_NOT_NULL(find_meta_descriptor(span, ZR_META_SET_ITEM));
    TEST_ASSERT_NOT_NULL(find_meta_descriptor(readOnlySpan, ZR_META_GET_ITEM));
    TEST_ASSERT_NULL(find_meta_descriptor(readOnlySpan, ZR_META_SET_ITEM));
}

static void test_span_runtime_prototypes_preserve_contiguous_view_protocols(void) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrObjectModule *module;
    const SZrTypeValue *spanValue;
    const SZrTypeValue *readOnlySpanValue;
    SZrObjectPrototype *span;
    SZrObjectPrototype *readOnlySpan;

    TEST_ASSERT_NOT_NULL(state);
    module = ZrContainerTests_ImportNativeModule(state, "zr.container");
    TEST_ASSERT_NOT_NULL(module);

    spanValue = ZrContainerTests_GetModuleExportValue(state, module, "Span");
    readOnlySpanValue = ZrContainerTests_GetModuleExportValue(
            state, module, "ReadOnlySpan");
    TEST_ASSERT_NOT_NULL(spanValue);
    TEST_ASSERT_NOT_NULL(readOnlySpanValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, spanValue->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, readOnlySpanValue->type);

    span = (SZrObjectPrototype *)ZR_CAST_OBJECT(
            state, spanValue->value.object);
    readOnlySpan = (SZrObjectPrototype *)ZR_CAST_OBJECT(
            state, readOnlySpanValue->value.object);
    TEST_ASSERT_NOT_NULL(span);
    TEST_ASSERT_NOT_NULL(readOnlySpan);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                    ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_MUTABLE),
            span->protocolMask);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                    ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_READONLY),
            readOnlySpan->protocolMask);

    ZrContainerTests_DestroyState(state);
}

static void test_span_compiler_prototypes_project_ref_like_member_contracts(void) {
    static const char kSource[] =
            "var {Span, ReadOnlySpan} = import(\"zr.container\");\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *script;
    const SZrTypePrototypeInfo *span;
    const SZrTypePrototypeInfo *readOnlySpan;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    script = parse_source(state, "span_contract_projection.zr", kSource);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);

    compiler->scriptAst = script;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(
            compiler, script->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);

    span = ZrContainerTests_FindTypePrototype(compiler, "Span");
    readOnlySpan = ZrContainerTests_FindTypePrototype(
            compiler, "ReadOnlySpan");
    TEST_ASSERT_NOT_NULL(span);
    TEST_ASSERT_NOT_NULL(readOnlySpan);
    TEST_ASSERT_BITS_HIGH(
            ZR_DECLARATION_MODIFIER_REF_LIKE, span->modifierFlags);
    TEST_ASSERT_BITS_HIGH(
            ZR_DECLARATION_MODIFIER_REF_LIKE, readOnlySpan->modifierFlags);
    TEST_ASSERT_FALSE(span->allowBoxedConstruction);
    TEST_ASSERT_FALSE(readOnlySpan->allowBoxedConstruction);
    TEST_ASSERT_NOT_NULL(find_member_by_role(
            span, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SOURCE));
    TEST_ASSERT_NOT_NULL(find_member_by_role(
            span, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_START));
    TEST_ASSERT_NOT_NULL(find_member_by_role(
            span, ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH));
    TEST_ASSERT_NOT_NULL(find_member_by_role(
            span, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE));
    TEST_ASSERT_NOT_NULL(find_member_by_role(
            span, ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION));

    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = NULL;
    ZrParser_Ast_Free(state, script);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void test_span_array_runtime_mutation_slice_and_readonly_view_share_storage(void) {
    static const char kSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(10);\n"
            "xs.add(20);\n"
            "xs.add(30);\n"
            "var view = xs.span();\n"
            "view[1] = 40;\n"
            "var tail = view.slice(1, 2);\n"
            "var readOnly = view.asReadOnly();\n"
            "return view.length * 10000 + view[1] * 100 + tail[0] * 10 + "
            "readOnly[2] + xs[1];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_array_runtime.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(34470, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_default_and_empty_slice_are_legal(void) {
    static const char kSource[] =
            "var container = import(\"zr.container\");\n"
            "var {Span, ReadOnlySpan} = import(\"zr.container\");\n"
            "var empty: Span<int> = init Span<int>();\n"
            "var readOnlyEmpty: ReadOnlySpan<int> = init ReadOnlySpan<int>();\n"
            "return empty.length * 100 + empty.slice(0, 0).length * 10 + "
            "readOnlyEmpty.slice(0, 0).length;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = -1;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_default_empty.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_index_and_slice_reject_out_of_range_access(void) {
    static const char kIndexSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(7);\n"
            "return xs.span()[1];\n";
    static const char kSliceSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(7);\n"
            "return xs.span().slice(1, 1).length;\n";
    SZrState *state;
    SZrFunction *function;
    SZrTypeValue result;

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_index_out_of_range.zr", kIndexSource);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(
            state, function, &result));
    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_slice_out_of_range.zr", kSliceSource);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(
            state, function, &result));
    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_readonly_span_rejects_index_assignment(void) {
    static const char kSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(1);\n"
            "var view = xs.span().asReadOnly();\n"
            "view[0] = 2;\n"
            "return view[0];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "readonly_span_assignment.zr", kSource);
    TEST_ASSERT_NULL(function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_implicitly_weakens_to_readonly_span_with_same_element_type(void) {
    static const char kSource[] =
            "var container = import(\"zr.container\");\n"
            "var {ReadOnlySpan} = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(17);\n"
            "var view: ReadOnlySpan<int> = xs.span();\n"
            "return view[0];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_readonly_weakening.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(17, result);
    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_rejects_capability_strengthening_and_element_type_change(void) {
    static const char kStrengtheningSource[] =
            "var container = import(\"zr.container\");\n"
            "var {Span} = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "var readOnly = xs.span().asReadOnly();\n"
            "var mutable: Span<int> = readOnly;\n";
    static const char kElementMismatchSource[] =
            "var container = import(\"zr.container\");\n"
            "var {ReadOnlySpan} = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "var wrong: ReadOnlySpan<string> = xs.span();\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, "span_strengthening_rejected.zr", kStrengtheningSource);
    TEST_ASSERT_NULL(function);
    ZrContainerTests_DestroyState(state);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, "span_element_mismatch_rejected.zr", kElementMismatchSource);
    TEST_ASSERT_NULL(function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_exact_overload_wins_over_readonly_weakening(void) {
    static const char kImportSource[] =
            "var {Span, ReadOnlySpan} = import(\"zr.container\");\n";
    static const char kCallSource[] = "var result = pick(view);";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *importScript;
    SZrAstNode *callScript;
    SZrAstNode *callExpression;
    SZrInferredType intType;
    SZrInferredType boolType;
    SZrInferredType spanType;
    SZrInferredType readOnlySpanType;
    SZrInferredType resultType;
    SZrInferredType spanElementType;
    SZrInferredType readOnlyElementType;
    SZrArray spanParameters;
    SZrArray readOnlyParameters;
    SZrString *pickName;
    SZrString *viewName;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    importScript = parse_source(
            state, "span_overload_import.zr", kImportSource);
    TEST_ASSERT_NOT_NULL(importScript);
    compiler->scriptAst = importScript;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(
            compiler, importScript->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);

    ZrParser_InferredType_Init(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &boolType, ZR_VALUE_TYPE_BOOL);
    ZrParser_InferredType_InitFull(
            state,
            &spanType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_CreateFromNative(state, "Span<int>"));
    ZrParser_InferredType_InitFull(
            state,
            &readOnlySpanType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_CreateFromNative(state, "ReadOnlySpan<int>"));
    ZrCore_Array_Init(
            state, &spanType.elementTypes, sizeof(SZrInferredType), 1u);
    ZrCore_Array_Init(
            state, &readOnlySpanType.elementTypes, sizeof(SZrInferredType), 1u);
    ZrParser_InferredType_Copy(state, &spanElementType, &intType);
    ZrParser_InferredType_Copy(state, &readOnlyElementType, &intType);
    ZrCore_Array_Push(state, &spanType.elementTypes, &spanElementType);
    ZrCore_Array_Push(
            state, &readOnlySpanType.elementTypes, &readOnlyElementType);
    ZrCore_Array_Init(state, &spanParameters, sizeof(SZrInferredType), 1u);
    ZrCore_Array_Init(state, &readOnlyParameters, sizeof(SZrInferredType), 1u);
    ZrCore_Array_Push(state, &spanParameters, &spanType);
    ZrCore_Array_Push(state, &readOnlyParameters, &readOnlySpanType);
    pickName = ZrCore_String_CreateFromNative(state, "pick");
    viewName = ZrCore_String_CreateFromNative(state, "view");

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            state, compiler->typeEnv, viewName, &spanType));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
            state,
            compiler->typeEnv,
            pickName,
            &intType,
            &readOnlyParameters));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
            state,
            compiler->typeEnv,
            pickName,
            &boolType,
            &spanParameters));

    callScript = parse_source(state, "span_exact_overload.zr", kCallSource);
    TEST_ASSERT_NOT_NULL(callScript);
    callExpression =
            callScript->data.script.statements->nodes[0]
                    ->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(callExpression);
    ZrParser_InferredType_Init(state, &resultType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            compiler, callExpression, &resultType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, resultType.baseType);

    ZrParser_InferredType_Free(state, &resultType);
    ZrParser_Ast_Free(state, callScript);
    ZrCore_Array_Free(state, &readOnlyParameters);
    ZrCore_Array_Free(state, &spanParameters);
    ZrParser_InferredType_Free(state, &readOnlySpanType);
    ZrParser_InferredType_Free(state, &spanType);
    ZrParser_InferredType_Free(state, &boolType);
    ZrParser_InferredType_Free(state, &intType);
    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = NULL;
    ZrParser_Ast_Free(state, importScript);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void test_span_slice_lowers_inline_without_native_callback_or_wrapper(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var empty: Span<int> = init Span<int>();\n"
            "var sliced = empty.slice(0, 0);\n"
            "return sliced.length;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = -1;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_slice_inline.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(count_inline_frame_slots(function) >= 2u);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            count_opcode_on_source_line(
                    function, ZR_INSTRUCTION_ENUM(CREATE_OBJECT), 3u));
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            count_opcode_on_source_line(
                    function,
                    ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL),
                    3u));
    TEST_ASSERT_TRUE(
            count_opcode_on_source_line(
                    function, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3u) >= 3u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_imported_span_inline_layout_reuses_provider_prototype(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var empty: Span<int> = init Span<int>();\n"
            "return empty.length;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrObjectModule *module;
    const SZrTypeValue *spanValue;
    SZrObjectPrototype *providerSpan;
    SZrObjectPrototype *resolvedSpan = NULL;
    SZrFunction *function;
    TZrInt64 result = -1;

    TEST_ASSERT_NOT_NULL(state);
    module = ZrContainerTests_ImportNativeModule(state, "zr.container");
    TEST_ASSERT_NOT_NULL(module);
    spanValue = ZrContainerTests_GetModuleExportValue(state, module, "Span");
    TEST_ASSERT_NOT_NULL(spanValue);
    providerSpan = (SZrObjectPrototype *)ZR_CAST_OBJECT(
            state, spanValue->value.object);
    TEST_ASSERT_NOT_NULL(providerSpan);

    function = compile_source(state, "span_imported_inline_layout.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slot = &function->frameSlotLayouts[index];
        const SZrCompiledPrototypeInfo *prototype;

        if (slot->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            slot->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
            continue;
        }
        prototype = compiled_prototype_at(function, slot->typeLayoutId);
        TEST_ASSERT_NOT_NULL(prototype);
        if ((prototype->modifierFlags &
             ZR_TYPE_MODIFIER_FLAG_IMPORTED_LAYOUT_ONLY) == 0u) {
            continue;
        }
        TEST_ASSERT_NOT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                function, slot->typeLayoutId, state));
        resolvedSpan = ZrCore_Function_ResolvePrototypeFrameStructPrototype(
                state, function, slot->typeLayoutId);
        break;
    }

    TEST_ASSERT_EQUAL_PTR(providerSpan, resolvedSpan);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_constant_slice_index_elides_only_proven_bounds_checks(void) {
    static const char kConstantIndexSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view = xs.span();\n"
            "var sliced = view.slice(0, 2);\n"
            "return sliced[1];\n";
    static const char kDynamicIndexSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view = xs.span();\n"
            "var sliced = view.slice(0, 2);\n"
            "var index = 1;\n"
            "return sliced[index];\n";
    SZrState *state;
    SZrFunction *function;
    TZrInt64 result = 0;
    TZrSize constantBoundsBranchCount;

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, "span_constant_bounds.zr", kConstantIndexSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            count_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED)));
    constantBoundsBranchCount = count_opcode(
            function, ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(22, result);
    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, "span_dynamic_bounds.zr", kDynamicIndexSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            count_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED)));
    TEST_ASSERT_EQUAL_UINT64(
            constantBoundsBranchCount + 1u,
            count_opcode(
                    function,
                    ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED)));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(22, result);
    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_span_compiler_publishes_structured_view_and_bounds_facts(void) {
    static const char kSource[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view = xs.span();\n"
            "var sliced = view.slice(0, 2);\n"
            "return sliced[1];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *script;
    const SZrSemanticIrFunction *semanticIr;
    const SZrSemanticBoundsFact *boundsFact;
    const SZrSemanticContiguousViewFact *viewFact;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    script = parse_source(state, "span_semir_facts.zr", kSource);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    compiler->scriptAst = script;
    compiler->currentAst = script;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);

    for (TZrSize index = 0u;
         index < script->data.script.statements->count;
         index++) {
        ZrContainerTests_CompileTopLevelStatement(
                compiler, script->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);
    }
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(compiler));
    semanticIr = ZrParser_Compiler_PreSemanticIr(compiler);
    TEST_ASSERT_NOT_NULL(semanticIr);
    TEST_ASSERT_TRUE(semanticIr->contiguousViewFacts.length >= 4u);
    TEST_ASSERT_EQUAL_UINT64(1u, semanticIr->boundsFacts.length);
    for (TZrSize factIndex = 0u;
         factIndex < semanticIr->contiguousViewFacts.length;
         factIndex++) {
        const SZrSemanticContiguousViewFact *candidate =
                ZrParser_SemanticIr_ContiguousViewFactAt(
                        semanticIr, factIndex);
        TEST_ASSERT_NOT_NULL(candidate);
        for (TZrSize instructionIndex = 0u;
             instructionIndex < semanticIr->instructions.length;
             instructionIndex++) {
            const SZrSemanticIrInstruction *instruction =
                    ZrParser_SemanticIr_InstructionAt(
                            semanticIr, instructionIndex);
            if (instruction != ZR_NULL &&
                instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
                instruction->placeId == candidate->viewPlaceId) {
                TEST_ASSERT_EQUAL_UINT32(
                        instruction->valueId, candidate->viewValueId);
            }
        }
    }
    boundsFact = ZrParser_SemanticIr_BoundsFactAt(semanticIr, 0u);
    TEST_ASSERT_NOT_NULL(boundsFact);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_BOUNDS_PROOF_CONSTANT_RANGE, boundsFact->proofKind);
    TEST_ASSERT_TRUE(boundsFact->hasKnownIndex);
    TEST_ASSERT_TRUE(boundsFact->hasKnownLength);
    TEST_ASSERT_EQUAL_INT64(1, boundsFact->knownIndex);
    TEST_ASSERT_EQUAL_INT64(2, boundsFact->knownLength);
    TEST_ASSERT_TRUE(boundsFact->lowerBoundProven);
    TEST_ASSERT_TRUE(boundsFact->upperBoundProven);
    TEST_ASSERT_TRUE(boundsFact->checkElided);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID,
            boundsFact->contiguousViewFactId);
    viewFact = ZrParser_SemanticIr_ContiguousViewFactAt(
            semanticIr, boundsFact->contiguousViewFactId - 1u);
    TEST_ASSERT_NOT_NULL(viewFact);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_CONTIGUOUS_SOURCE_ARRAY, viewFact->sourceKind);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_REGION_ID_INVALID, viewFact->regionId);
    TEST_ASSERT_TRUE(viewFact->hasKnownStart);
    TEST_ASSERT_TRUE(viewFact->hasKnownLength);
    TEST_ASSERT_EQUAL_INT64(0, viewFact->knownStart);
    TEST_ASSERT_EQUAL_INT64(2, viewFact->knownLength);
    TEST_ASSERT_FALSE(viewFact->isReadOnly);

    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = NULL;
    ZrParser_Ast_Free(state, script);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_span_descriptors_publish_ref_like_contiguous_view_contracts);
    RUN_TEST(test_span_runtime_prototypes_preserve_contiguous_view_protocols);
    RUN_TEST(test_span_compiler_prototypes_project_ref_like_member_contracts);
    RUN_TEST(test_span_array_runtime_mutation_slice_and_readonly_view_share_storage);
    RUN_TEST(test_span_array_source_survives_gc_compaction_while_view_is_live);
    RUN_TEST(test_span_default_and_empty_slice_are_legal);
    RUN_TEST(test_span_index_and_slice_reject_out_of_range_access);
    RUN_TEST(test_readonly_span_rejects_index_assignment);
    RUN_TEST(test_span_implicitly_weakens_to_readonly_span_with_same_element_type);
    RUN_TEST(test_span_rejects_capability_strengthening_and_element_type_change);
    RUN_TEST(test_span_exact_overload_wins_over_readonly_weakening);
    RUN_TEST(test_span_slice_lowers_inline_without_native_callback_or_wrapper);
    RUN_TEST(test_imported_span_inline_layout_reuses_provider_prototype);
    RUN_TEST(test_span_constant_slice_index_elides_only_proven_bounds_checks);
    RUN_TEST(test_span_compiler_publishes_structured_view_and_bounds_facts);
    RUN_TEST(test_span_owner_move_and_native_drop_conflict_with_active_view);
    return UNITY_END();
}
