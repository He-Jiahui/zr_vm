#include <string.h>
#include <time.h>

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                   \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

static const SZrTypeValue *find_string_entry_in_object_array(SZrState *state, SZrObject *array, const char *expected) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || expected == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < ZrContainerTests_GetArrayLength(array); index++) {
        const SZrTypeValue *value = ZrContainerTests_GetArrayEntryValue(state, array, index);
        if (value != ZR_NULL &&
            value->type == ZR_VALUE_TYPE_STRING &&
            ZrContainerTests_StringEqualsCString(ZR_CAST_STRING(state, value->value.object), expected)) {
            return value;
        }
    }

    return ZR_NULL;
}

static TZrBool string_array_contains(const SZrArray *array, const char *expected) {
    TZrSize index;

    if (array == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < array->length; index++) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)array, index);
        if (namePtr != ZR_NULL &&
            *namePtr != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(*namePtr), expected) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrInferredType *member_parameter_type_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    if (memberInfo == ZR_NULL || index >= memberInfo->parameterTypes.length) {
        return ZR_NULL;
    }

    return (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
}

static SZrAstNode *parse_test_ast(SZrState *state, const char *path, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || path == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static void test_container_metadata_open_module_info_exposes_generic_shapes(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Metadata - Open Module Info Exposes Generic Shapes";
    SZrState *state;
    SZrObjectModule *module;
    const SZrTypeValue *moduleInfoValue;
    const SZrTypeValue *typesValue;
    SZrObject *moduleInfo;
    SZrObject *typesArray;
    SZrObject *arrayEntry;
    SZrObject *mapEntry;
    SZrObject *pairEntry;
    const SZrTypeValue *implementsValue;
    const SZrTypeValue *genericParametersValue;
    const SZrTypeValue *metaMethodsValue;
    const SZrTypeValue *constraintsValue;
    SZrObject *implementsArray;
    SZrObject *genericParametersArray;
    SZrObject *metaMethodsArray;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    module = ZrContainerTests_ImportNativeModule(state, "zr.container");
    TEST_ASSERT_NOT_NULL(module);

    moduleInfoValue = ZrContainerTests_GetModuleExportValue(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    TEST_ASSERT_NOT_NULL(moduleInfoValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);

    typesValue = ZrContainerTests_GetObjectFieldValue(state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
    typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
    TEST_ASSERT_EQUAL_UINT64(12, ZrContainerTests_GetArrayLength(typesArray));

    arrayEntry = ZrContainerTests_FindNamedEntryInArray(state, typesArray, "name", "Array");
    mapEntry = ZrContainerTests_FindNamedEntryInArray(state, typesArray, "name", "Map");
    pairEntry = ZrContainerTests_FindNamedEntryInArray(state, typesArray, "name", "Pair");
    TEST_ASSERT_NOT_NULL(arrayEntry);
    TEST_ASSERT_NOT_NULL(mapEntry);
    TEST_ASSERT_NOT_NULL(pairEntry);

    implementsValue = ZrContainerTests_GetObjectFieldValue(state, arrayEntry, "implements");
    TEST_ASSERT_NOT_NULL(implementsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, implementsValue->type);
    implementsArray = ZR_CAST_OBJECT(state, implementsValue->value.object);
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state, implementsArray, "ArrayLike<T>"));
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state, implementsArray, "Iterable<T>"));

    metaMethodsValue = ZrContainerTests_GetObjectFieldValue(state, arrayEntry, "metaMethods");
    TEST_ASSERT_NOT_NULL(metaMethodsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, metaMethodsValue->type);
    metaMethodsArray = ZR_CAST_OBJECT(state, metaMethodsValue->value.object);
    TEST_ASSERT_NOT_NULL(metaMethodsArray);
    TEST_ASSERT_EQUAL_UINT64(3, ZrContainerTests_GetArrayLength(metaMethodsArray));

    genericParametersValue = ZrContainerTests_GetObjectFieldValue(state, mapEntry, "genericParameters");
    TEST_ASSERT_NOT_NULL(genericParametersValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, genericParametersValue->type);
    genericParametersArray = ZR_CAST_OBJECT(state, genericParametersValue->value.object);
    TEST_ASSERT_EQUAL_UINT64(2, ZrContainerTests_GetArrayLength(genericParametersArray));
    constraintsValue = ZrContainerTests_GetObjectFieldValue(
            state,
            ZrContainerTests_GetArrayEntryObject(state, genericParametersArray, 0),
            "constraints");
    TEST_ASSERT_NOT_NULL(constraintsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constraintsValue->type);
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state,
                                                           ZR_CAST_OBJECT(state, constraintsValue->value.object),
                                                           "Hashable"));
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state,
                                                           ZR_CAST_OBJECT(state, constraintsValue->value.object),
                                                           "Equatable"));

    implementsValue = ZrContainerTests_GetObjectFieldValue(state, pairEntry, "implements");
    TEST_ASSERT_NOT_NULL(implementsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, implementsValue->type);
    implementsArray = ZR_CAST_OBJECT(state, implementsValue->value.object);
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state, implementsArray, "Equatable<Pair<K,V>>"));
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state, implementsArray, "Comparable<Pair<K,V>>"));
    TEST_ASSERT_NOT_NULL(find_string_entry_in_object_array(state, implementsArray, "Hashable"));

    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_metadata_closed_native_prototypes_substitute_members_and_interfaces(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Metadata - Closed Native Prototypes Substitute Members And Interfaces";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Map, Pair, LinkedList} = %import(\"zr.container\");\n"
            "var xs: Array<int> = new container.Array<int>();\n"
            "var map: Map<string, int> = new container.Map<string, int>();\n"
            "var pair: Pair<int, string> = new container.Pair<int, string>(1, \"a\");\n"
            "var list: LinkedList<int> = new container.LinkedList<int>();\n"
            "var node = list.addLast(1);\n";
    const SZrTypePrototypeInfo *arrayPrototype;
    const SZrTypePrototypeInfo *mapPrototype;
    const SZrTypePrototypeInfo *pairPrototype;
    const SZrTypePrototypeInfo *listPrototype;
    const SZrTypePrototypeInfo *nodePrototype;
    const SZrTypeMemberInfo *arrayGetItem;
    const SZrTypeMemberInfo *mapSetItem;
    const SZrTypeMemberInfo *listAddLast;
    const SZrTypeMemberInfo *nodeNext;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    ast = parse_test_ast(state, "container_closed_metadata_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
        ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(cs->hasError);
    }

    arrayPrototype = ZrContainerTests_FindTypePrototype(cs, "Array<int>");
    mapPrototype = ZrContainerTests_FindTypePrototype(cs, "Map<string, int>");
    pairPrototype = ZrContainerTests_FindTypePrototype(cs, "Pair<int, string>");
    listPrototype = ZrContainerTests_FindTypePrototype(cs, "LinkedList<int>");
    nodePrototype = ZrContainerTests_FindTypePrototype(cs, "LinkedNode<int>");

    TEST_ASSERT_NOT_NULL(arrayPrototype);
    TEST_ASSERT_NOT_NULL(mapPrototype);
    TEST_ASSERT_NOT_NULL(pairPrototype);
    TEST_ASSERT_NOT_NULL(listPrototype);
    TEST_ASSERT_NOT_NULL(nodePrototype);
    TEST_ASSERT_EQUAL_UINT64(1, ZrContainerTests_CountTypePrototypes(cs, "Array<int>"));

    TEST_ASSERT_TRUE(string_array_contains(&arrayPrototype->implements, "ArrayLike<int>"));
    TEST_ASSERT_TRUE(string_array_contains(&arrayPrototype->implements, "Iterable<int>"));
    TEST_ASSERT_TRUE(string_array_contains(&mapPrototype->implements, "Iterable<Pair<string, int>>"));
    TEST_ASSERT_TRUE(string_array_contains(&pairPrototype->implements, "Equatable<Pair<int, string>>"));
    TEST_ASSERT_TRUE(string_array_contains(&pairPrototype->implements, "Comparable<Pair<int, string>>"));
    TEST_ASSERT_TRUE(string_array_contains(&pairPrototype->implements, "Hashable"));

    arrayGetItem = ZrContainerTests_FindMetaMember(arrayPrototype, ZR_META_GET_ITEM);
    mapSetItem = ZrContainerTests_FindMetaMember(mapPrototype, ZR_META_SET_ITEM);
    listAddLast = ZrContainerTests_FindTypeMemberByName(listPrototype, "addLast");
    nodeNext = ZrContainerTests_FindTypeMemberByName(nodePrototype, "next");

    TEST_ASSERT_NOT_NULL(arrayGetItem);
    TEST_ASSERT_NOT_NULL(mapSetItem);
    TEST_ASSERT_NOT_NULL(listAddLast);
    TEST_ASSERT_NOT_NULL(nodeNext);
    TEST_ASSERT_NOT_NULL(arrayGetItem->returnTypeName);
    TEST_ASSERT_NOT_NULL(mapSetItem->returnTypeName);
    TEST_ASSERT_NOT_NULL(listAddLast->returnTypeName);
    TEST_ASSERT_NOT_NULL(nodeNext->fieldTypeName);

    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(arrayGetItem->returnTypeName));
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(mapSetItem->returnTypeName));
    TEST_ASSERT_EQUAL_STRING("LinkedNode<int>", ZrCore_String_GetNativeString(listAddLast->returnTypeName));
    TEST_ASSERT_EQUAL_STRING("LinkedNode<int>", ZrCore_String_GetNativeString(nodeNext->fieldTypeName));

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_metadata_closed_native_method_parameter_types_preserve_specialized_value_kinds(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Metadata - Closed Native Method Parameters Preserve Specialized Value Kinds";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Map, LinkedList} = %import(\"zr.container\");\n"
            "var map: Map<string, int> = new container.Map<string, int>();\n"
            "var list: LinkedList<int> = new container.LinkedList<int>();\n";
    const SZrTypePrototypeInfo *mapPrototype;
    const SZrTypePrototypeInfo *listPrototype;
    const SZrTypeMemberInfo *mapContainsKey;
    const SZrTypeMemberInfo *listAddLast;
    const SZrInferredType *mapKeyParameter;
    const SZrInferredType *listValueParameter;
    SZrInferredType directString;
    SZrInferredType directInt;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_closed_method_parameter_metadata_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
        ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(cs->hasError);
    }

    mapPrototype = ZrContainerTests_FindTypePrototype(cs, "Map<string, int>");
    listPrototype = ZrContainerTests_FindTypePrototype(cs, "LinkedList<int>");
    TEST_ASSERT_NOT_NULL(mapPrototype);
    TEST_ASSERT_NOT_NULL(listPrototype);

    mapContainsKey = ZrContainerTests_FindTypeMemberByName(mapPrototype, "containsKey");
    listAddLast = ZrContainerTests_FindTypeMemberByName(listPrototype, "addLast");
    TEST_ASSERT_NOT_NULL(mapContainsKey);
    TEST_ASSERT_NOT_NULL(listAddLast);

    mapKeyParameter = member_parameter_type_at(mapContainsKey, 0);
    listValueParameter = member_parameter_type_at(listAddLast, 0);
    TEST_ASSERT_NOT_NULL(mapKeyParameter);
    TEST_ASSERT_NOT_NULL(listValueParameter);

    ZrParser_InferredType_Init(state, &directString, ZR_VALUE_TYPE_STRING);
    ZrParser_InferredType_Init(state, &directInt, ZR_VALUE_TYPE_INT64);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, mapKeyParameter->baseType);
    TEST_ASSERT_NOT_NULL(mapKeyParameter->typeName);
    TEST_ASSERT_EQUAL_STRING("string", ZrCore_String_GetNativeString(mapKeyParameter->typeName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(mapKeyParameter, &directString));
    TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&directString, mapKeyParameter));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, listValueParameter->baseType);
    TEST_ASSERT_NOT_NULL(listValueParameter->typeName);
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(listValueParameter->typeName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(listValueParameter, &directInt));
    TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&directInt, listValueParameter));

    ZrParser_InferredType_Free(state, &directInt);
    ZrParser_InferredType_Free(state, &directString);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_container_metadata_open_module_info_exposes_generic_shapes);
    RUN_TEST(test_container_metadata_closed_native_prototypes_substitute_members_and_interfaces);
    RUN_TEST(test_container_metadata_closed_native_method_parameter_types_preserve_specialized_value_kinds);
    return UNITY_END();
}
