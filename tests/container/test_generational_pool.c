#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "unity.h"

#include "container_test_common.h"
#include "harness/runtime_support.h"
#include "zr_vm_lib_container/generational_pool.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

ZR_PARSER_API TZrBool compiler_validate_ref_struct_rules(
        SZrCompilerState *compiler,
        SZrAstNode *node);
ZR_PARSER_API TZrBool compiler_validate_reference_escapes(
        SZrCompilerState *compiler,
        SZrAstNode *node);

typedef struct STestElement {
    int value;
    int marker;
} STestElement;

typedef struct STestCallbacks {
    uint64_t initializeCount;
    uint64_t dropCount;
    uint64_t scanCount;
} STestCallbacks;

typedef struct SConcurrentWorker {
    SZrPool *pool;
    int workerId;
    TZrSize iterationCount;
    int failureStep;
} SConcurrentWorker;

static TZrBool test_element_initialize(
        void *destination,
        const void *source,
        void *context) {
    STestCallbacks *callbacks = (STestCallbacks *)context;
    const STestElement *input = (const STestElement *)source;

    callbacks->initializeCount++;
    if (input == ZR_NULL || input->value < 0) {
        return ZR_FALSE;
    }
    memcpy(destination, input, sizeof(*input));
    return ZR_TRUE;
}

static void test_element_drop(void *element, void *context) {
    STestCallbacks *callbacks = (STestCallbacks *)context;
    STestElement *value = (STestElement *)element;

    callbacks->dropCount++;
    value->value = -1;
    value->marker = -1;
}

static void test_element_scan(void *element, void *context) {
    STestCallbacks *callbacks = (STestCallbacks *)context;
    STestElement *value = (STestElement *)element;

    TEST_ASSERT_EQUAL_INT(0x5a5a, value->marker);
    callbacks->scanCount++;
}

static SZrPoolTypeLayout test_layout(
        STestCallbacks *callbacks,
        EZrPoolGcScanKind scanKind,
        TZrSize alignment) {
    SZrPoolTypeLayout layout;

    memset(&layout, 0, sizeof(layout));
    layout.elementSize = sizeof(STestElement);
    layout.elementAlignment = alignment;
    layout.gcScanKind = scanKind;
    layout.initialize = test_element_initialize;
    layout.drop = test_element_drop;
    layout.scan = scanKind == ZR_POOL_GC_SCAN_FREE
                          ? ZR_NULL
                          : test_element_scan;
    layout.context = callbacks;
    return layout;
}

static SZrPoolConfig test_config(TZrSize slabCapacity, uint64_t generationLimit) {
    SZrPoolConfig config;

    config.slabCapacity = slabCapacity;
    config.generationLimit = generationLimit;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_THREAD_LOCAL;
    return config;
}

