#include "unity.h"

#include <string.h>

#include "container_test_common.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_library.h"
#include "zr_vm_parser.h"
#include "harness/runtime_support.h"

static const ZrLibMethodDescriptor *find_type_method(
        const TZrChar *typeName,
        TZrUInt32 role) {
    const ZrLibModuleDescriptor *module =
            ZrVmLibContainer_GetPoolingModuleDescriptor();

    for (TZrSize typeIndex = 0u;
         module != ZR_NULL && typeIndex < module->typeCount;
         typeIndex++) {
        const ZrLibTypeDescriptor *type = &module->types[typeIndex];

        if (strcmp(type->name, typeName) != 0) {
            continue;
        }
        for (TZrSize methodIndex = 0u;
             methodIndex < type->methodCount;
             methodIndex++) {
            if (type->methods[methodIndex].contractRole == role) {
                return &type->methods[methodIndex];
            }
        }
    }
    return ZR_NULL;
}

static TZrSize count_opcode_recursive(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    TZrSize count = 0u;

    if (function == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index]
                        .instruction.operationCode == opcode) {
            count++;
        }
    }
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; index++) {
        count += count_opcode_recursive(
                &function->childFunctionList[index], opcode);
    }
    return count;
}

static void test_production_pool_consumes_inline_closed_type_without_value_mirror(void) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrObjectModule *module;
    SZrObject *poolObject;
    SZrFunction *function;
    SZrFunctionFrameSlotLayout slotLayout = {0};
    SZrTypeLayout typeLayout;
    const SZrTypeLayout *layouts[1];
    const SZrTypeLayout *otherLayouts[1];
    SZrAotCodeRegistration registration = {0};
    SZrAotCodeRegistration otherRegistration = {0};
    ZrLibCallContext deliverContext = {0};
    ZrLibCallContext recycleContext = {0};
    SZrTypeValue poolValue;
    SZrTypeValue handleValue;
    SZrTypeValue recycleResult;
    SZrTypeValue rejectedHandle;
    TZrStackValuePointer frameBase;
    SZrStackFramePlace place;
    TZrInt64 payload[2] = {17, 29};
    const ZrLibMethodDescriptor *deliver;
    const ZrLibMethodDescriptor *recycle;

    TEST_ASSERT_NOT_NULL(state);
    module = ZrContainerTests_ImportNativeModule(state, "zr.pooling");
    TEST_ASSERT_NOT_NULL(module);
    poolObject = ZrLib_Type_NewInstance(state, "zr.pooling.Pool");
    if (poolObject == ZR_NULL) {
        poolObject = ZrLib_Type_NewInstance(state, "Pool");
    }
    TEST_ASSERT_NOT_NULL(poolObject);
    ZrLib_Value_SetObject(
            state, &poolValue, poolObject, ZR_VALUE_TYPE_OBJECT);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_TypeLayout_InitStruct(
            &typeLayout,
            (TZrUInt32)sizeof(payload),
            (TZrUInt32)_Alignof(TZrInt64),
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&typeLayout));
    layouts[0] = &typeLayout;
    registration.typeLayouts = layouts;
    registration.typeLayoutCount = ZR_ARRAY_COUNT(layouts);
    function->metadataCodeRegistration = &registration;
    function->metadataTypeLayoutCount = ZR_ARRAY_COUNT(layouts);

    slotLayout.stackSlot = 0u;
    slotLayout.byteSize = typeLayout.byteSize;
    slotLayout.byteAlign = typeLayout.byteAlign;
    slotLayout.typeLayoutId = 0u;
    slotLayout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    slotLayout.isParameter = ZR_TRUE;
    function->frameSlotLayouts = &slotLayout;
    function->frameSlotLayoutLength = 1u;
    function->frameByteSize = slotLayout.byteSize;
    function->frameByteAlign = slotLayout.byteAlign;
    frameBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state, function, frameBase, 0u, &place));
    memcpy(place.address, payload, sizeof(payload));

    deliverContext.state = state;
    deliverContext.argumentCount = 1u;
    deliverContext.selfValue = &poolValue;
    deliverContext.inlineFrameFunction = function;
    deliverContext.inlineFrameBase = frameBase;
    deliverContext.inlineArgumentStartSlot = 0u;
    TEST_ASSERT_NULL(ZrLib_CallContext_Argument(&deliverContext, 0u));
    deliver = find_type_method(
            "Pool", ZR_MEMBER_CONTRACT_ROLE_POOL_DELIVER);
    TEST_ASSERT_NOT_NULL(deliver);
    ZrLib_Value_SetNull(&handleValue);
    TEST_ASSERT_TRUE(deliver->callback(&deliverContext, &handleValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, handleValue.type);
    TEST_ASSERT_NULL(ZrLib_Object_GetFieldCString(
            state, poolObject, "__zr_pool_values"));
    otherLayouts[0] = &typeLayout;
    otherRegistration.typeLayouts = otherLayouts;
    otherRegistration.typeLayoutCount = ZR_ARRAY_COUNT(otherLayouts);
    function->metadataCodeRegistration = &otherRegistration;
    ZrLib_Value_SetNull(&rejectedHandle);
    TEST_ASSERT_FALSE(deliver->callback(
            &deliverContext, &rejectedHandle));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, rejectedHandle.type);
    function->metadataCodeRegistration = &registration;

    recycle = find_type_method(
            "Pool", ZR_MEMBER_CONTRACT_ROLE_POOL_RECYCLE);
    TEST_ASSERT_NOT_NULL(recycle);
    recycleContext.state = state;
    recycleContext.argumentCount = 1u;
    recycleContext.argumentValues = &handleValue;
    recycleContext.selfValue = &poolValue;
    ZrLib_Value_SetNull(&recycleResult);
    TEST_ASSERT_TRUE(recycle->callback(
            &recycleContext, &recycleResult));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, recycleResult.type);
    TEST_ASSERT_TRUE(recycleResult.value.nativeObject.nativeBool);

    ZrContainerTests_DestroyState(state);
}

