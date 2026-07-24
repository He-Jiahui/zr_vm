#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_iteration/module.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/iteration_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_system.h"

static const ZrLibTypeDescriptor *find_type(const ZrLibModuleDescriptor *descriptor, const char *name) {
    TZrSize index;

    if (descriptor == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->typeCount; ++index) {
        const ZrLibTypeDescriptor *type = &descriptor->types[index];
        if (type->name != ZR_NULL && strcmp(type->name, name) == 0) {
            return type;
        }
    }

    return ZR_NULL;
}

static const ZrLibMethodDescriptor *find_method(const ZrLibTypeDescriptor *type, const char *name) {
    TZrSize index;

    if (type == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < type->methodCount; ++index) {
        const ZrLibMethodDescriptor *method = &type->methods[index];
        if (method->name != ZR_NULL && strcmp(method->name, name) == 0) {
            return method;
        }
    }

    return ZR_NULL;
}

static TZrBool type_implements(const ZrLibTypeDescriptor *type, const char *typeName) {
    TZrSize index;

    if (type == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < type->implementsTypeCount; ++index) {
        const TZrChar *implemented = type->implementsTypeNames[index];
        if (implemented != ZR_NULL && strcmp(implemented, typeName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; ++index) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_iteration_descriptor_owns_public_enumerator_contract(void) {
    const ZrLibModuleDescriptor *descriptor = ZrVmLibIteration_GetModuleDescriptor();
    const ZrLibTypeDescriptor *iterable;
    const ZrLibTypeDescriptor *enumerator;
    const ZrLibTypeDescriptor *iterator;
    const ZrLibTypeDescriptor *asyncIterator;
    const ZrLibMethodDescriptor *getEnumerator;
    const ZrLibMethodDescriptor *moveNext;
    const ZrLibMethodDescriptor *close;

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_STRING("zr.iteration", descriptor->moduleName);
    TEST_ASSERT_EQUAL_UINT64(4u, descriptor->typeCount);

    iterable = find_type(descriptor, "Iterable");
    enumerator = find_type(descriptor, "Enumerator");
    iterator = find_type(descriptor, "Iterator");
    asyncIterator = find_type(descriptor, "AsyncIterator");
    TEST_ASSERT_NOT_NULL(iterable);
    TEST_ASSERT_NOT_NULL(enumerator);
    TEST_ASSERT_NOT_NULL(iterator);
    TEST_ASSERT_NOT_NULL(asyncIterator);

    TEST_ASSERT_EQUAL_UINT64(ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE), iterable->protocolMask);
    TEST_ASSERT_EQUAL_UINT64(ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR), enumerator->protocolMask);
    TEST_ASSERT_EQUAL_UINT64(ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR), iterator->protocolMask);
    TEST_ASSERT_FALSE(iterator->allowValueConstruction);
    TEST_ASSERT_FALSE(iterator->allowBoxedConstruction);

    getEnumerator = find_method(iterable, "getEnumerator");
    moveNext = find_method(enumerator, "moveNext");
    close = find_method(asyncIterator, "close");
    TEST_ASSERT_NOT_NULL(getEnumerator);
    TEST_ASSERT_NOT_NULL(moveNext);
    TEST_ASSERT_NOT_NULL(close);
    TEST_ASSERT_EQUAL_STRING("zr.iteration.Enumerator<T>", getEnumerator->returnTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT, getEnumerator->contractRole);
    TEST_ASSERT_EQUAL_STRING("bool", moveNext->returnTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT, moveNext->contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<void>", close->returnTypeName);
}

static void test_container_descriptor_uses_iteration_contract_owner(void) {
    const ZrLibModuleDescriptor *container = ZrVmLibContainer_GetModuleDescriptor();
    const ZrLibTypeDescriptor *array;
    const ZrLibTypeDescriptor *map;
    const ZrLibTypeDescriptor *set;
    const ZrLibTypeDescriptor *linkedList;
    const ZrLibMethodDescriptor *getEnumerator;

    TEST_ASSERT_NOT_NULL(container);
    array = find_type(container, "Array");
    map = find_type(container, "Map");
    set = find_type(container, "Set");
    linkedList = find_type(container, "LinkedList");
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_NOT_NULL(map);
    TEST_ASSERT_NOT_NULL(set);
    TEST_ASSERT_NOT_NULL(linkedList);

    TEST_ASSERT_TRUE(type_implements(array, "zr.iteration.Iterable<T>"));
    TEST_ASSERT_TRUE(type_implements(map, "zr.iteration.Iterable<Pair<K,V>>"));
    TEST_ASSERT_TRUE(type_implements(set, "zr.iteration.Iterable<T>"));
    TEST_ASSERT_TRUE(type_implements(linkedList, "zr.iteration.Iterable<T>"));
    TEST_ASSERT_FALSE(type_implements(array, "zr.builtin.IEnumerable<T>"));

    getEnumerator = find_method(array, "getIterator");
    TEST_ASSERT_NOT_NULL(getEnumerator);
    TEST_ASSERT_EQUAL_STRING("zr.iteration.Enumerator<T>", getEnumerator->returnTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT, getEnumerator->contractRole);
}

static void test_builtin_descriptor_does_not_own_iteration_protocols(void) {
    SZrState *state = ZrContainerTests_CreateState();
    const ZrLibModuleDescriptor *builtin;

    TEST_ASSERT_NOT_NULL(state);
    builtin = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.builtin");
    TEST_ASSERT_NOT_NULL(builtin);
    TEST_ASSERT_NULL(find_type(builtin, "IEnumerable"));
    TEST_ASSERT_NULL(find_type(builtin, "IEnumerator"));

    ZrContainerTests_DestroyState(state);
}

static void test_enumerator_binding_uses_protocol_facts_only(void) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler = ZrContainerTests_CreateCompilerState(state);
    SZrInferredType enumerator;
    SZrInferredType iterable;
    SZrInferredType iterator;
    SZrInferredType rawArray;
    SZrInferredType refLikeIterator;
    SZrInferredType noCapability;
    SZrInferredType intType;
    SZrInferredType elementType;
    SZrString *sourceName;
    SZrAstNode *ast;
    const char *source = "var iteration = %import(\"zr.iteration\");\n";

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(compiler);

    sourceName = ZrCore_String_CreateFromNative(state, "enumerator_protocol_binding_test.zr");
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    compiler->scriptAst = ast;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(compiler, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(compiler->hasError);
    TEST_ASSERT_NOT_NULL(ZrContainerTests_FindTypePrototype(compiler, "Enumerator"));
    TEST_ASSERT_NOT_NULL(ZrContainerTests_FindTypePrototype(compiler, "Iterable"));
    TEST_ASSERT_NOT_NULL(ZrContainerTests_FindTypePrototype(compiler, "Iterator"));

    ZrParser_InferredType_InitFull(state,
                                   &enumerator,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   ZrCore_String_CreateFromNative(state, "Enumerator<int>"));
    ZrParser_InferredType_InitFull(state,
                                   &iterable,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   ZrCore_String_CreateFromNative(state, "Iterable<int>"));
    ZrParser_InferredType_InitFull(state,
                                   &iterator,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   ZrCore_String_CreateFromNative(state, "Iterator<int>"));
    ZrParser_InferredType_Init(state, &rawArray, ZR_VALUE_TYPE_ARRAY);
    ZrParser_InferredType_Init(state, &refLikeIterator, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(state, &noCapability, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(state, &rawArray.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &rawArray.elementTypes, &intType);
    refLikeIterator.referenceAccess = ZR_REFERENCE_ACCESS_READONLY;
    refLikeIterator.isReadonlyView = ZR_TRUE;
    refLikeIterator.protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR);
    ZrCore_Array_Init(state, &refLikeIterator.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &refLikeIterator.elementTypes, &intType);

    ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_EnumeratorBinding_ResolveElementType(compiler, &enumerator, &elementType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, elementType.baseType);
    ZrParser_InferredType_Free(state, &elementType);

    ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_EnumeratorBinding_ResolveElementType(compiler, &iterable, &elementType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, elementType.baseType);
    ZrParser_InferredType_Free(state, &elementType);

    ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_EnumeratorBinding_ResolveElementType(compiler, &iterator, &elementType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, elementType.baseType);
    ZrParser_InferredType_Free(state, &elementType);

    ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_EnumeratorBinding_ResolveElementType(compiler, &refLikeIterator, &elementType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, elementType.baseType);
    ZrParser_InferredType_Free(state, &elementType);

    ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_EnumeratorBinding_ResolveElementType(compiler, &rawArray, &elementType));
    TEST_ASSERT_FALSE(ZrParser_EnumeratorBinding_ResolveElementType(compiler, &noCapability, &elementType));
    ZrParser_InferredType_Free(state, &elementType);

    ZrParser_InferredType_Free(state, &intType);
    ZrParser_InferredType_Free(state, &noCapability);
    ZrParser_InferredType_Free(state, &refLikeIterator);
    ZrParser_InferredType_Free(state, &rawArray);
    ZrParser_InferredType_Free(state, &iterator);
    ZrParser_InferredType_Free(state, &iterable);
    ZrParser_InferredType_Free(state, &enumerator);
    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void test_typed_foreach_keeps_static_iterator_lowering_through_break_cleanup(void) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;
    const char *source =
            "for(var item in [1, 2, 3]) {\n"
            "    if(item > 1) { break; }\n"
            "}\n";

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "enumerator_static_lowering_test.zr");
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    function = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_INIT)));
    TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) ||
                     function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)));
    TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_CURRENT)));
    TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_ITER_INIT)));
    TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)));

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_iteration_descriptor_owns_public_enumerator_contract);
    RUN_TEST(test_container_descriptor_uses_iteration_contract_owner);
    RUN_TEST(test_builtin_descriptor_does_not_own_iteration_protocols);
    RUN_TEST(test_enumerator_binding_uses_protocol_facts_only);
    RUN_TEST(test_typed_foreach_keeps_static_iterator_lowering_through_break_cleanup);
    return UNITY_END();
}