static const ZrLibTypeDescriptor *find_pooling_type(
        const ZrLibModuleDescriptor *module,
        const TZrChar *name) {
    for (TZrSize index = 0u; index < module->typeCount; index++) {
        if (strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return ZR_NULL;
}

static const ZrLibMethodDescriptor *find_pooling_method(
        const ZrLibTypeDescriptor *type,
        TZrUInt32 role) {
    for (TZrSize index = 0u; index < type->methodCount; index++) {
        if (type->methods[index].contractRole == role) {
            return &type->methods[index];
        }
    }
    return ZR_NULL;
}

static SZrAstNode *parse_pooling_source(
        SZrState *state,
        const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_Create(
            state,
            (TZrNativeString)"generational_pool_metadata.zr",
            strlen("generational_pool_metadata.zr"));

    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static void test_native_descriptor_publishes_stable_slot_contract_by_role(void) {
    const ZrLibModuleDescriptor *module =
            ZrVmLibContainer_GetPoolingModuleDescriptor();
    const ZrLibTypeDescriptor *pool;
    const ZrLibTypeDescriptor *handle;
    const ZrLibTypeDescriptor *writeRef;
    const ZrLibTypeDescriptor *readRef;
    const ZrLibMethodDescriptor *deliver;
    const ZrLibMethodDescriptor *tryRead;
    const ZrLibMethodDescriptor *tryBorrow;

    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_STRING("zr.pooling", module->moduleName);
    TEST_ASSERT_EQUAL_UINT64(1u, module->constantCount);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
            (uint64_t)module->constants[0].intValue);
    pool = find_pooling_type(module, "Pool");
    handle = find_pooling_type(module, "PoolHandle");
    writeRef = find_pooling_type(module, "PoolRef");
    readRef = find_pooling_type(module, "PoolReadRef");
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(writeRef);
    TEST_ASSERT_NOT_NULL(readRef);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE),
            pool->protocolMask);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, handle->prototypeType);
    TEST_ASSERT_EQUAL_UINT64(3u, handle->fieldCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_POOL_ID,
            handle->fields[0].contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_SLOT,
            handle->fields[1].contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_GENERATION,
            handle->fields[2].contractRole);
    deliver = find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_DELIVER);
    TEST_ASSERT_NOT_NULL(deliver);
    TEST_ASSERT_EQUAL_STRING("PoolHandle<T>", deliver->returnTypeName);
    TEST_ASSERT_NOT_NULL(find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_VALIDATE));
    TEST_ASSERT_NOT_NULL(find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_RECYCLE));
    tryRead = find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ);
    tryBorrow = find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE);
    TEST_ASSERT_NOT_NULL(tryRead);
    TEST_ASSERT_NOT_NULL(tryBorrow);
    TEST_ASSERT_EQUAL_STRING("bool", tryRead->returnTypeName);
    TEST_ASSERT_EQUAL_STRING("bool", tryBorrow->returnTypeName);
    TEST_ASSERT_EQUAL_UINT64(2u, tryRead->parameterCount);
    TEST_ASSERT_EQUAL_UINT64(2u, tryBorrow->parameterCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryRead->minArgumentCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryRead->maxArgumentCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryBorrow->minArgumentCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryBorrow->maxArgumentCount);
    TEST_ASSERT_EQUAL_STRING("handle", tryRead->parameters[0].name);
    TEST_ASSERT_EQUAL_STRING("view", tryRead->parameters[1].name);
    TEST_ASSERT_EQUAL_STRING("PoolReadRef<T>", tryRead->parameters[1].typeName);
    TEST_ASSERT_EQUAL_STRING("handle", tryBorrow->parameters[0].name);
    TEST_ASSERT_EQUAL_STRING("view", tryBorrow->parameters[1].name);
    TEST_ASSERT_EQUAL_STRING("PoolRef<T>", tryBorrow->parameters[1].typeName);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE), writeRef->protocolMask);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE), readRef->protocolMask);
    TEST_ASSERT_FALSE(writeRef->allowBoxedConstruction);
    TEST_ASSERT_FALSE(readRef->allowBoxedConstruction);
    TEST_ASSERT_EQUAL_STRING(
            "__zr_pool_guard_value", writeRef->fields[0].name);
    TEST_ASSERT_TRUE(writeRef->fields[0].runtimeOnly);
    TEST_ASSERT_FALSE(writeRef->fields[0].isReadonly);
    TEST_ASSERT_TRUE(readRef->fields[0].runtimeOnly);
    TEST_ASSERT_TRUE(readRef->fields[0].isReadonly);
    TEST_ASSERT_EQUAL_STRING("value", writeRef->methods[0].propertyName);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIB_REFERENCE_ACCESS_WRITABLE,
            writeRef->methods[0].propertyReferenceAccess);
    TEST_ASSERT_TRUE(writeRef->methods[0].propertyExportsWritableRef);
    TEST_ASSERT_EQUAL_STRING("value", readRef->methods[0].propertyName);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIB_REFERENCE_ACCESS_READONLY,
            readRef->methods[0].propertyReferenceAccess);
    TEST_ASSERT_FALSE(readRef->methods[0].propertyExportsWritableRef);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION,
            writeRef->methods[0].contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE,
            writeRef->methods[1].contractRole);
    for (TZrSize index = 0u; index < pool->methodCount; index++) {
        TEST_ASSERT_NOT_NULL(pool->methods[index].callback);
    }
    TEST_ASSERT_NOT_NULL(writeRef->methods[0].callback);
    TEST_ASSERT_NOT_NULL(writeRef->methods[1].callback);
    TEST_ASSERT_NOT_NULL(writeRef->metaMethods[0].callback);
    TEST_ASSERT_NOT_NULL(readRef->methods[0].callback);
    TEST_ASSERT_NOT_NULL(readRef->methods[1].callback);
    TEST_ASSERT_NOT_NULL(readRef->metaMethods[0].callback);
}