static void test_source_pool_struct_write_guard_projects_and_commits_closed_type(void) {
    static const TZrChar *source =
            "struct Particle {\n"
            "  pub var x: int = 0;\n"
            "  pub var y: int = 0;\n"
            "}\n"
            "var {Pool, PoolRef, PoolReadRef} = import(\"zr.pooling\");\n"
            "fn run(): int {\n"
            "var pool = new Pool<Particle>();\n"
            "var particle: Particle = init Particle();\n"
            "particle.x = 17;\n"
            "particle.y = 29;\n"
            "var handle = pool.deliver(particle);\n"
            "{\n"
            "var writeView: PoolRef<Particle>;\n"
            "if (!pool.tryBorrow(handle, out writeView)) { return -1; }\n"
            "writeView.value.x = 41;\n"
            "writeView.value.y = 43;\n"
            "var snapshot: Particle = writeView.value;\n"
            "if (snapshot.x != 41 || snapshot.y != 43) { return -2; }\n"
            "snapshot.x = 0;\n"
            "snapshot = writeView.value;\n"
            "if (snapshot.x != 41 || snapshot.y != 43) { return -3; }\n"
            "}\n"
            "{\n"
            "var readView: PoolReadRef<Particle>;\n"
            "if (!pool.tryRead(handle, out readView)) { return -4; }\n"
            "if (readView.value.x != 41 || readView.value.y != 43) { return -5; }\n"
            "}\n"
            "if (!pool.recycle(handle)) { return -6; }\n"
            "return 84;\n"
            "}\n"
            "return run();\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "pooling_closed_type_guard.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(CLOSE_SCOPE)));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(84, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_source_pool_guards_close_on_abrupt_exit_and_replacement(void) {
    static const TZrChar *source =
            "struct Particle {\n"
            "  pub var x: int = 0;\n"
            "}\n"
            "var {Pool, PoolRef} = import(\"zr.pooling\");\n"
            "var pool = new Pool<Particle>();\n"
            "var particle: Particle = init Particle();\n"
            "var handle = pool.deliver(particle);\n"
            "fn borrowAndReturn(): int {\n"
            "  var view: PoolRef<Particle>;\n"
            "  if (!pool.tryBorrow(handle, out view)) { return -1; }\n"
            "  return view.value.x;\n"
            "}\n"
            "fn borrowAndThrow(): int {\n"
            "  var view: PoolRef<Particle>;\n"
            "  if (!pool.tryBorrow(handle, out view)) { return -2; }\n"
            "  throw 7;\n"
            "}\n"
            "fn run(): int {\n"
            "  if (borrowAndReturn() != 0) { return -3; }\n"
            "  var check: PoolRef<Particle>;\n"
            "  if (!pool.tryBorrow(handle, out check)) { return -4; }\n"
            "  check.close();\n"
            "  try { borrowAndThrow(); } catch (error) {}\n"
            "  if (!pool.tryBorrow(handle, out check)) { return -5; }\n"
            "  check.close();\n"
            "  var breakCount = 0;\n"
            "  while (true) {\n"
            "    var breakView: PoolRef<Particle>;\n"
            "    if (!pool.tryBorrow(handle, out breakView)) { return -6; }\n"
            "    breakCount = breakCount + 1;\n"
            "    break;\n"
            "  }\n"
            "  if (breakCount != 1 || !pool.tryBorrow(handle, out check)) { return -7; }\n"
            "  check.close();\n"
            "  var continueCount = 0;\n"
            "  while (continueCount < 1) {\n"
            "    var continueView: PoolRef<Particle>;\n"
            "    if (!pool.tryBorrow(handle, out continueView)) { return -8; }\n"
            "    continueCount = continueCount + 1;\n"
            "    continue;\n"
            "  }\n"
            "  if (!pool.tryBorrow(handle, out check)) { return -9; }\n"
            "  check.close();\n"
            "  var other = pool.deliver(particle);\n"
            "  var replacement: PoolRef<Particle>;\n"
            "  if (!pool.tryBorrow(handle, out replacement)) { return -10; }\n"
            "  if (!pool.tryBorrow(other, out replacement)) { return -11; }\n"
            "  if (!pool.recycle(handle)) { return -12; }\n"
            "  replacement.close();\n"
            "  if (!pool.recycle(other)) { return -13; }\n"
            "  return 109;\n"
            "}\n"
            "return run();\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "pooling_guard_cleanup.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(109, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static TZrBool find_inline_struct_parameter(
        SZrFunction *function,
        SZrFunction **outFunction,
        const SZrFunctionFrameSlotLayout **outSlot) {
    if (function == ZR_NULL || outFunction == ZR_NULL ||
        outSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u;
         index < function->frameSlotLayoutLength;
         index++) {
        const SZrFunctionFrameSlotLayout *slot =
                &function->frameSlotLayouts[index];

        if (slot->slotKind == ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
            slot->isParameter &&
            slot->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
            *outFunction = function;
            *outSlot = slot;
            return ZR_TRUE;
        }
    }
    for (TZrUInt32 index = 0u;
         index < function->childFunctionLength;
         index++) {
        if (find_inline_struct_parameter(
                    &function->childFunctionList[index],
                    outFunction,
                    outSlot)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void set_int_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        TZrInt64 number) {
    SZrTypeValue value;

    ZrLib_Value_SetInt(state, &value, number);
    ZrLib_Object_SetFieldCString(state, object, name, &value);
}

static TZrInt64 read_int_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name) {
    const SZrTypeValue *value = ZrLib_Object_GetFieldCString(
            state, object, name);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    return value->value.nativeObject.nativeInt64;
}

static void test_native_struct_write_guard_copies_projection_back_to_inline_slot(void) {
    static const TZrChar *source =
            "struct Particle {\n"
            "  pub var x: int = 0;\n"
            "  pub var y: int = 0;\n"
            "}\n"
            "fn inspect(value: Particle): int { return value.x; }\n"
            "return 0;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrObjectModule *module;
    SZrString *sourceName;
    SZrFunction *function;
    SZrFunction *layoutFunction = ZR_NULL;
    const SZrFunctionFrameSlotLayout *slot;
    SZrAotCodeRegistration registration = {0};
    const SZrTypeLayout *registeredLayouts[16] = {0};
    const SZrTypeLayout *prototypeLayout;
    SZrObject *poolObject;
    SZrObject *particleObject;
    SZrTypeValue poolValue;
    SZrTypeValue particleValue;
    SZrTypeValue handleValue;
    SZrTypeValue callbackResult;
    SZrTypeValue borrowArguments[2];
    SZrTypeValue readArguments[2];
    ZrLibCallContext deliverContext = {0};
    ZrLibCallContext borrowContext = {0};
    ZrLibCallContext readContext = {0};
    ZrLibCallContext closeContext = {0};
    TZrStackValuePointer inlineFrameBase;
    TZrStackValuePointer borrowFrameBase;
    TZrStackValuePointer readFrameBase;
    SZrStackFramePlace place;
    SZrObject *writeView;
    SZrObject *readView;
    SZrObject *projectedValue;
    const SZrTypeValue *projected;
    ZrLibInlineArgumentView inlineView;
    ZrLibInlineSpan inlineSpan;
    SZrTypeLayoutRegistryView registry;
    const SZrTypeLayout *resolvedLayout;
    const ZrLibMethodDescriptor *deliver;
    const ZrLibMethodDescriptor *tryBorrow;
    const ZrLibMethodDescriptor *tryRead;
    const ZrLibMethodDescriptor *closeWrite;
    const ZrLibMethodDescriptor *closeRead;
    TZrInt64 executionResult = 0;

    TEST_ASSERT_NOT_NULL(state);
    module = ZrContainerTests_ImportNativeModule(state, "zr.pooling");
    TEST_ASSERT_NOT_NULL(module);
    sourceName = ZrCore_String_CreateFromNative(
            state, "pooling_native_struct_writeback.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &executionResult));
    TEST_ASSERT_EQUAL_INT64(0, executionResult);
    TEST_ASSERT_TRUE(ZrCore_Stack_CheckFullAndGrow(
            state,
            128u,
            "pooling closed-type runtime test frame"));
    TEST_ASSERT_TRUE(find_inline_struct_parameter(
            function, &layoutFunction, &slot));
    TEST_ASSERT_TRUE(slot->typeLayoutId < ZR_ARRAY_COUNT(registeredLayouts));
    prototypeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
            layoutFunction, slot->typeLayoutId, state);
    TEST_ASSERT_NOT_NULL(prototypeLayout);
    registeredLayouts[slot->typeLayoutId] = prototypeLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = slot->typeLayoutId + 1u;
    layoutFunction->metadataCodeRegistration = &registration;
    layoutFunction->metadataTypeLayoutCount = registration.typeLayoutCount;

    poolObject = ZrLib_Type_NewInstance(state, "zr.pooling.Pool");
    if (poolObject == ZR_NULL) {
        poolObject = ZrLib_Type_NewInstance(state, "Pool");
    }
    TEST_ASSERT_NOT_NULL(poolObject);
    ZrLib_Value_SetObject(
            state, &poolValue, poolObject, ZR_VALUE_TYPE_OBJECT);

    inlineFrameBase = state->stackBase.valuePointer + 8u;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            layoutFunction,
            inlineFrameBase,
            slot->stackSlot,
            &place));
    TEST_ASSERT_TRUE(ZrCore_Function_InitInlineStorage(
            state,
            layoutFunction,
            slot->typeLayoutId,
            place.address,
            slot->byteSize));
    TEST_ASSERT_TRUE(ZrCore_Function_CopyInlineStorageToObjectValue(
            state,
            layoutFunction,
            slot->typeLayoutId,
            place.address,
            slot->byteSize,
            &particleValue));
    particleObject = ZR_CAST_OBJECT(state, particleValue.value.object);
    set_int_field(state, particleObject, "x", 17);
    set_int_field(state, particleObject, "y", 29);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyObjectValueToInlineStorage(
            state,
            layoutFunction,
            slot->typeLayoutId,
            place.address,
            slot->byteSize,
            &particleValue));
    deliver = find_type_method(
            "Pool", ZR_MEMBER_CONTRACT_ROLE_POOL_DELIVER);
    TEST_ASSERT_NOT_NULL(deliver);
    deliverContext.state = state;
    deliverContext.argumentCount = 1u;
    deliverContext.selfValue = &poolValue;
    deliverContext.inlineFrameFunction = layoutFunction;
    deliverContext.inlineFrameBase = inlineFrameBase;
    deliverContext.inlineArgumentStartSlot = slot->stackSlot;
    TEST_ASSERT_TRUE(ZrLib_CallContext_InlineArgumentSpan(
            &deliverContext, 0u, &inlineSpan));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(
            layoutFunction, &registry));
    TEST_ASSERT_TRUE(inlineSpan.typeLayoutId < registry.count);
    resolvedLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(
            layoutFunction, inlineSpan.typeLayoutId);
    TEST_ASSERT_NOT_NULL(resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(inlineSpan.byteSize, resolvedLayout->byteSize);
    TEST_ASSERT_EQUAL_UINT32(inlineSpan.byteAlign, resolvedLayout->byteAlign);
    TEST_ASSERT_TRUE(ZrLib_CallContext_InlineArgumentView(
            &deliverContext, 0u, &inlineView));
    ZrLib_Value_SetNull(&handleValue);
    TEST_ASSERT_TRUE(deliver->callback(&deliverContext, &handleValue));

    tryBorrow = find_type_method(
            "Pool", ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE);
    closeWrite = find_type_method(
            "PoolRef", ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE);
    TEST_ASSERT_NOT_NULL(tryBorrow);
    TEST_ASSERT_NOT_NULL(closeWrite);
    borrowArguments[0] = handleValue;
    ZrLib_Value_SetNull(&borrowArguments[1]);
    borrowFrameBase = state->stackBase.valuePointer + 64u;
    ZrCore_Stack_CopyValue(state, borrowFrameBase + 1u, &handleValue);
    ZrLib_Value_SetNull(ZrCore_Stack_GetValueNoProfile(
            borrowFrameBase + 2u));
    borrowContext.state = state;
    borrowContext.functionBase = borrowFrameBase;
    borrowContext.argumentValues = borrowArguments;
    borrowContext.argumentCount = ZR_ARRAY_COUNT(borrowArguments);
    borrowContext.selfValue = &poolValue;
    ZrLib_Value_SetNull(&callbackResult);
    TEST_ASSERT_TRUE(tryBorrow->callback(
            &borrowContext, &callbackResult));
    TEST_ASSERT_TRUE(callbackResult.value.nativeObject.nativeBool);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, borrowArguments[1].type);
    TEST_ASSERT_NOT_NULL(ZrCore_Value_GetMeta(
            state, &borrowArguments[1], ZR_META_CLOSE));
    writeView = ZR_CAST_OBJECT(state, borrowArguments[1].value.object);
    projected = ZrLib_Object_GetFieldCString(
            state, writeView, "__zr_pool_guard_value");
    TEST_ASSERT_NOT_NULL(projected);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, projected->type);
    projectedValue = ZR_CAST_OBJECT(state, projected->value.object);
    set_int_field(state, projectedValue, "x", 41);
    set_int_field(state, projectedValue, "y", 43);
    closeContext.state = state;
    closeContext.selfValue = &borrowArguments[1];
    TEST_ASSERT_TRUE(closeWrite->callback(
            &closeContext, &callbackResult));

    tryRead = find_type_method(
            "Pool", ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ);
    closeRead = find_type_method(
            "PoolReadRef", ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE);
    TEST_ASSERT_NOT_NULL(tryRead);
    TEST_ASSERT_NOT_NULL(closeRead);
    readArguments[0] = handleValue;
    ZrLib_Value_SetNull(&readArguments[1]);
    readFrameBase = state->stackBase.valuePointer + 96u;
    ZrCore_Stack_CopyValue(state, readFrameBase + 1u, &handleValue);
    ZrLib_Value_SetNull(ZrCore_Stack_GetValueNoProfile(
            readFrameBase + 2u));
    readContext.state = state;
    readContext.functionBase = readFrameBase;
    readContext.argumentValues = readArguments;
    readContext.argumentCount = ZR_ARRAY_COUNT(readArguments);
    readContext.selfValue = &poolValue;
    TEST_ASSERT_TRUE(tryRead->callback(&readContext, &callbackResult));
    TEST_ASSERT_TRUE(callbackResult.value.nativeObject.nativeBool);
    readView = ZR_CAST_OBJECT(state, readArguments[1].value.object);
    projected = ZrLib_Object_GetFieldCString(
            state, readView, "__zr_pool_guard_value");
    TEST_ASSERT_NOT_NULL(projected);
    projectedValue = ZR_CAST_OBJECT(state, projected->value.object);
    TEST_ASSERT_EQUAL_INT64(41, read_int_field(
            state, projectedValue, "x"));
    TEST_ASSERT_EQUAL_INT64(43, read_int_field(
            state, projectedValue, "y"));
    closeContext.selfValue = &readArguments[1];
    TEST_ASSERT_TRUE(closeRead->callback(
            &closeContext, &callbackResult));

    ZrContainerTests_DestroyState(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_production_pool_consumes_inline_closed_type_without_value_mirror);
    RUN_TEST(test_source_pool_struct_write_guard_projects_and_commits_closed_type);
    RUN_TEST(test_source_pool_guards_close_on_abrupt_exit_and_replacement);
    RUN_TEST(test_native_struct_write_guard_copies_projection_back_to_inline_slot);
    return UNITY_END();
}