static void test_native_pool_reflection_hides_guard_bypass_and_runtime_storage(void) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrObjectModule *module;
    const SZrTypeValue *poolRefValue;
    SZrTypeValue descriptorValue;
    SZrObject *descriptor;
    const SZrTypeValue *idValue;
    SZrReflectionTypeIdentity identity;
    SZrReflectionMemberQuery query;
    SZrObject *member = ZR_NULL;
    const SZrTypeValue *getterValue;
    const SZrTypeValue *setterValue;
    const SZrTypeValue *referenceAccessValue;
    EZrReflectionQueryStatus queryStatus;
    EZrReflectionConstructionStatus constructionStatus;
    SZrTypeValue instance;

    TEST_ASSERT_NOT_NULL(state);
    module = ZrContainerTests_ImportNativeModule(state, "zr.pooling");
    TEST_ASSERT_NOT_NULL(module);
    poolRefValue = ZrContainerTests_GetModuleExportValue(
            state, module, "PoolRef");
    TEST_ASSERT_NOT_NULL(poolRefValue);
    ZrCore_Value_ResetAsNull(&descriptorValue);
    TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
            state, poolRefValue, &descriptorValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, descriptorValue.type);
    descriptor = ZR_CAST_OBJECT(state, descriptorValue.value.object);
    TEST_ASSERT_NOT_NULL(descriptor);
    idValue = ZrContainerTests_GetObjectFieldValue(state, descriptor, "id");
    TEST_ASSERT_NOT_NULL(idValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, idValue->type);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadTypeIdObject(
            state,
            ZR_CAST_OBJECT(state, idValue->value.object),
            &identity,
            ZR_NULL));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT, identity.category);

    ZrCore_Reflection_MemberQueryInitDefault(&query);
    TEST_ASSERT_FALSE(ZrCore_Reflection_GetMember(
            state,
            descriptor,
            ZrCore_String_CreateFromNative(state, "__zr_pool_guard_value"),
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            ZR_NULL,
            0u,
            &query,
            &member,
            &queryStatus));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_NOT_FOUND, queryStatus);
    TEST_ASSERT_FALSE(ZrCore_Reflection_GetMember(
            state,
            descriptor,
            ZrCore_String_CreateFromNative(state, "__get_value"),
            ZR_REFLECTION_MEMBER_KIND_METHOD,
            ZR_NULL,
            0u,
            &query,
            &member,
            &queryStatus));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_NOT_FOUND, queryStatus);
    TEST_ASSERT_TRUE(ZrCore_Reflection_GetMember(
            state,
            descriptor,
            ZrCore_String_CreateFromNative(state, "value"),
            ZR_REFLECTION_MEMBER_KIND_PROPERTY,
            ZR_NULL,
            0u,
            &query,
            &member,
            &queryStatus));
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_OK, queryStatus);
    getterValue = ZrContainerTests_GetObjectFieldValue(state, member, "getter");
    setterValue = ZrContainerTests_GetObjectFieldValue(state, member, "setter");
    referenceAccessValue = ZrContainerTests_GetObjectFieldValue(
            state, member, "referenceAccess");
    TEST_ASSERT_NOT_NULL(getterValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, getterValue->type);
    TEST_ASSERT_NULL(setterValue);
    TEST_ASSERT_NOT_NULL(referenceAccessValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(referenceAccessValue->type));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            referenceAccessValue->value.nativeObject.nativeInt64);

    ZrCore_Value_ResetAsNull(&instance);
    TEST_ASSERT_FALSE(ZrCore_Reflection_CreateInstance(
            state,
            descriptor,
            ZR_NULL,
            0u,
            &instance,
            &constructionStatus));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_CONSTRUCTION_STATUS_TYPE_NOT_CONSTRUCTIBLE,
            constructionStatus);
    ZrContainerTests_DestroyState(state);
}

static void test_native_pool_executes_identity_and_recycle_from_source(void) {
    static const TZrChar *source =
            "var {Pool} = import(\"zr.pooling\");\n"
            "var pool = new Pool<int>();\n"
            "var handle = pool.deliver(41);\n"
            "if (!pool.isLive(handle)) { return -1; }\n"
            "if (!pool.recycle(handle)) { return -2; }\n"
            "if (pool.isLive(handle)) { return -3; }\n"
            "return 41;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "generational_pool_language_identity.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(41, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_native_pool_executes_scoped_read_write_guards_from_source(void) {
    static const TZrChar *source =
            "var {Pool, PoolRef, PoolReadRef} = import(\"zr.pooling\");\n"
            "var pool = new Pool<int>();\n"
            "var handle = pool.deliver(41);\n"
            "var readView: PoolReadRef<int>;\n"
            "if (!pool.tryRead(handle, out readView)) { return -1; }\n"
            "if (readView.value != 41) { return -2; }\n"
            "var writeView: PoolRef<int>;\n"
            "if (pool.tryBorrow(handle, out writeView)) { return -3; }\n"
            "readView.close();\n"
            "if (!pool.tryBorrow(handle, out writeView)) { return -4; }\n"
            "writeView.value = 42;\n"
            "writeView.close();\n"
            "if (!pool.tryRead(handle, out readView)) { return -5; }\n"
            "if (readView.value != 42) { return -6; }\n"
            "if (!pool.recycle(handle)) { return -7; }\n"
            "if (pool.tryRead(handle, out readView)) { return -8; }\n"
            "return 42;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "generational_pool_language_guard.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_native_semantic_import_preserves_ref_like_and_stable_slot_protocols(void) {
    static const TZrChar *source =
            "var {Pool, PoolHandle, PoolRef, PoolReadRef} = "
            "import(\"zr.pooling\");\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;
    const SZrTypePrototypeInfo *pool;
    const SZrTypePrototypeInfo *handle;
    const SZrTypePrototypeInfo *writeRef;
    const SZrTypePrototypeInfo *readRef;
    const SZrTypeMemberInfo *deliver;
    const SZrTypeMemberInfo *tryRead;
    const SZrTypeMemberInfo *tryBorrow;
    const SZrTypeMemberInfo *valueProjection;
    const SZrTypeMemberInfo *readValueProjection;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    ast = parse_pooling_source(state, source);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    compiler->scriptAst = ast;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    for (TZrSize index = 0u;
         index < ast->data.script.statements->count;
         index++) {
        ZrContainerTests_CompileTopLevelStatement(
                compiler, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(compiler->hasError);
    }
    pool = ZrContainerTests_FindTypePrototype(compiler, "Pool");
    handle = ZrContainerTests_FindTypePrototype(compiler, "PoolHandle");
    writeRef = ZrContainerTests_FindTypePrototype(compiler, "PoolRef");
    readRef = ZrContainerTests_FindTypePrototype(compiler, "PoolReadRef");
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(writeRef);
    TEST_ASSERT_NOT_NULL(readRef);
    TEST_ASSERT_TRUE(
            (pool->protocolMask &
             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE)) != 0u);
    TEST_ASSERT_TRUE(
            (writeRef->modifierFlags & ZR_DECLARATION_MODIFIER_REF_LIKE) != 0u);
    TEST_ASSERT_TRUE(
            (readRef->modifierFlags & ZR_DECLARATION_MODIFIER_REF_LIKE) != 0u);
    TEST_ASSERT_FALSE(writeRef->allowBoxedConstruction);
    TEST_ASSERT_FALSE(readRef->allowBoxedConstruction);
    TEST_ASSERT_EQUAL_UINT64(3u, handle->members.length);
    deliver = ZrContainerTests_FindTypeMemberByName(pool, "deliver");
    tryRead = ZrContainerTests_FindTypeMemberByName(pool, "tryRead");
    tryBorrow = ZrContainerTests_FindTypeMemberByName(pool, "tryBorrow");
    valueProjection = ZrContainerTests_FindTypeMemberByName(writeRef, "value");
    readValueProjection =
            ZrContainerTests_FindTypeMemberByName(readRef, "value");
    TEST_ASSERT_NOT_NULL(deliver);
    TEST_ASSERT_EQUAL_STRING(
            "PoolHandle<T>",
            ZrCore_String_GetNativeString(deliver->returnTypeName));
    TEST_ASSERT_NOT_NULL(tryRead);
    TEST_ASSERT_NOT_NULL(tryBorrow);
    TEST_ASSERT_NOT_NULL(valueProjection);
    TEST_ASSERT_NOT_NULL(readValueProjection);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ,
            tryRead->contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE,
            tryBorrow->contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION,
            valueProjection->contractRole);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_DECLARATION,
                          valueProjection->memberType);
    TEST_ASSERT_TRUE(valueProjection->hasStructuredReturnType);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            valueProjection->structuredReturnType.referenceAccess);
    TEST_ASSERT_TRUE(valueProjection->exportsWritableRef);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_READONLY,
            readValueProjection->structuredReturnType.referenceAccess);
    TEST_ASSERT_FALSE(readValueProjection->exportsWritableRef);
    TEST_ASSERT_EQUAL_UINT32(2u, tryRead->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(2u, tryBorrow->parameterCount);
    TEST_ASSERT_EQUAL_STRING(
            "bool", ZrCore_String_GetNativeString(tryRead->returnTypeName));
    TEST_ASSERT_EQUAL_STRING(
            "bool", ZrCore_String_GetNativeString(tryBorrow->returnTypeName));
    TEST_ASSERT_TRUE(tryRead->parameterPassingModes.isValid);
    TEST_ASSERT_TRUE(tryBorrow->parameterPassingModes.isValid);
    TEST_ASSERT_EQUAL_UINT64(2u, tryRead->parameterPassingModes.length);
    TEST_ASSERT_EQUAL_UINT64(2u, tryBorrow->parameterPassingModes.length);
    TEST_ASSERT_EQUAL_UINT64(2u, tryRead->parameterTypes.length);
    TEST_ASSERT_EQUAL_UINT64(2u, tryBorrow->parameterTypes.length);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE),
            ((const SZrInferredType *)ZrCore_Array_Get(
                     (SZrArray *)&tryRead->parameterTypes, 1u))
                    ->protocolMask);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE),
            ((const SZrInferredType *)ZrCore_Array_Get(
                     (SZrArray *)&tryBorrow->parameterTypes, 1u))
                    ->protocolMask);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_VALUE,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryRead->parameterPassingModes, 0u));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_OUT,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryRead->parameterPassingModes, 1u));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_VALUE,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryBorrow->parameterPassingModes, 0u));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_OUT,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryBorrow->parameterPassingModes, 1u));

    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void assert_native_pool_ref_storage_rejected(
        const TZrChar *source,
        const TZrChar *expectedMessage) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    compiler->suppressErrorOutput = ZR_TRUE;
    ast = parse_pooling_source(state, source);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT64(1u, ast->data.script.statements->count);
    compiler->scriptAst = ast;
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(
            compiler, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);
    TEST_ASSERT_FALSE(compiler_validate_ref_struct_rules(compiler, ast));
    TEST_ASSERT_TRUE(compiler->hasStructuredError);
    TEST_ASSERT_NOT_NULL(compiler->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler->errorMessage, expectedMessage));

    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void test_native_ref_like_capability_rejects_pool_ref_storage(void) {
    assert_native_pool_ref_storage_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "class Holder { var view: PoolRef<int>; }\n",
            "cannot be stored in a class field");
    assert_native_pool_ref_storage_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "var globalView: PoolRef<int>;\n",
            "cannot be stored in module/global storage");
    assert_native_pool_ref_storage_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "fn invalid(): void { var views: PoolRef<int>[1]; }\n",
            "cannot be an array element");
    assert_native_pool_ref_storage_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "class Box<T> {}\n"
            "fn invalid(): void { var boxed: Box<PoolRef<int>>; }\n",
            "cannot be used as an unconstrained generic argument");
    assert_native_pool_ref_storage_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "native extern(\"fixture\") {\n"
            "  fn consume(view: PoolRef<int>): void;\n"
            "}\n",
            "cannot cross a native opaque ABI boundary");
}

static void assert_native_pool_ref_escape_rejected(
        const TZrChar *source,
        const TZrChar *expectedMessage) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    compiler->suppressErrorOutput = ZR_TRUE;
    ast = parse_pooling_source(state, source);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT64(1u, ast->data.script.statements->count);
    compiler->scriptAst = ast;
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(
            compiler, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);
    TEST_ASSERT_TRUE(compiler_validate_ref_struct_rules(compiler, ast));
    TEST_ASSERT_FALSE(compiler_validate_reference_escapes(compiler, ast));
    TEST_ASSERT_TRUE(compiler->hasStructuredError);
    TEST_ASSERT_NOT_NULL(compiler->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler->errorMessage, expectedMessage));

    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void test_native_ref_like_capability_rejects_pool_ref_escape(void) {
    assert_native_pool_ref_escape_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "fn invalid(view: PoolRef<int>): int {\n"
            "  var views = [view];\n"
            "  return 0;\n"
            "}\n",
            "cannot be stored in an array");
    assert_native_pool_ref_escape_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "fn invalid(view: PoolRef<int>): int {\n"
            "  var read = fn(): int => view.value;\n"
            "  return 0;\n"
            "}\n",
            "cannot be captured by a closure");
    assert_native_pool_ref_escape_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "async fn invalid(view: PoolRef<int>): Task<int> {\n"
            "  var task = pause().start();\n"
            "  await task;\n"
            "  return view.value;\n"
            "}\n",
            "cannot cross an await suspension");
    assert_native_pool_ref_escape_rejected(
            "let {PoolRef} = import(\"zr.pooling\");\n"
            "fn invalid(view: PoolRef<int>): zr.iteration.Iterator<int> {\n"
            "  yield 1;\n"
            "  yield view.value;\n"
            "}\n",
            "cannot cross a yield suspension");
}

static void test_identity_recycle_and_retirement_preserve_guarded_value(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_MAPPED, 16u);
    SZrPoolConfig config = test_config(1u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPool *otherPool = ZR_NULL;
    SZrPoolHandle first;
    SZrPoolHandle second;
    SZrPoolGuard readGuard = {0};
    STestElement firstValue = {11, 0x5a5a};
    STestElement secondValue = {22, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &otherPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &firstValue, &first));
    TEST_ASSERT_EQUAL_UINT64(1u, first.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_WRONG_POOL, ZrPool_Validate(otherPool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, first, &readGuard));
    TEST_ASSERT_EQUAL_INT(
            11,
            ((const STestElement *)ZrPoolGuard_ReadOnlyValue(&readGuard))->value);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_ENTITY_RETIRED, ZrPool_Validate(pool, first));
    TEST_ASSERT_EQUAL_INT(
            11,
            ((const STestElement *)ZrPoolGuard_ReadOnlyValue(&readGuard))->value);
    {
        SZrPoolGuard rejected = {0};
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_ENTITY_RETIRED,
                ZrPool_TryRead(pool, first, &rejected));
        TEST_ASSERT_FALSE(rejected.active);
    }

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&readGuard));
    TEST_ASSERT_EQUAL_UINT64(1u, callbacks.dropCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &secondValue, &second));
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)first.slotIndex, (uint64_t)second.slotIndex);
    TEST_ASSERT_EQUAL_UINT64(first.generation + 1u, second.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Validate(pool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Recycle(pool, first));

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&otherPool));
    TEST_ASSERT_NULL(pool);
    TEST_ASSERT_NULL(otherPool);
    TEST_ASSERT_EQUAL_UINT64(2u, callbacks.dropCount);
}

static void test_reader_writer_conflicts_and_guard_release_are_deterministic(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_MAPPED, 8u);
    SZrPoolConfig config = test_config(4u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SZrPoolGuard firstRead = {0};
    SZrPoolGuard secondRead = {0};
    SZrPoolGuard writer = {0};
    STestElement value = {7, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &value, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handle, &firstRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handle, &secondRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_BORROW_CONFLICT,
            ZrPool_TryBorrow(pool, handle, &writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&firstRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&secondRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryBorrow(pool, handle, &writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_BORROW_CONFLICT,
            ZrPool_TryRead(pool, handle, &firstRead));
    ((STestElement *)ZrPoolGuard_Value(&writer))->value = 9;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_INVALID_ARGUMENT, ZrPoolGuard_Release(&writer));

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

static void test_pool_destroy_retires_live_slots_and_waits_for_active_guard(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_MAPPED, 8u);
    SZrPoolConfig config = test_config(2u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SZrPoolGuard guard = {0};
    STestElement value = {31, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &value, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryBorrow(pool, handle, &guard));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_POOL_BUSY, ZrPool_Destroy(&pool));
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_POOL_DESTROYED, ZrPool_Validate(pool, handle));
    TEST_ASSERT_EQUAL_INT(
            31, ((STestElement *)ZrPoolGuard_Value(&guard))->value);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&guard));
    TEST_ASSERT_EQUAL_UINT64(1u, callbacks.dropCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_NULL(pool);
}

static void test_construction_failure_and_generation_exhaustion_never_publish_aliases(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_FREE, 8u);
    SZrPoolConfig config = test_config(1u, 2u);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle first = {0};
    SZrPoolHandle second = {0};
    SZrPoolHandle third = {0};
    SZrPoolStats stats;
    STestElement rejected = {-1, 0x5a5a};
    STestElement accepted = {1, 0x5a5a};
    uint64_t scannedSlots = UINT64_MAX;
    uint64_t scannedBytes = UINT64_MAX;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_CONSTRUCTION_FAILED,
            ZrPool_Deliver(pool, &rejected, &first));
    TEST_ASSERT_EQUAL_UINT64(0u, first.poolId);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &accepted, &first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &accepted, &second));
    TEST_ASSERT_EQUAL_UINT64(2u, second.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, second));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &accepted, &third));
    TEST_ASSERT_NOT_EQUAL(first.slotIndex, third.slotIndex);
    TEST_ASSERT_EQUAL_UINT64(1u, third.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Validate(pool, second));

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)stats.exhaustedCount);
    TEST_ASSERT_EQUAL_UINT64(1u, stats.reuseCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(0u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(0u, scannedBytes);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

static void test_slabs_preserve_alignment_and_scan_only_initialized_slots(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_BARRIERED, 64u);
    SZrPoolConfig config = test_config(2u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handles[5];
    SZrPoolGuard guard = {0};
    SZrPoolStats stats;
    uint64_t scannedSlots = 0u;
    uint64_t scannedBytes = 0u;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    for (int index = 0; index < 5; index++) {
        STestElement value = {index, 0x5a5a};
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(pool, &value, &handles[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handles[4], &guard));
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            (uint64_t)(uintptr_t)ZrPoolGuard_ReadOnlyValue(&guard) % 64u);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&guard));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(5u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(5u * sizeof(STestElement), scannedBytes);
    TEST_ASSERT_EQUAL_UINT64(5u, callbacks.scanCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(3u, (uint64_t)stats.slabCount);
    TEST_ASSERT_EQUAL_UINT64(6u, (uint64_t)stats.slotCount);
    TEST_ASSERT_EQUAL_UINT64(5u, (uint64_t)stats.liveCount);
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)stats.freeCount);

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT64(5u, callbacks.dropCount);
}

static void test_million_handle_validation_and_rejection_stress(void) {
    const TZrSize handleCount = 1000000u;
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_FREE, 8u);
    SZrPoolConfig config = test_config(4096u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle *handles = (SZrPoolHandle *)malloc(
            sizeof(*handles) * handleCount);
    STestElement value = {42, 0x5a5a};
    SZrPoolStats stats;

    TEST_ASSERT_NOT_NULL(handles);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    for (TZrSize index = 0u; index < handleCount; index++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(pool, &value, &handles[index]));
    }
    for (TZrSize index = 0u; index < handleCount; index++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Validate(pool, handles[index]));
    }
    for (TZrSize index = 0u; index < handleCount; index += 2u) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Recycle(pool, handles[index]));
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_HANDLE_STALE,
                ZrPool_Validate(pool, handles[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)(handleCount / 2u), (uint64_t)stats.liveCount);
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)(stats.slotCount - stats.liveCount),
            (uint64_t)stats.freeCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT64(handleCount, callbacks.dropCount);
    free(handles);
}

static void run_concurrent_worker(SConcurrentWorker *worker) {
    for (TZrSize iteration = 0u;
         iteration < worker->iterationCount;
         iteration++) {
        int value = worker->workerId;
        SZrPoolHandle handle;
        SZrPoolGuard guard = {0};

        if (ZrPool_Deliver(worker->pool, &value, &handle) !=
            ZR_POOL_STATUS_OK) {
            worker->failureStep = 1;
            return;
        }
        if (ZrPool_TryBorrow(worker->pool, handle, &guard) !=
            ZR_POOL_STATUS_OK) {
            worker->failureStep = 2;
            return;
        }
        (*(int *)ZrPoolGuard_Value(&guard))++;
        if (ZrPoolGuard_Release(&guard) != ZR_POOL_STATUS_OK) {
            worker->failureStep = 3;
            return;
        }
        if (ZrPool_TryRead(worker->pool, handle, &guard) !=
            ZR_POOL_STATUS_OK) {
            worker->failureStep = 4;
            return;
        }
        if (*(const int *)ZrPoolGuard_ReadOnlyValue(&guard) !=
            worker->workerId + 1) {
            worker->failureStep = 5;
            return;
        }
        if (ZrPoolGuard_Release(&guard) != ZR_POOL_STATUS_OK) {
            worker->failureStep = 6;
            return;
        }
        if (ZrPool_Recycle(worker->pool, handle) != ZR_POOL_STATUS_OK) {
            worker->failureStep = 7;
            return;
        }
        if (ZrPool_Validate(worker->pool, handle) !=
            ZR_POOL_STATUS_HANDLE_STALE) {
            worker->failureStep = 8;
            return;
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI concurrent_worker_entry(LPVOID parameter) {
    run_concurrent_worker((SConcurrentWorker *)parameter);
    return 0u;
}
#else
static void *concurrent_worker_entry(void *parameter) {
    run_concurrent_worker((SConcurrentWorker *)parameter);
    return ZR_NULL;
}
#endif

static void test_concurrent_pool_serializes_state_without_charging_thread_local_mode(void) {
    enum {
        workerCount = 4,
        iterationsPerWorker = 25000
    };
    SZrPoolTypeLayout layout = {0};
    SZrPoolConfig config = test_config(64u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SConcurrentWorker workers[workerCount];
    SZrPoolStats stats;

    layout.elementSize = sizeof(int);
    layout.elementAlignment = sizeof(int);
    layout.gcScanKind = ZR_POOL_GC_SCAN_FREE;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_CONCURRENT;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    for (int index = 0; index < workerCount; index++) {
        workers[index].pool = pool;
        workers[index].workerId = index;
        workers[index].iterationCount = iterationsPerWorker;
        workers[index].failureStep = 0;
    }

#if defined(_WIN32)
    {
        HANDLE threads[workerCount];

        for (int index = 0; index < workerCount; index++) {
            threads[index] = CreateThread(
                    ZR_NULL,
                    0u,
                    concurrent_worker_entry,
                    &workers[index],
                    0u,
                    ZR_NULL);
            TEST_ASSERT_NOT_NULL(threads[index]);
        }
        TEST_ASSERT_EQUAL_UINT32(
                WAIT_OBJECT_0,
                WaitForMultipleObjects(workerCount, threads, TRUE, INFINITE));
        for (int index = 0; index < workerCount; index++) {
            CloseHandle(threads[index]);
        }
    }
#else
    {
        pthread_t threads[workerCount];

        for (int index = 0; index < workerCount; index++) {
            TEST_ASSERT_EQUAL_INT(
                    0,
                    pthread_create(
                            &threads[index],
                            ZR_NULL,
                            concurrent_worker_entry,
                            &workers[index]));
        }
        for (int index = 0; index < workerCount; index++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_join(threads[index], ZR_NULL));
        }
    }
#endif

    for (int index = 0; index < workerCount; index++) {
        TEST_ASSERT_EQUAL_INT(0, workers[index].failureStep);
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)workerCount * iterationsPerWorker,
            stats.deliverCount);
    TEST_ASSERT_EQUAL_UINT64(stats.deliverCount, stats.recycleCount);
    TEST_ASSERT_EQUAL_UINT64(stats.deliverCount, stats.dropCount);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)stats.liveCount);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)stats.retiredCount);
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)stats.slotCount, (uint64_t)stats.freeCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_native_descriptor_publishes_stable_slot_contract_by_role);
    RUN_TEST(test_native_pool_reflection_hides_guard_bypass_and_runtime_storage);
    RUN_TEST(test_native_pool_executes_identity_and_recycle_from_source);
    RUN_TEST(test_native_pool_executes_scoped_read_write_guards_from_source);
    RUN_TEST(test_native_semantic_import_preserves_ref_like_and_stable_slot_protocols);
    RUN_TEST(test_native_ref_like_capability_rejects_pool_ref_storage);
    RUN_TEST(test_native_ref_like_capability_rejects_pool_ref_escape);
    RUN_TEST(test_identity_recycle_and_retirement_preserve_guarded_value);
    RUN_TEST(test_reader_writer_conflicts_and_guard_release_are_deterministic);
    RUN_TEST(test_pool_destroy_retires_live_slots_and_waits_for_active_guard);
    RUN_TEST(test_construction_failure_and_generation_exhaustion_never_publish_aliases);
    RUN_TEST(test_slabs_preserve_alignment_and_scan_only_initialized_slots);
    RUN_TEST(test_million_handle_validation_and_rejection_stress);
    RUN_TEST(test_concurrent_pool_serializes_state_without_charging_thread_local_mode);
    return UNITY_END();
}
