//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "module_fixture_support.h"
#include "runtime_support.h"
#include "zr_vm_library.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"
#include "test_support.h"

TZrBool ZrVmLibFfi_Register(SZrGlobalState *global);

// Unity 测试宏扩展 - 添加缺失的 UINT64 不等断言
#ifndef TEST_ASSERT_NOT_EQUAL_UINT64
#define TEST_ASSERT_NOT_EQUAL_UINT64(expected, actual)                                             TEST_ASSERT_NOT_EQUAL((expected), (actual))
#endif

#ifndef TEST_ASSERT_NOT_EQUAL_INT64
#define TEST_ASSERT_NOT_EQUAL_INT64(expected, actual)                                              TEST_ASSERT_NOT_EQUAL((expected), (actual))
#endif

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const TZrChar *kProbeCounterStateField = "__probe_counter_state";

static SZrFunction *debug_resolve_function_from_value(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION) {
        return ZR_CAST_FUNCTION(state, value->value.object);
    }

    if (value->type == ZR_VALUE_TYPE_CLOSURE && !value->isNative) {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_to_object_type_name(const SZrFunction *function, const TZrChar *typeName) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        TZrUInt16 constantIndex;
        const SZrTypeValue *constantValue;

        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_OBJECT)) {
            continue;
        }

        constantIndex = instruction->instruction.operand.operand1[1];
        if (constantIndex >= function->constantValueLength) {
            continue;
        }

        constantValue = &function->constantValueList[constantIndex];
        if (constantValue->type == ZR_VALUE_TYPE_STRING &&
            strcmp(ZrCore_String_GetNativeString((SZrString *)constantValue->value.object), typeName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_get_member_name(const SZrFunction *function, const TZrChar *memberName) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        TZrUInt16 memberEntryIndex;
        SZrString *symbol;

        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_MEMBER)) {
            continue;
        }

        memberEntryIndex = instruction->instruction.operand.operand1[1];
        if (memberEntryIndex >= function->memberEntryLength) {
            continue;
        }

        symbol = function->memberEntries[memberEntryIndex].symbol;
        if (symbol != ZR_NULL && strcmp(ZrCore_String_GetNativeString(symbol), memberName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void probe_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void probe_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetNull(&fieldValue);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static TZrBool probe_get_self_object(const ZrLibCallContext *context, SZrObject **outObject) {
    SZrTypeValue *selfValue;

    if (outObject != ZR_NULL) {
        *outObject = ZR_NULL;
    }

    if (context == ZR_NULL || outObject == ZR_NULL) {
        return ZR_FALSE;
    }

    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outObject = ZR_CAST_OBJECT(context->state, selfValue->value.object);
    return *outObject != ZR_NULL;
}

static TZrBool probe_get_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 *outValue) {
    const SZrTypeValue *fieldValue;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldValue = ZrLib_Object_GetFieldCString(state, object, fieldName);
    if (fieldValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(fieldValue->type)) {
        return ZR_FALSE;
    }

    *outValue = fieldValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool probe_native_create_counter_iterable(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLibTempValueRoot root;
    SZrObject *iterable;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
        return ZR_FALSE;
    }

    iterable = ZrLib_Type_NewInstance(context->state, "NativeCounterIterable");
    if (iterable == ZR_NULL || !ZrLib_TempValueRoot_SetObject(&root, iterable, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }

    probe_set_int_field(context->state, iterable, kProbeCounterStateField, 0);
    ZrCore_Value_Copy(context->state, result, ZrLib_TempValueRoot_Value(&root));
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}

static TZrBool probe_native_counter_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLibTempValueRoot root;
    SZrObject *iterator;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
        return ZR_FALSE;
    }

    iterator = ZrLib_Type_NewInstance(context->state, "NativeCounterIterator");
    if (iterator == ZR_NULL || !ZrLib_TempValueRoot_SetObject(&root, iterator, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }

    probe_set_int_field(context->state, iterator, kProbeCounterStateField, -1);
    probe_set_null_field(context->state, iterator, "current");
    ZrCore_Value_Copy(context->state, result, ZrLib_TempValueRoot_Value(&root));
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}

static TZrBool probe_native_counter_iterator_move_next(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *iterator = ZR_NULL;
    TZrInt64 index = -1;

    if (context == ZR_NULL || result == ZR_NULL || !probe_get_self_object(context, &iterator)) {
        return ZR_FALSE;
    }

    probe_get_int_field(context->state, iterator, kProbeCounterStateField, &index);
    index += 1;
    probe_set_int_field(context->state, iterator, kProbeCounterStateField, index);

    switch (index) {
        case 0:
            probe_set_int_field(context->state, iterator, "current", 4);
            ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
            return ZR_TRUE;
        case 1:
            probe_set_int_field(context->state, iterator, "current", 6);
            ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
            return ZR_TRUE;
        case 2:
            probe_set_int_field(context->state, iterator, "current", 9);
            ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
            return ZR_TRUE;
        default:
            probe_set_null_field(context->state, iterator, "current");
            ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
            return ZR_TRUE;
    }
}

static const ZrLibMethodDescriptor kProbeReadableMethods[] = {
        {
                .name = "read",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = ZR_NULL,
                .returnTypeName = "int",
                .documentation = "Read the current value.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .contractRole = 0,
        },
};

static const ZrLibMethodDescriptor kProbeStreamReadableMethods[] = {
        {
                .name = "available",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = ZR_NULL,
                .returnTypeName = "int",
                .documentation = "Return the available item count.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .contractRole = 0,
        },
};

static const ZrLibParameterDescriptor kProbeConfigureParameters[] = {
        {"mode", "NativeMode", "The mode to apply."},
};

static const ZrLibMethodDescriptor kProbeDeviceMethods[] = {
        {
                .name = "configure",
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = ZR_NULL,
                .returnTypeName = "NativeMode",
                .documentation = "Apply a new device mode.",
                .isStatic = ZR_FALSE,
                .parameters = kProbeConfigureParameters,
                .parameterCount = ZR_ARRAY_COUNT(kProbeConfigureParameters),
                .contractRole = 0,
        },
};

static const ZrLibParameterDescriptor kProbeLookupParameters[] = {
        {"key", "K", "The lookup key."},
};

static const ZrLibMethodDescriptor kProbeLookupMethods[] = {
        {
                .name = "lookup",
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = ZR_NULL,
                .returnTypeName = "V",
                .documentation = "Resolve a value from the lookup table.",
                .isStatic = ZR_FALSE,
                .parameters = kProbeLookupParameters,
                .parameterCount = ZR_ARRAY_COUNT(kProbeLookupParameters),
                .contractRole = 0,
        },
};

static const ZrLibParameterDescriptor kProbeCreateDeviceParameters[] = {
        {"mode", "NativeMode", "The initial device mode."},
};

static const ZrLibFunctionDescriptor kProbeNativeFunctions[] = {
        {
                .name = "createDevice",
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = ZR_NULL,
                .returnTypeName = "NativeDevice",
                .documentation = "Create a native device.",
                .parameters = kProbeCreateDeviceParameters,
                .parameterCount = ZR_ARRAY_COUNT(kProbeCreateDeviceParameters),
        },
        {
                .name = "createCounterIterable",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = probe_native_create_counter_iterable,
                .returnTypeName = "NativeCounterIterable",
                .documentation = "Create a native iterable value.",
                .parameters = ZR_NULL,
                .parameterCount = 0,
        },
};

static const TZrChar *kProbeDeviceImplements[] = {
        "NativeStreamReadable",
};

static const TZrChar *kProbeCounterIterableImplements[] = {
        "zr.builtin.IEnumerable<int>",
};

static const TZrChar *kProbeCounterIteratorImplements[] = {
        "zr.builtin.IEnumerator<int>",
};

static const ZrLibFieldDescriptor kProbeDeviceFields[] = {
        {"mode", "NativeMode", "The current device mode.", 0},
};

static const ZrLibEnumMemberDescriptor kProbeModeMembers[] = {
        {"Off", ZR_LIB_CONSTANT_KIND_INT, 0, 0.0, ZR_NULL, ZR_FALSE, "Disabled mode."},
        {"On", ZR_LIB_CONSTANT_KIND_INT, 1, 0.0, ZR_NULL, ZR_FALSE, "Enabled mode."},
};

static const ZrLibFieldDescriptor kProbeBoxFields[] = {
        {"value", "T", "The boxed value.", 0},
};

static const ZrLibFieldDescriptor kProbeCounterIteratorFields[] = {
        {"current", "int", "The current counter value.", ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD},
};

static const ZrLibMethodDescriptor kProbeBoxMethods[] = {
        {
                .name = "getValue",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = ZR_NULL,
                .returnTypeName = "T",
                .documentation = "Return the boxed value.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .contractRole = 0,
        },
};

static const ZrLibMethodDescriptor kProbeCounterIterableMethods[] = {
        {
                .name = "getIterator",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = probe_native_counter_get_iterator,
                .returnTypeName = "NativeCounterIterator",
                .documentation = "Create a contract-driven counter iterator.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .contractRole = ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT,
        },
};

static const ZrLibMethodDescriptor kProbeCounterIteratorMethods[] = {
        {
                .name = "moveNext",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = probe_native_counter_iterator_move_next,
                .returnTypeName = "bool",
                .documentation = "Advance the contract-driven counter iterator.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .contractRole = ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT,
        },
};

static const ZrLibGenericParameterDescriptor kProbeBoxGenericParameters[] = {
        {
                .name = "T",
                .documentation = "The boxed element type.",
        },
};

static const TZrChar *kProbeLookupKeyConstraints[] = {
        "NativeReadable",
        "NativeStreamReadable",
};

static const ZrLibGenericParameterDescriptor kProbeLookupGenericParameters[] = {
        {
                .name = "K",
                .documentation = "The lookup key type.",
                .constraintTypeNames = kProbeLookupKeyConstraints,
                .constraintTypeCount = ZR_ARRAY_COUNT(kProbeLookupKeyConstraints),
        },
        {
                .name = "V",
                .documentation = "The lookup value type.",
        },
};

static const ZrLibTypeDescriptor kProbeNativeTypes[] = {
        {
                .name = "NativeReadable",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                .methods = kProbeReadableMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeReadableMethods),
                .documentation = "Readable interface.",
                .allowValueConstruction = ZR_FALSE,
                .allowBoxedConstruction = ZR_FALSE,
                .constructorSignature = "NativeReadable()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .protocolMask = 0,
        },
        {
                .name = "NativeStreamReadable",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                .methods = kProbeStreamReadableMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeStreamReadableMethods),
                .documentation = "Stream-readable interface.",
                .extendsTypeName = "NativeReadable",
                .allowValueConstruction = ZR_FALSE,
                .allowBoxedConstruction = ZR_FALSE,
                .constructorSignature = "NativeStreamReadable()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .protocolMask = 0,
        },
        {
                .name = "NativeMode",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM,
                .documentation = "Device mode enum.",
                .enumMembers = kProbeModeMembers,
                .enumMemberCount = ZR_ARRAY_COUNT(kProbeModeMembers),
                .enumValueTypeName = "int",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeMode(value: int)",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .protocolMask = 0,
        },
        {
                .name = "NativeDevice",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .fields = kProbeDeviceFields,
                .fieldCount = ZR_ARRAY_COUNT(kProbeDeviceFields),
                .methods = kProbeDeviceMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeDeviceMethods),
                .documentation = "Concrete device type.",
                .implementsTypeNames = kProbeDeviceImplements,
                .implementsTypeCount = ZR_ARRAY_COUNT(kProbeDeviceImplements),
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeDevice()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .protocolMask = 0,
        },
        {
                .name = "NativeCounterIterable",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .methods = kProbeCounterIterableMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeCounterIterableMethods),
                .documentation = "Native iterable type backed by explicit iterable contracts.",
                .implementsTypeNames = kProbeCounterIterableImplements,
                .implementsTypeCount = ZR_ARRAY_COUNT(kProbeCounterIterableImplements),
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeCounterIterable()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE),
        },
        {
                .name = "NativeCounterIterator",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .fields = kProbeCounterIteratorFields,
                .fieldCount = ZR_ARRAY_COUNT(kProbeCounterIteratorFields),
                .methods = kProbeCounterIteratorMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeCounterIteratorMethods),
                .documentation = "Native iterator type backed by explicit iterator contracts.",
                .implementsTypeNames = kProbeCounterIteratorImplements,
                .implementsTypeCount = ZR_ARRAY_COUNT(kProbeCounterIteratorImplements),
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeCounterIterator()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR),
        },
        {
                .name = "NativeBox",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .fields = kProbeBoxFields,
                .fieldCount = ZR_ARRAY_COUNT(kProbeBoxFields),
                .methods = kProbeBoxMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeBoxMethods),
                .documentation = "Open generic native box type.",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeBox(value: T)",
                .genericParameters = kProbeBoxGenericParameters,
                .genericParameterCount = ZR_ARRAY_COUNT(kProbeBoxGenericParameters),
                .protocolMask = 0,
        },
        {
                .name = "NativeLookup",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .methods = kProbeLookupMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeLookupMethods),
                .documentation = "Open generic native lookup type.",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeLookup()",
                .genericParameters = kProbeLookupGenericParameters,
                .genericParameterCount = ZR_ARRAY_COUNT(kProbeLookupGenericParameters),
                .protocolMask = 0,
        },
};

static const ZrLibModuleDescriptor kProbeNativeModuleDescriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "probe.native_shapes",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = kProbeNativeFunctions,
        .functionCount = ZR_ARRAY_COUNT(kProbeNativeFunctions),
        .types = kProbeNativeTypes,
        .typeCount = ZR_ARRAY_COUNT(kProbeNativeTypes),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Native test module containing interface, enum and implements metadata.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
};

static const ZrLibModuleDescriptor kProbeFutureAbiModuleDescriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "probe.future_abi",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        "Native module that requires a future runtime ABI.",
        ZR_NULL,
        0,
        "9.9.9",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION + 1,
        0,
        ZR_NULL,
};

static const ZrLibModuleDescriptor kProbeUnsupportedCapabilityModuleDescriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "probe.unsupported_capability",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        "Native module that requires unsupported runtime capabilities.",
        ZR_NULL,
        0,
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        ((TZrUInt64)1u << 40),
        ZR_NULL,
};

typedef struct ZrTestAllocatorFailureConfig {
    TZrBool armed;
    TZrBool fired;
    EZrMemoryNativeType failType;
} ZrTestAllocatorFailureConfig;

// 测试日志宏（符合测试规范）
#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double) (clock() - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                             \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

// 简单的测试分配器
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZrTestAllocatorFailureConfig *failureConfig = (ZrTestAllocatorFailureConfig *)userData;
    ZR_UNUSED_PARAMETER(originalSize);

    if (pointer == ZR_NULL && newSize > 0 && failureConfig != ZR_NULL && failureConfig->armed && !failureConfig->fired &&
        (EZrMemoryNativeType)flag == failureConfig->failType) {
        failureConfig->fired = ZR_TRUE;
        return ZR_NULL;
    }

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
            return realloc(pointer, newSize);
        } else {
            return malloc(newSize);
        }
    }
}

static SZrState *create_test_state_with_allocator_context(TZrPtr allocatorUserData) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, allocatorUserData, 12345, &callbacks);
    if (!global)
        return ZR_NULL;

    SZrState *mainState = global->mainThreadState;
    if (mainState) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        // 注册 parser 模块
        ZrParser_ToGlobalState_Register(mainState);
        ZrVmLibMath_Register(global);
        ZrVmLibContainer_Register(global);
        ZrVmLibSystem_Register(global);
    }

    return mainState;
}

// 创建测试用的SZrState
static SZrState *create_test_state(void) {
    return create_test_state_with_allocator_context(ZR_NULL);
}

static TZrBool register_probe_native_module(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(state->global, &kProbeNativeModuleDescriptor);
}

// 销毁测试用的SZrState
static void destroy_test_state(SZrState *state) {
    if (!state)
        return;

    SZrGlobalState *global = state->global;
    if (global) {
        ZrCore_GlobalState_Free(global);
    }
}

#define string_equals_cstring ZrTests_Fixture_StringEqualsCString
#define get_object_field_value ZrTests_Fixture_GetObjectFieldValue
#define get_array_length ZrTests_Fixture_GetArrayLength
#define get_array_entry_object ZrTests_Fixture_GetArrayEntryObject
#define get_array_entry_value ZrTests_Fixture_GetArrayEntryValue

static const SZrTypeValue *get_module_export_value(SZrState *state, SZrObjectModule *module, const TZrChar *exportName) {
    SZrString *exportNameString;

    if (state == ZR_NULL || module == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    exportNameString = ZrCore_String_Create(state, (TZrNativeString)exportName, strlen(exportName));
    if (exportNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportNameString);
}

static const TZrChar *get_current_exception_message_cstring(SZrState *state) {
    SZrObject *errorObject;
    const SZrTypeValue *messageValue;

    if (state == ZR_NULL || !state->hasCurrentException || state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    errorObject = ZR_CAST_OBJECT(state, state->currentException.value.object);
    if (errorObject == ZR_NULL) {
        return ZR_NULL;
    }

    messageValue = get_object_field_value(state, errorObject, "message");
    if (messageValue == ZR_NULL || messageValue->type != ZR_VALUE_TYPE_STRING || messageValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object));
}

static const SZrTypeValue *get_zr_global_value(SZrState *state, const TZrChar *memberName) {
    SZrGlobalState *global;
    SZrObject *zrObject;

    if (state == ZR_NULL || state->global == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    if (global->zrObject.type != ZR_VALUE_TYPE_OBJECT || global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, global->zrObject.value.object);
    return get_object_field_value(state, zrObject, memberName);
}

static SZrObjectModule *import_native_module(SZrState *state, const TZrChar *moduleName) {
    SZrString *modulePath;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    modulePath = ZrCore_String_Create(state, (TZrNativeString)moduleName, strlen(moduleName));
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_ImportByPath(state, modulePath);
}

typedef ZrTestsFixtureSource SZrModuleFixtureSource;
typedef ZrTestsFixtureReader SZrModuleFixtureReader;

#define MODULE_FIXTURE_SOURCE_TEXT(pathValue, sourceValue) ZR_TESTS_FIXTURE_SOURCE_TEXT(pathValue, sourceValue)
#define read_test_file_bytes ZrTests_Fixture_ReadFileBytes
#define module_fixture_reader_read ZrTests_Fixture_ReaderRead

static const SZrModuleFixtureSource *g_module_fixture_sources = ZR_NULL;
static TZrSize g_module_fixture_source_count = 0;

static TZrByte *build_module_binary_fixture(SZrState *state,
                                            const TZrChar *moduleSource,
                                            const TZrChar *binaryPath,
                                            TZrSize *outLength) {
    return ZrTests_Fixture_BuildBinaryFile(state, moduleSource, binaryPath, ZR_FALSE, outLength);
}

static TZrBool module_fixture_source_loader(SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(state,
                                                 sourcePath,
                                                 md5,
                                                 io,
                                                 g_module_fixture_sources,
                                                 g_module_fixture_source_count);
}

static SZrObject *find_named_entry_in_array(SZrState *state,
                                            SZrObject *array,
                                            const TZrChar *fieldName,
                                            const TZrChar *expectedValue) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || fieldName == ZR_NULL || expectedValue == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < get_array_length(array); index++) {
        SZrObject *entry = get_array_entry_object(state, array, index);
        const SZrTypeValue *fieldValue = get_object_field_value(state, entry, fieldName);
        if (fieldValue != ZR_NULL &&
            fieldValue->type == ZR_VALUE_TYPE_STRING &&
            string_equals_cstring(ZR_CAST_STRING(state, fieldValue->value.object), expectedValue)) {
            return entry;
        }
    }

    return ZR_NULL;
}

static const SZrTypeValue *find_string_entry_in_array(SZrState *state, SZrObject *array, const TZrChar *expectedValue) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || expectedValue == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < get_array_length(array); index++) {
        const SZrTypeValue *entryValue = get_array_entry_value(state, array, index);
        if (entryValue != ZR_NULL &&
            entryValue->type == ZR_VALUE_TYPE_STRING &&
            string_equals_cstring(ZR_CAST_STRING(state, entryValue->value.object), expectedValue)) {
            return entryValue;
        }
    }

    return ZR_NULL;
}

static SZrObjectPrototype *get_module_exported_prototype(SZrState *state,
                                                         SZrObjectModule *module,
                                                         const TZrChar *typeName) {
    SZrString *name;
    const SZrTypeValue *exportedValue;
    SZrObject *object;

    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    name = ZrCore_String_Create(state, (TZrNativeString)typeName, strlen(typeName));
    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    exportedValue = ZrCore_Module_GetPubExport(state, module, name);
    if (exportedValue == ZR_NULL || exportedValue->type != ZR_VALUE_TYPE_OBJECT || exportedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

static const SZrManagedFieldInfo *find_managed_field_info(const SZrObjectPrototype *prototype,
                                                          SZrState *state,
                                                          const TZrChar *fieldName) {
    SZrString *expectedName;

    if (prototype == ZR_NULL || state == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    expectedName = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 i = 0; i < prototype->managedFieldCount; i++) {
        const SZrManagedFieldInfo *info = &prototype->managedFields[i];
        if (info->name != ZR_NULL && ZrCore_String_Equal(info->name, expectedName)) {
            return info;
        }
    }

    return ZR_NULL;
}

static const SZrMemberDescriptor *find_member_descriptor_info(SZrState *state,
                                                              SZrObjectPrototype *prototype,
                                                              const TZrChar *memberName,
                                                              TZrBool includeInherited) {
    SZrString *expectedName;

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    expectedName = ZrCore_String_Create(state, (TZrNativeString)memberName, strlen(memberName));
    if (expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, expectedName, includeInherited);
}

static TZrBool lookup_struct_field_offset(SZrState *state,
                                        SZrStructPrototype *prototype,
                                        const TZrChar *fieldName,
                                        TZrUInt64 *outOffset) {
    SZrString *fieldNameString;
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL || outOffset == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    pair = ZrCore_HashSet_Find(state, &prototype->keyOffsetMap, &key);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    *outOffset = pair->value.value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static char *read_reference_file(const TZrChar *relativePath, TZrSize *size) {
    return ZrTests_Reference_ReadFixture(relativePath, size);
}

typedef struct {
    TZrBool reported;
    SZrFileRange location;
    EZrToken token;
    char message[256];
} SZrCapturedParserDiagnostic;

static void clear_parser_diagnostic(SZrCapturedParserDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
}

static void capture_parser_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SZrCapturedParserDiagnostic *diagnostic = (SZrCapturedParserDiagnostic *)userData;

    if (diagnostic == ZR_NULL || diagnostic->reported) {
        return;
    }

    diagnostic->reported = ZR_TRUE;
    diagnostic->token = token;
    if (location != ZR_NULL) {
        diagnostic->location = *location;
    }
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

static SZrAstNode *parse_reference_source_with_diagnostic(SZrState *state,
                                                          const char *source,
                                                          size_t sourceLength,
                                                          const char *sourceNameText,
                                                          SZrCapturedParserDiagnostic *diagnostic) {
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *ast;

    clear_parser_diagnostic(diagnostic);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, state, source, sourceLength, sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    if (diagnostic != ZR_NULL &&
        !diagnostic->reported &&
        parserState.hasError &&
        parserState.errorMessage != ZR_NULL) {
        diagnostic->reported = ZR_TRUE;
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", parserState.errorMessage);
    }
    ZrParser_State_Free(&parserState);
    return ast;
}

static TZrSize count_substring_occurrences(const TZrChar *text, const TZrChar *needle) {
    TZrSize count = 0;
    const TZrChar *cursor;
    TZrSize needleLength;

    if (text == ZR_NULL || needle == ZR_NULL || needle[0] == '\0') {
        return 0;
    }

    needleLength = strlen(needle);
    cursor = text;
    while ((cursor = strstr(cursor, needle)) != ZR_NULL) {
        count += 1;
        cursor += needleLength;
    }

    return count;
}

// 测试初始化和清理
void setUp(void) {}

void tearDown(void) {}

// ==================== 1. 脚本级变量作为 __entry 局部变量 ====================

static void test_script_level_variables_as_entry_locals(void) {
    SZrTestTimer timer;
    const char *testSummary = "Script Level Variables as __entry Locals";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Script level variables compilation",
              "Testing that script-level variables are compiled as __entry function local variables");

    const char *source = "var x = 1;\nvar y = 2;\nvar z = x + y;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func != ZR_NULL) {
        // 检查函数是否有局部变量
        TEST_ASSERT_TRUE(func->localVariableLength >= 3); // 至少应该有 x, y, z 三个局部变量

        // 检查局部变量名
        TZrBool foundX = ZR_FALSE;
        TZrBool foundY = ZR_FALSE;
        TZrBool foundZ = ZR_FALSE;

        for (TZrUInt32 i = 0; i < func->localVariableLength; i++) {
            SZrString *varName = func->localVariableList[i].name;
            if (varName != ZR_NULL) {
                SZrString *xName = ZrCore_String_Create(state, "x", 1);
                SZrString *yName = ZrCore_String_Create(state, "y", 1);
                SZrString *zName = ZrCore_String_Create(state, "z", 1);
                if (ZrCore_String_Equal(varName, xName))
                    foundX = ZR_TRUE;
                if (ZrCore_String_Equal(varName, yName))
                    foundY = ZR_TRUE;
                if (ZrCore_String_Equal(varName, zName))
                    foundZ = ZR_TRUE;
            }
        }

        TEST_ASSERT_TRUE(foundX);
        TEST_ASSERT_TRUE(foundY);
        TEST_ASSERT_TRUE(foundZ);

        ZrCore_Function_Free(state, func);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 2. pub/pri/pro 可见性修饰符解析 ====================

static void test_visibility_modifiers_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Visibility Modifiers Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Visibility modifiers parsing", "Testing parsing of pub/pri/pro visibility modifiers for variables");

    // 测试 pub var
    const char *source1 = "pub var x = 1;";
    SZrString *sourceName1 = ZrCore_String_Create(state, "test1.zr", 8);
    SZrAstNode *ast1 = ZrParser_Parse(state, source1, strlen(source1), sourceName1);
    TEST_ASSERT_NOT_NULL(ast1);
    if (ast1 != ZR_NULL) {
        if (ast1->type == ZR_AST_SCRIPT && ast1->data.script.statements != ZR_NULL) {
            if (ast1->data.script.statements->count > 0) {
                SZrAstNode *stmt = ast1->data.script.statements->nodes[0];
                if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                    EZrAccessModifier access = stmt->data.variableDeclaration.accessModifier;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, access);
                }
            }
        }
        ZrParser_Ast_Free(state, ast1);
    }

    // 测试 pri var（默认）
    const char *source2 = "var y = 2;";
    SZrString *sourceName2 = ZrCore_String_Create(state, "test2.zr", 8);
    SZrAstNode *ast2 = ZrParser_Parse(state, source2, strlen(source2), sourceName2);
    TEST_ASSERT_NOT_NULL(ast2);
    if (ast2 != ZR_NULL) {
        if (ast2->type == ZR_AST_SCRIPT && ast2->data.script.statements != ZR_NULL) {
            if (ast2->data.script.statements->count > 0) {
                SZrAstNode *stmt = ast2->data.script.statements->nodes[0];
                if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                    EZrAccessModifier access = stmt->data.variableDeclaration.accessModifier;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PRIVATE, access);
                }
            }
        }
        ZrParser_Ast_Free(state, ast2);
    }

    // 测试 pro var
    const char *source3 = "pro var z = 3;";
    SZrString *sourceName3 = ZrCore_String_Create(state, "test3.zr", 8);
    SZrAstNode *ast3 = ZrParser_Parse(state, source3, strlen(source3), sourceName3);
    TEST_ASSERT_NOT_NULL(ast3);
    if (ast3 != ZR_NULL) {
        if (ast3->type == ZR_AST_SCRIPT && ast3->data.script.statements != ZR_NULL) {
            if (ast3->data.script.statements->count > 0) {
                SZrAstNode *stmt = ast3->data.script.statements->nodes[0];
                if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                    EZrAccessModifier access = stmt->data.variableDeclaration.accessModifier;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PROTECTED, access);
                }
            }
        }
        ZrParser_Ast_Free(state, ast3);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 3. 模块导出收集（pub 和 pro 分开存储）====================

static void test_module_export_collection(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Export Collection";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module export collection",
              "Testing that pub and pro variables are collected and stored separately in module object");

    const char *source = "pub var pubVar = 10;\npro var proVar = 20;\npri var priVar = 30;";
    SZrString *sourceName = ZrCore_String_Create(state, "test_module.zr", 14);
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func != ZR_NULL) {
        // 检查导出变量信息
        TEST_ASSERT_TRUE(func->exportedVariableLength >= 2); // 至少应该有 pubVar 和 proVar

        TZrBool foundPubVar = ZR_FALSE;
        TZrBool foundProVar = ZR_FALSE;
        TZrBool foundPriVar = ZR_FALSE;

        for (TZrUInt32 i = 0; i < func->exportedVariableLength; i++) {
            SZrString *varName = func->exportedVariables[i].name;
            TZrUInt8 accessModifier = func->exportedVariables[i].accessModifier;

            if (varName != ZR_NULL) {
                SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
                SZrString *proVarName = ZrCore_String_Create(state, "proVar", 6);
                SZrString *priVarName = ZrCore_String_Create(state, "priVar", 6);

                if (ZrCore_String_Equal(varName, pubVarName)) {
                    foundPubVar = ZR_TRUE;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, accessModifier);
                } else if (ZrCore_String_Equal(varName, proVarName)) {
                    foundProVar = ZR_TRUE;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PROTECTED, accessModifier);
                } else if (ZrCore_String_Equal(varName, priVarName)) {
                    foundPriVar = ZR_TRUE;
                }
            }
        }

        TEST_ASSERT_TRUE(foundPubVar);
        TEST_ASSERT_TRUE(foundProVar);
        TEST_ASSERT_TRUE(!foundPriVar); // pri 变量不应该被导出

        ZrCore_Function_Free(state, func);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 4. 模块缓存机制 ====================

static void test_module_cache_operations(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Cache Operations";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module cache operations", "Testing module cache lookup and add operations with path as key");

    // 创建测试模块
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test_module", 11);
    SZrString *fullPath = ZrCore_String_Create(state, "test/path/module.zr", 19);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);

    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    // 添加 pub 导出
    SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
    SZrTypeValue pubValue;
    ZrCore_Value_InitAsInt(state, &pubValue, 100);
    ZrCore_Module_AddPubExport(state, module, pubVarName, &pubValue);

    // 添加到缓存
    SZrString *cacheKey = ZrCore_String_Create(state, "test/path/module.zr", 19);
    ZrCore_Module_AddToCache(state, cacheKey, module);

    // 从缓存获取
    SZrObjectModule *cachedModule = ZrCore_Module_GetFromCache(state, cacheKey);
    TEST_ASSERT_NOT_NULL(cachedModule);
    TEST_ASSERT_EQUAL_PTR(module, cachedModule);

    // 验证模块信息
    TEST_ASSERT_EQUAL_UINT64(pathHash, cachedModule->pathHash);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(moduleName, cachedModule->moduleName));

    // 验证导出
    const SZrTypeValue *retrievedPub = ZrCore_Module_GetPubExport(state, cachedModule, pubVarName);
    TEST_ASSERT_NOT_NULL(retrievedPub);
    TEST_ASSERT_EQUAL_INT64(100, retrievedPub->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 5. 内部模块导入 helper / zr.import 隐藏 ====================

static void test_module_import_helper_and_hidden_zr_import(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Import Helper And Hidden zr.import";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("module import helper and hidden zr.import",
              "Testing internal module import helper loads native modules while zr.import is no longer script-visible");

    SZrGlobalState *global = state->global;
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(global->zrObject.type == ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NULL(get_zr_global_value(state, "import"));

    {
        SZrString *modulePath = ZrCore_String_Create(state, "zr.system", 9);
        SZrObjectModule *firstImport;
        SZrObjectModule *cachedImport;

        TEST_ASSERT_NOT_NULL(modulePath);
        firstImport = ZrCore_Module_ImportByPath(state, modulePath);
        TEST_ASSERT_NOT_NULL(firstImport);
        cachedImport = ZrCore_Module_ImportByPath(state, modulePath);
        TEST_ASSERT_NOT_NULL(cachedImport);
        TEST_ASSERT_EQUAL_PTR(firstImport, cachedImport);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 6. 缓存命中/未命中场景 ====================

static void test_module_cache_hit_miss(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Cache Hit/Miss Scenarios";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module cache hit/miss scenarios", "Testing cache hit and miss scenarios");

    // 测试缓存未命中
    SZrString *nonExistentPath = ZrCore_String_Create(state, "non/existent/path.zr", 21);
    SZrObjectModule *cached1 = ZrCore_Module_GetFromCache(state, nonExistentPath);
    TEST_ASSERT_NULL(cached1);

    // 创建并添加模块到缓存
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test", 4);
    SZrString *fullPath = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    SZrString *cacheKey = ZrCore_String_Create(state, "test/path.zr", 12);
    ZrCore_Module_AddToCache(state, cacheKey, module);

    // 测试缓存命中
    SZrObjectModule *cached2 = ZrCore_Module_GetFromCache(state, cacheKey);
    TEST_ASSERT_NOT_NULL(cached2);
    TEST_ASSERT_EQUAL_PTR(module, cached2);

    // 测试不同路径的缓存未命中
    SZrString *differentPath = ZrCore_String_Create(state, "different/path.zr", 18);
    SZrObjectModule *cached3 = ZrCore_Module_GetFromCache(state, differentPath);
    TEST_ASSERT_NULL(cached3);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 7. 模块可见性访问控制 ====================

static void test_module_visibility_access_control(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Visibility Access Control";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module visibility access control", "Testing that pub exports are accessible across modules, pro exports "
                                                  "are accessible within same module library");

    // 创建测试模块
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test_module", 11);
    SZrString *fullPath = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    // 添加 pub 导出
    SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
    SZrTypeValue pubValue;
    ZrCore_Value_InitAsInt(state, &pubValue, 100);
    ZrCore_Module_AddPubExport(state, module, pubVarName, &pubValue);

    // 添加 pro 导出
    SZrString *proVarName = ZrCore_String_Create(state, "proVar", 6);
    SZrTypeValue proValue;
    ZrCore_Value_InitAsInt(state, &proValue, 200);
    ZrCore_Module_AddProExport(state, module, proVarName, &proValue);

    // 测试 pub 导出访问（跨模块）
    const SZrTypeValue *retrievedPub = ZrCore_Module_GetPubExport(state, module, pubVarName);
    TEST_ASSERT_NOT_NULL(retrievedPub);
    TEST_ASSERT_EQUAL_INT64(100, retrievedPub->value.nativeObject.nativeInt64);

    // 测试 pro 导出访问（同模块库）
    const SZrTypeValue *retrievedPro = ZrCore_Module_GetProExport(state, module, proVarName);
    TEST_ASSERT_NOT_NULL(retrievedPro);
    TEST_ASSERT_EQUAL_INT64(200, retrievedPro->value.nativeObject.nativeInt64);

    // 验证 pro 区域包含 pub（pro 应该包含所有 pub）
    const SZrTypeValue *pubInPro = ZrCore_Module_GetProExport(state, module, pubVarName);
    TEST_ASSERT_NOT_NULL(pubInPro);
    TEST_ASSERT_EQUAL_INT64(100, pubInPro->value.nativeObject.nativeInt64);

    // 验证 pro 变量在 pub 区域不可访问
    const SZrTypeValue *proInPub = ZrCore_Module_GetPubExport(state, module, proVarName);
    TEST_ASSERT_NULL(proInPub); // pro 变量不应该在 pub 区域

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 8. zr 标识符的全局访问和作用域覆盖 ====================

static void test_zr_identifier_global_access_and_scope_override(void) {
    SZrTestTimer timer;
    const char *testSummary = "zr Identifier Global Access and Scope Override";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("zr identifier access",
              "Testing that zr identifier is globally accessible but can be overridden by local variables");

    // 测试全局 zr 对象访问
    SZrGlobalState *global = state->global;
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(global->zrObject.type == ZR_VALUE_TYPE_OBJECT);

    // 编译一个访问 zr 的脚本
    const char *source1 = "var result = zr;";
    SZrString *sourceName1 = ZrCore_String_Create(state, "test1.zr", 8);
    SZrFunction *func1 = ZrParser_Source_Compile(state, source1, strlen(source1), sourceName1);
    TEST_ASSERT_NOT_NULL(func1);

    // 检查是否生成了 GET_GLOBAL 指令
    TZrBool foundGetGlobal = ZR_FALSE;
    for (TZrUInt32 i = 0; i < func1->instructionsLength; i++) {
        EZrInstructionCode opcode = (EZrInstructionCode) func1->instructionsList[i].instruction.operationCode;
        if (opcode == ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
            foundGetGlobal = ZR_TRUE;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundGetGlobal);

    ZrCore_Function_Free(state, func1);

    // 测试局部变量覆盖 zr
    const char *source2 = "var zr = 123;\nvar result = zr;";
    SZrString *sourceName2 = ZrCore_String_Create(state, "test2.zr", 8);
    SZrFunction *func2 = ZrParser_Source_Compile(state, source2, strlen(source2), sourceName2);
    TEST_ASSERT_NOT_NULL(func2);

    // 检查是否使用了 GET_STACK 而不是 GET_GLOBAL（因为局部变量覆盖了全局 zr）
    TZrBool foundGetStack = ZR_FALSE;
    TZrBool foundGetGlobal2 = ZR_FALSE;
    for (TZrUInt32 i = 0; i < func2->instructionsLength; i++) {
        EZrInstructionCode opcode = (EZrInstructionCode) func2->instructionsList[i].instruction.operationCode;
        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK)) {
            foundGetStack = ZR_TRUE;
        }
        if (opcode == ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
            foundGetGlobal2 = ZR_TRUE;
        }
    }
    // 应该使用 GET_STACK 访问局部变量，而不是 GET_GLOBAL
    TEST_ASSERT_TRUE(foundGetStack);
    // 不应该使用 GET_GLOBAL（因为局部变量覆盖了）
    TEST_ASSERT_TRUE(!foundGetGlobal2);

    ZrCore_Function_Free(state, func2);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 9. 路径哈希计算 ====================

static void test_path_hash_calculation(void) {
    SZrTestTimer timer;
    const char *testSummary = "Path Hash Calculation";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Path hash calculation", "Testing path hash calculation using xxhash");

    SZrString *path1 = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 hash1 = ZrCore_Module_CalculatePathHash(state, path1);
    TEST_ASSERT_NOT_EQUAL_UINT64(0, hash1);

    // 相同路径应该产生相同哈希
    SZrString *path2 = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 hash2 = ZrCore_Module_CalculatePathHash(state, path2);
    TEST_ASSERT_EQUAL_UINT64(hash1, hash2);

    // 不同路径应该产生不同哈希
    SZrString *path3 = ZrCore_String_Create(state, "different/path.zr", 18);
    TZrUInt64 hash3 = ZrCore_Module_CalculatePathHash(state, path3);
    TEST_ASSERT_NOT_EQUAL_UINT64(hash1, hash3);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 10. 完整模块加载流程 ====================

static void test_complete_module_loading_flow(void) {
    SZrTestTimer timer;
    const char *testSummary = "Complete Module Loading Flow";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Complete module loading flow",
              "Testing complete flow: compile source -> collect exports -> create module -> cache");

    // 编译包含导出的源代码
    const char *source = "%module \"test_module\";\npub var pubVar = 100;\npro var proVar = 200;";
    SZrString *sourceName = ZrCore_String_Create(state, "test_module.zr", 14);
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(func);

    // 检查导出信息
    TEST_ASSERT_TRUE(func->exportedVariableLength >= 2);

    // 创建模块对象
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test_module", 11);
    SZrString *fullPath = ZrCore_String_Create(state, "test_module.zr", 14);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    // 模拟导出收集（从函数的导出信息中收集）
    for (TZrUInt32 i = 0; i < func->exportedVariableLength; i++) {
        SZrString *varName = func->exportedVariables[i].name;
        TZrUInt8 accessModifier = func->exportedVariables[i].accessModifier;

        // 创建测试值
        SZrTypeValue value;
        ZrCore_Value_InitAsInt(state, &value, (TZrInt64) (i + 1) * 100);

        if (accessModifier == ZR_ACCESS_PUBLIC) {
            ZrCore_Module_AddPubExport(state, module, varName, &value);
        } else if (accessModifier == ZR_ACCESS_PROTECTED) {
            ZrCore_Module_AddProExport(state, module, varName, &value);
        }
    }

    // 添加到缓存
    SZrString *cacheKey = ZrCore_String_Create(state, "test_module.zr", 14);
    ZrCore_Module_AddToCache(state, cacheKey, module);

    // 从缓存获取并验证
    SZrObjectModule *cachedModule = ZrCore_Module_GetFromCache(state, cacheKey);
    TEST_ASSERT_NOT_NULL(cachedModule);
    TEST_ASSERT_EQUAL_PTR(module, cachedModule);

    // 验证导出
    SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
    const SZrTypeValue *pubValue = ZrCore_Module_GetPubExport(state, cachedModule, pubVarName);
    TEST_ASSERT_NOT_NULL(pubValue);

    SZrString *proVarName = ZrCore_String_Create(state, "proVar", 6);
    const SZrTypeValue *proValue = ZrCore_Module_GetProExport(state, cachedModule, proVarName);
    TEST_ASSERT_NOT_NULL(proValue);

    ZrCore_Function_Free(state, func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_module_restores_owned_field_prototype_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Restores Owned Field Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Owned field runtime metadata",
              "Testing that prototypeData loading restores struct field offsets and managed-field metadata for direct %unique/%shared fields");

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        const char *source =
            "%module \"field_meta\";\n"
            "pub struct HandleBox { var handle: %unique Resource; var count: int; }\n"
            "pub class Holder { var resource: %shared Resource; var version: int; }";
        SZrString *sourceName = ZrCore_String_Create(state, "field_meta.zr", 13);
        SZrFunction *entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        SZrObjectModule *module;
        SZrObjectPrototype *handleBoxPrototype;
        SZrObjectPrototype *holderPrototype;
        const SZrManagedFieldInfo *handleInfo;
        const SZrManagedFieldInfo *resourceInfo;
        TZrUInt64 handleOffset = 0;
        TZrUInt64 countOffset = 0;
        TZrSize createdCount;

        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_NOT_NULL(entryFunction->prototypeData);
        TEST_ASSERT_EQUAL_UINT32(2, entryFunction->prototypeCount);

        module = ZrCore_Module_Create(state);
        TEST_ASSERT_NOT_NULL(module);
        ZrCore_Module_SetInfo(state,
                        module,
                        ZrCore_String_Create(state, "field_meta", 10),
                        ZrCore_Module_CalculatePathHash(state, ZrCore_String_Create(state, "field_meta.zr", 13)),
                        ZrCore_String_Create(state, "field_meta.zr", 13));

        createdCount = ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction);
        TEST_ASSERT_EQUAL_UINT64(2, createdCount);

        handleBoxPrototype = get_module_exported_prototype(state, module, "HandleBox");
        holderPrototype = get_module_exported_prototype(state, module, "Holder");
        TEST_ASSERT_NOT_NULL(handleBoxPrototype);
        TEST_ASSERT_NOT_NULL(holderPrototype);

        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, handleBoxPrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_CLASS, holderPrototype->type);

        TEST_ASSERT_EQUAL_UINT32(1, handleBoxPrototype->managedFieldCount);
        TEST_ASSERT_EQUAL_UINT32(1, holderPrototype->managedFieldCount);

        handleInfo = find_managed_field_info(handleBoxPrototype, state, "handle");
        resourceInfo = find_managed_field_info(holderPrototype, state, "resource");
        TEST_ASSERT_NOT_NULL(handleInfo);
        TEST_ASSERT_NOT_NULL(resourceInfo);

        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_UNIQUE, handleInfo->ownershipQualifier);
        TEST_ASSERT_TRUE(handleInfo->callsClose);
        TEST_ASSERT_TRUE(handleInfo->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, handleInfo->declarationOrder);

        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_SHARED, resourceInfo->ownershipQualifier);
        TEST_ASSERT_TRUE(resourceInfo->callsClose);
        TEST_ASSERT_TRUE(resourceInfo->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, resourceInfo->declarationOrder);

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)handleBoxPrototype, "handle", &handleOffset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)handleBoxPrototype, "count", &countOffset));
        TEST_ASSERT_EQUAL_UINT64(0, handleOffset);
        TEST_ASSERT_TRUE(countOffset > handleOffset);

        ZrCore_Function_Free(state, entryFunction);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_module_restores_advanced_oop_runtime_descriptor_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Restores Advanced OOP Runtime Descriptor Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Advanced OOP runtime descriptor metadata",
              "Testing that module prototype materialization restores modifier flags, override chains, virtual slots, property identity, and interface contract slots");

    {
        SZrState *state = create_test_state();
        const char *source =
                "%module \"advanced_oop_runtime\";\n"
                "pub abstract class Base {\n"
                "    pub abstract ping(): int;\n"
                "    pub abstract get score: int;\n"
                "}\n"
                "pub final class Derived : Base {\n"
                "    pub override final ping(): int { return 1; }\n"
                "    pub override get final score: int { return 2; }\n"
                "}\n"
                "interface Readable { read(): int; }\n"
                "pub class Device : Readable {\n"
                "    pub read(): int { return 1; }\n"
                "}\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrObjectModule *module;
        SZrObjectPrototype *basePrototype;
        SZrObjectPrototype *derivedPrototype;
        SZrObjectPrototype *devicePrototype;
        const SZrMemberDescriptor *basePing;
        const SZrMemberDescriptor *derivedPing;
        const SZrMemberDescriptor *baseGetter;
        const SZrMemberDescriptor *derivedGetter;
        const SZrMemberDescriptor *deviceRead;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "advanced_oop_runtime.zr", 23);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_NOT_NULL(entryFunction->prototypeData);

        module = ZrCore_Module_Create(state);
        TEST_ASSERT_NOT_NULL(module);
        ZrCore_Module_SetInfo(state,
                              module,
                              ZrCore_String_Create(state, "advanced_oop_runtime", 20),
                              ZrCore_Module_CalculatePathHash(state,
                                                              ZrCore_String_Create(state,
                                                                                   "advanced_oop_runtime.zr",
                                                                                   23)),
                              ZrCore_String_Create(state, "advanced_oop_runtime.zr", 23));

        TEST_ASSERT_TRUE(ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction) >= 3);

        basePrototype = get_module_exported_prototype(state, module, "Base");
        derivedPrototype = get_module_exported_prototype(state, module, "Derived");
        devicePrototype = get_module_exported_prototype(state, module, "Device");
        TEST_ASSERT_NOT_NULL(basePrototype);
        TEST_ASSERT_NOT_NULL(derivedPrototype);
        TEST_ASSERT_NOT_NULL(devicePrototype);

        TEST_ASSERT_NOT_EQUAL_UINT32(0, basePrototype->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedPrototype->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);
        TEST_ASSERT_EQUAL_UINT32(2, basePrototype->nextVirtualSlotIndex);
        TEST_ASSERT_EQUAL_UINT32(basePrototype->nextVirtualSlotIndex, derivedPrototype->nextVirtualSlotIndex);
        TEST_ASSERT_EQUAL_UINT32(1, basePrototype->nextPropertyIdentity);
        TEST_ASSERT_EQUAL_UINT32(basePrototype->nextPropertyIdentity, derivedPrototype->nextPropertyIdentity);

        basePing = find_member_descriptor_info(state, basePrototype, "ping", ZR_FALSE);
        derivedPing = find_member_descriptor_info(state, derivedPrototype, "ping", ZR_FALSE);
        baseGetter = find_member_descriptor_info(state, basePrototype, "__get_score", ZR_FALSE);
        derivedGetter = find_member_descriptor_info(state, derivedPrototype, "__get_score", ZR_FALSE);
        deviceRead = find_member_descriptor_info(state, devicePrototype, "read", ZR_FALSE);
        TEST_ASSERT_NOT_NULL(basePing);
        TEST_ASSERT_NOT_NULL(derivedPing);
        TEST_ASSERT_NOT_NULL(baseGetter);
        TEST_ASSERT_NOT_NULL(derivedGetter);
        TEST_ASSERT_NOT_NULL(deviceRead);

        TEST_ASSERT_NOT_EQUAL_UINT32(0, basePing->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, basePing->modifierFlags & ZR_DECLARATION_MODIFIER_VIRTUAL);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedPing->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedPing->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);
        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, basePing->virtualSlotIndex);
        TEST_ASSERT_EQUAL_UINT32(basePing->virtualSlotIndex, derivedPing->virtualSlotIndex);
        TEST_ASSERT_NOT_NULL(derivedPing->ownerTypeName);
        TEST_ASSERT_NOT_NULL(derivedPing->baseDefinitionOwnerTypeName);
        TEST_ASSERT_NOT_NULL(derivedPing->baseDefinitionName);
        TEST_ASSERT_TRUE(string_equals_cstring(derivedPing->ownerTypeName, "Derived"));
        TEST_ASSERT_TRUE(string_equals_cstring(derivedPing->baseDefinitionOwnerTypeName, "Base"));
        TEST_ASSERT_TRUE(string_equals_cstring(derivedPing->baseDefinitionName, "ping"));

        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedGetter->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedGetter->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);
        TEST_ASSERT_EQUAL_UINT32(1, baseGetter->accessorRole);
        TEST_ASSERT_EQUAL_UINT32(1, derivedGetter->accessorRole);
        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, baseGetter->propertyIdentity);
        TEST_ASSERT_EQUAL_UINT32(baseGetter->propertyIdentity, derivedGetter->propertyIdentity);
        TEST_ASSERT_TRUE(string_equals_cstring(derivedGetter->baseDefinitionOwnerTypeName, "Base"));
        TEST_ASSERT_TRUE(string_equals_cstring(derivedGetter->baseDefinitionName, "__get_score"));

        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, deviceRead->interfaceContractSlot);
        TEST_ASSERT_EQUAL_UINT32(0, deviceRead->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE);
        TEST_ASSERT_TRUE(string_equals_cstring(deviceRead->ownerTypeName, "Device"));

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_exports_complex_function_graph_without_null_call_targets(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "lin_alg",
                    "projectVectorsImpl(seed) {\n"
                    "    return seed + 2;\n"
                    "}\n"
                    "pub var projectVectors = projectVectorsImpl;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "signal",
                    "mixSignalImpl(seed) {\n"
                    "    return seed + 3;\n"
                    "}\n"
                    "pub var mixSignal = mixSignalImpl;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "tensor_pipeline",
                    "runTensorPassImpl() {\n"
                    "    return 11;\n"
                    "}\n"
                    "pub var runTensorPass = runTensorPassImpl;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "probe_callbacks",
                    "var lin = %import(\"lin_alg\");\n"
                    "var signal = %import(\"signal\");\n"
                    "var tensor = %import(\"tensor_pipeline\");\n"
                    "\n"
                    "scaleValue(input) {\n"
                    "    return input + 1;\n"
                    "}\n"
                    "\n"
                    "runProbeImpl() {\n"
                    "    var vectorValue = lin.projectVectors(2);\n"
                    "    var signalValue = signal.mixSignal(5);\n"
                    "    var tensorValue = tensor.runTensorPass();\n"
                    "    return scaleValue(vectorValue + signalValue + tensorValue);\n"
                    "}\n"
                    "\n"
                    "summarizeProbeImpl(value) {\n"
                    "    return value + 7;\n"
                    "}\n"
                    "\n"
                    "pub var runProbe = runProbeImpl;\n"
                    "pub var summarizeProbe = summarizeProbeImpl;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source Module Exports Complex Function Graph";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *runExport;
        const SZrTypeValue *summarizeExport;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);
        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "probe_callbacks");
        TEST_ASSERT_NOT_NULL(module);

        runExport = get_module_export_value(state, module, "runProbe");
        summarizeExport = get_module_export_value(state, module, "summarizeProbe");
        TEST_ASSERT_NOT_NULL(runExport);
        TEST_ASSERT_NOT_NULL(summarizeExport);
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(runExport->type));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(summarizeExport->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(runExport->type) || ZR_VALUE_IS_TYPE_CLOSURE(runExport->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(summarizeExport->type) || ZR_VALUE_IS_TYPE_CLOSURE(summarizeExport->type));

        TEST_ASSERT_TRUE(ZrLib_CallValue(state, runExport, ZR_NULL, ZR_NULL, 0, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
        TEST_ASSERT_EQUAL_INT64(24, result.value.nativeObject.nativeInt64);

        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_preinstalled_callable_preserves_imported_module_captures_after_native_imports(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "lin_alg",
                    "projectVectorsImpl(seed) {\n"
                    "    return seed + 2;\n"
                    "}\n"
                    "pub var projectVectors = projectVectorsImpl;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "signal",
                    "mixSignalImpl(seed) {\n"
                    "    return seed + 3;\n"
                    "}\n"
                    "pub var mixSignal = mixSignalImpl;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "tensor_pipeline",
                    "runTensorPassImpl() {\n"
                    "    return 11;\n"
                    "}\n"
                    "pub var runTensorPass = runTensorPassImpl;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "probe_callbacks",
                    "var math = %import(\"zr.math\");\n"
                    "var system = %import(\"zr.system\");\n"
                    "var lin = %import(\"lin_alg\");\n"
                    "var signal = %import(\"signal\");\n"
                    "var tensor = %import(\"tensor_pipeline\");\n"
                    "\n"
                    "helperValue() {\n"
                    "    return 0;\n"
                    "}\n"
                    "\n"
                    "pub runProbe(): int {\n"
                    "    var vector = $math.Vector3(1.0, 2.0, 3.0);\n"
                    "    system.console.printLine(\"probe\");\n"
                    "    return lin.projectVectors(2) + signal.mixSignal(5) + tensor.runTensorPass() + <int>vector.x;\n"
                    "}\n"
                    "\n"
                    "pub summarizeProbe(value): int {\n"
                    "    return value + helperValue();\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source Module Preinstalled Callable Preserves Imported Module Captures After Native Imports";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *runExport;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "probe_callbacks");
        TEST_ASSERT_NOT_NULL(module);

        runExport = get_module_export_value(state, module, "runProbe");
        TEST_ASSERT_NOT_NULL(runExport);
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(runExport->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(runExport->type) || ZR_VALUE_IS_TYPE_CLOSURE(runExport->type));

        TEST_ASSERT_TRUE(ZrLib_CallValue(state, runExport, ZR_NULL, ZR_NULL, 0, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
        TEST_ASSERT_EQUAL_INT64(24, result.value.nativeObject.nativeInt64);

        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_imported_function_alias_with_parameters_preserves_call_signature(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "dep",
                    "sumImpl(left: int, right: int) {\n"
                    "    return left + right;\n"
                    "}\n"
                    "pub var sum = sumImpl;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Imported Function Alias With Parameters Preserves Call Signature";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var dep = %import(\"dep\");\n"
                "return dep.sum(3, 4);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "imported_function_alias_signature_test.zr", 41);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
        TEST_ASSERT_EQUAL_INT64(7, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_module_init_summary_cache_growth_keeps_current_source_summary_stable(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT("dep0", "pub value(): int {\n    return 0;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep1", "pub value(): int {\n    return 1;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep2", "pub value(): int {\n    return 2;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep3", "pub value(): int {\n    return 3;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep4", "pub value(): int {\n    return 4;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep5", "pub value(): int {\n    return 5;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep6", "pub value(): int {\n    return 6;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep7", "pub value(): int {\n    return 7;\n}\n"),
            MODULE_FIXTURE_SOURCE_TEXT("dep8", "pub value(): int {\n    return 8;\n}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Module Init Summary Cache Growth Keeps Current Source Summary Stable";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var dep0 = %import(\"dep0\");\n"
                "var dep1 = %import(\"dep1\");\n"
                "var dep2 = %import(\"dep2\");\n"
                "var dep3 = %import(\"dep3\");\n"
                "var dep4 = %import(\"dep4\");\n"
                "var dep5 = %import(\"dep5\");\n"
                "var dep6 = %import(\"dep6\");\n"
                "var dep7 = %import(\"dep7\");\n"
                "var dep8 = %import(\"dep8\");\n"
                "return dep8.value() + dep0.value();\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "module_init_summary_realloc_safe_test.zr", 41);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_EQUAL_UINT32(9u, entryFunction->staticImportLength);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT64(8, result);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_cyclic_source_modules_allow_declaration_ready_function_references(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "a",
                    "var b = %import(\"b\");\n"
                    "pub ping(): int {\n"
                    "    return 41;\n"
                    "}\n"
                    "pub var other = b.pong;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "b",
                    "var a = %import(\"a\");\n"
                    "pub pong(): int {\n"
                    "    return 1;\n"
                    "}\n"
                    "pub var other = a.ping;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Cyclic Source Modules Allow Declaration Ready Function References";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var a = %import(\"a\");\n"
                "return a.other() + a.ping();\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "cyclic_function_reference_pass_test.zr", 38);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT64(42, result);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_cyclic_source_modules_allow_safe_imported_call_during_entry(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "a",
                    "var b = %import(\"b\");\n"
                    "pub ping(): int {\n"
                    "    return 41;\n"
                    "}\n"
                    "pub var value = b.callPing();\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "b",
                    "var a = %import(\"a\");\n"
                    "pub callPing(): int {\n"
                    "    return a.ping();\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Cyclic Source Modules Allow Safe Imported Call During Entry";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var a = %import(\"a\");\n"
                "return a.value;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "cyclic_safe_imported_call_pass_test.zr", 39);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT64(41, result);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_cyclic_source_modules_dynamic_entry_read_raises_cycle_init_error(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "a",
                    "var b = %import(\"b\");\n"
                    "pub var a1 = 41;\n"),
            MODULE_FIXTURE_SOURCE_TEXT(
                    "b",
                    "var a = %import(\"a\");\n"
                    "pub var b1 = a[\"a1\"];\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Cyclic Source Modules Dynamic Entry Read Raises Cycle Init Error";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var b = %import(\"b\");\n"
                "return 0;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        const TZrChar *message;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "cyclic_dynamic_entry_read_runtime_guard_test.zr", 47);
        TEST_ASSERT_NOT_NULL(sourceName);

        source =
                "var a = %import(\"a\");\n"
                "return 0;\n";
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer + 1, state->stackTop.valuePointer);
        TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
        TEST_ASSERT_TRUE(state->hasCurrentException);
        message = get_current_exception_message_cstring(state);
        TEST_ASSERT_NOT_NULL(message);
        TEST_ASSERT_NOT_NULL(strstr(message, "circular import initialization"));
        TEST_ASSERT_NOT_NULL(strstr(message, "a.a1"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_module_init_callable_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reference.binary_peer",
                    "pub fb(): int {\n"
                    "    return 11;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Module Init Callable Metadata";
    const TZrChar *moduleSource =
            "%module \"reference.binary_meta_init\";\n"
            "var peer = %import(\"reference.binary_peer\");\n"
            "pub callPeer(): int {\n"
            "    return peer.fb();\n"
            "}\n"
            "pub capturePeerFn(): int {\n"
            "    var fnRef = peer.fb;\n"
            "    return 1;\n"
            "}\n"
            "pub var peerFn = peer.fb;\n"
            "pub var value: int = 2;\n";
    const TZrChar *intermediatePath = "module_init_callable_metadata_roundtrip.zri";
    const TZrChar *binaryPath = "module_init_callable_metadata_roundtrip.zro";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrString *sourceName = ZR_NULL;
        SZrFunction *sourceFunction = ZR_NULL;
        TZrChar *intermediateText = ZR_NULL;
        TZrSize intermediateLength = 0;
        TZrByte *binaryBytes = ZR_NULL;
        TZrSize binaryLength = 0;
        SZrModuleFixtureReader reader = {0};
        SZrIo io;
        SZrIoSource *sourceObject = ZR_NULL;
        const SZrIoFunction *binaryEntry = ZR_NULL;
        SZrFunction *runtimeFunction = ZR_NULL;
        TZrUInt32 index;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "reference.binary_meta_init.zr",
                                          strlen("reference.binary_meta_init.zr"));
        TEST_ASSERT_NOT_NULL(sourceName);

        sourceFunction = ZrParser_Source_Compile(state, moduleSource, strlen(moduleSource), sourceName);
        TEST_ASSERT_NOT_NULL(sourceFunction);
        TEST_ASSERT_EQUAL_UINT32(1u, sourceFunction->staticImportLength);
        TEST_ASSERT_EQUAL_UINT32(1u, sourceFunction->moduleEntryEffectLength);
        TEST_ASSERT_EQUAL_UINT32(2u, sourceFunction->exportedCallableSummaryLength);
        TEST_ASSERT_EQUAL_UINT32(2u, sourceFunction->topLevelCallableBindingLength);
        TEST_ASSERT_EQUAL_UINT32(4u, sourceFunction->exportedVariableLength);
        TEST_ASSERT_EQUAL_UINT32(4u, sourceFunction->typedExportedSymbolLength);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, sourceFunction, intermediatePath));
        intermediateText = ZrTests_ReadTextFile(intermediatePath, &intermediateLength);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_TRUE(intermediateLength > 0);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "STATIC_IMPORTS (1):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "MODULE_ENTRY_EFFECTS (1):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "EXPORTED_CALLABLE_SUMMARIES (2):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "TOP_LEVEL_CALLABLE_BINDINGS (2):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "IMPORT_CALL reference.binary_peer.fb"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "IMPORT_REF reference.binary_peer.fb"));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, sourceFunction, binaryPath));
        binaryBytes = read_test_file_bytes(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        ZrCore_Memory_RawSet(&io, 0, sizeof(io));
        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;
        ZrCore_Io_Init(state, &io, module_fixture_reader_read, ZR_NULL, &reader);
        io.isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(&io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
        TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
        binaryEntry = sourceObject->modules[0].entryFunction;

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->staticImportLength, (TZrUInt32)binaryEntry->staticImportsLength);
        for (index = 0; index < sourceFunction->staticImportLength; ++index) {
            TEST_ASSERT_TRUE(string_equals_cstring(binaryEntry->staticImports[index],
                                                   ZrCore_String_GetNativeString(sourceFunction->staticImports[index])));
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->moduleEntryEffectLength, (TZrUInt32)binaryEntry->moduleEntryEffectsLength);
        for (index = 0; index < sourceFunction->moduleEntryEffectLength; ++index) {
            const SZrFunctionModuleEffect *expectedEffect = &sourceFunction->moduleEntryEffects[index];
            const SZrIoFunctionModuleEffect *actualEffect = &binaryEntry->moduleEntryEffects[index];

            TEST_ASSERT_EQUAL_UINT8(expectedEffect->kind, actualEffect->kind);
            TEST_ASSERT_EQUAL_UINT8(expectedEffect->exportKind, actualEffect->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedEffect->readiness, actualEffect->readiness);
            TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->moduleName,
                                                   ZrCore_String_GetNativeString(expectedEffect->moduleName)));
            TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->symbolName,
                                                   ZrCore_String_GetNativeString(expectedEffect->symbolName)));
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->exportedCallableSummaryLength,
                                 (TZrUInt32)binaryEntry->exportedCallableSummariesLength);
        for (index = 0; index < sourceFunction->exportedCallableSummaryLength; ++index) {
            const SZrFunctionCallableSummary *expectedSummary = &sourceFunction->exportedCallableSummaries[index];
            const SZrIoFunctionCallableSummary *actualSummary = &binaryEntry->exportedCallableSummaries[index];
            TZrUInt32 effectIndex;

            TEST_ASSERT_TRUE(string_equals_cstring(actualSummary->name,
                                                   ZrCore_String_GetNativeString(expectedSummary->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedSummary->callableChildIndex, actualSummary->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT32(expectedSummary->effectCount, (TZrUInt32)actualSummary->effectCount);
            for (effectIndex = 0; effectIndex < expectedSummary->effectCount; ++effectIndex) {
                const SZrFunctionModuleEffect *expectedEffect = &expectedSummary->effects[effectIndex];
                const SZrIoFunctionModuleEffect *actualEffect = &actualSummary->effects[effectIndex];

                TEST_ASSERT_EQUAL_UINT8(expectedEffect->kind, actualEffect->kind);
                TEST_ASSERT_EQUAL_UINT8(expectedEffect->exportKind, actualEffect->exportKind);
                TEST_ASSERT_EQUAL_UINT8(expectedEffect->readiness, actualEffect->readiness);
                TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->moduleName,
                                                       ZrCore_String_GetNativeString(expectedEffect->moduleName)));
                TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->symbolName,
                                                       ZrCore_String_GetNativeString(expectedEffect->symbolName)));
            }
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->topLevelCallableBindingLength,
                                 (TZrUInt32)binaryEntry->topLevelCallableBindingsLength);
        for (index = 0; index < sourceFunction->topLevelCallableBindingLength; ++index) {
            const SZrFunctionTopLevelCallableBinding *expectedBinding = &sourceFunction->topLevelCallableBindings[index];
            const SZrIoFunctionTopLevelCallableBinding *actualBinding = &binaryEntry->topLevelCallableBindings[index];

            TEST_ASSERT_TRUE(string_equals_cstring(actualBinding->name,
                                                   ZrCore_String_GetNativeString(expectedBinding->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->stackSlot, actualBinding->stackSlot);
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->callableChildIndex, actualBinding->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->accessModifier, actualBinding->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->exportKind, actualBinding->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->readiness, actualBinding->readiness);
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->exportedVariableLength, (TZrUInt32)binaryEntry->exportedVariablesLength);
        for (index = 0; index < sourceFunction->exportedVariableLength; ++index) {
            const SZrFunctionExportedVariable *expectedExport = &sourceFunction->exportedVariables[index];
            const SZrIoFunctionExportedVariable *actualExport = &binaryEntry->exportedVariables[index];

            TEST_ASSERT_TRUE(string_equals_cstring(actualExport->name,
                                                   ZrCore_String_GetNativeString(expectedExport->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedExport->stackSlot, actualExport->stackSlot);
            TEST_ASSERT_EQUAL_UINT8(expectedExport->accessModifier, actualExport->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedExport->exportKind, actualExport->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedExport->readiness, actualExport->readiness);
            TEST_ASSERT_EQUAL_UINT32(expectedExport->callableChildIndex, actualExport->callableChildIndex);
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->typedExportedSymbolLength,
                                 (TZrUInt32)binaryEntry->typedExportedSymbolsLength);
        for (index = 0; index < sourceFunction->typedExportedSymbolLength; ++index) {
            const SZrFunctionTypedExportSymbol *expectedSymbol = &sourceFunction->typedExportedSymbols[index];
            const SZrIoFunctionTypedExportSymbol *actualSymbol = &binaryEntry->typedExportedSymbols[index];

            TEST_ASSERT_TRUE(string_equals_cstring(actualSymbol->name,
                                                   ZrCore_String_GetNativeString(expectedSymbol->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->stackSlot, actualSymbol->stackSlot);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->accessModifier, actualSymbol->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->symbolKind, actualSymbol->symbolKind);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->exportKind, actualSymbol->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->readiness, actualSymbol->readiness);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->callableChildIndex, actualSymbol->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->parameterCount, (TZrUInt32)actualSymbol->parameterCount);
        }

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->staticImportLength, runtimeFunction->staticImportLength);
        for (index = 0; index < sourceFunction->staticImportLength; ++index) {
            TEST_ASSERT_TRUE(string_equals_cstring(runtimeFunction->staticImports[index],
                                                   ZrCore_String_GetNativeString(sourceFunction->staticImports[index])));
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->moduleEntryEffectLength, runtimeFunction->moduleEntryEffectLength);
        for (index = 0; index < sourceFunction->moduleEntryEffectLength; ++index) {
            const SZrFunctionModuleEffect *expectedEffect = &sourceFunction->moduleEntryEffects[index];
            const SZrFunctionModuleEffect *actualEffect = &runtimeFunction->moduleEntryEffects[index];

            TEST_ASSERT_EQUAL_UINT8(expectedEffect->kind, actualEffect->kind);
            TEST_ASSERT_EQUAL_UINT8(expectedEffect->exportKind, actualEffect->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedEffect->readiness, actualEffect->readiness);
            TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->moduleName,
                                                   ZrCore_String_GetNativeString(expectedEffect->moduleName)));
            TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->symbolName,
                                                   ZrCore_String_GetNativeString(expectedEffect->symbolName)));
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->exportedCallableSummaryLength,
                                 runtimeFunction->exportedCallableSummaryLength);
        for (index = 0; index < sourceFunction->exportedCallableSummaryLength; ++index) {
            const SZrFunctionCallableSummary *expectedSummary = &sourceFunction->exportedCallableSummaries[index];
            const SZrFunctionCallableSummary *actualSummary = &runtimeFunction->exportedCallableSummaries[index];
            TZrUInt32 effectIndex;

            TEST_ASSERT_TRUE(string_equals_cstring(actualSummary->name,
                                                   ZrCore_String_GetNativeString(expectedSummary->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedSummary->callableChildIndex, actualSummary->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT32(expectedSummary->effectCount, actualSummary->effectCount);
            for (effectIndex = 0; effectIndex < expectedSummary->effectCount; ++effectIndex) {
                const SZrFunctionModuleEffect *expectedEffect = &expectedSummary->effects[effectIndex];
                const SZrFunctionModuleEffect *actualEffect = &actualSummary->effects[effectIndex];

                TEST_ASSERT_EQUAL_UINT8(expectedEffect->kind, actualEffect->kind);
                TEST_ASSERT_EQUAL_UINT8(expectedEffect->exportKind, actualEffect->exportKind);
                TEST_ASSERT_EQUAL_UINT8(expectedEffect->readiness, actualEffect->readiness);
                TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->moduleName,
                                                       ZrCore_String_GetNativeString(expectedEffect->moduleName)));
                TEST_ASSERT_TRUE(string_equals_cstring(actualEffect->symbolName,
                                                       ZrCore_String_GetNativeString(expectedEffect->symbolName)));
            }
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->topLevelCallableBindingLength,
                                 runtimeFunction->topLevelCallableBindingLength);
        for (index = 0; index < sourceFunction->topLevelCallableBindingLength; ++index) {
            const SZrFunctionTopLevelCallableBinding *expectedBinding = &sourceFunction->topLevelCallableBindings[index];
            const SZrFunctionTopLevelCallableBinding *actualBinding = &runtimeFunction->topLevelCallableBindings[index];

            TEST_ASSERT_TRUE(string_equals_cstring(actualBinding->name,
                                                   ZrCore_String_GetNativeString(expectedBinding->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->stackSlot, actualBinding->stackSlot);
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->callableChildIndex, actualBinding->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->accessModifier, actualBinding->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->exportKind, actualBinding->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->readiness, actualBinding->readiness);
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->exportedVariableLength, runtimeFunction->exportedVariableLength);
        for (index = 0; index < sourceFunction->exportedVariableLength; ++index) {
            const SZrFunctionExportedVariable *expectedExport = &sourceFunction->exportedVariables[index];
            const SZrFunctionExportedVariable *actualExport = &runtimeFunction->exportedVariables[index];

            TEST_ASSERT_TRUE(string_equals_cstring(actualExport->name,
                                                   ZrCore_String_GetNativeString(expectedExport->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedExport->stackSlot, actualExport->stackSlot);
            TEST_ASSERT_EQUAL_UINT8(expectedExport->accessModifier, actualExport->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedExport->exportKind, actualExport->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedExport->readiness, actualExport->readiness);
            TEST_ASSERT_EQUAL_UINT32(expectedExport->callableChildIndex, actualExport->callableChildIndex);
        }

        TEST_ASSERT_EQUAL_UINT32(sourceFunction->typedExportedSymbolLength, runtimeFunction->typedExportedSymbolLength);
        for (index = 0; index < sourceFunction->typedExportedSymbolLength; ++index) {
            const SZrFunctionTypedExportSymbol *expectedSymbol = &sourceFunction->typedExportedSymbols[index];
            const SZrFunctionTypedExportSymbol *actualSymbol = &runtimeFunction->typedExportedSymbols[index];

            TEST_ASSERT_TRUE(string_equals_cstring(actualSymbol->name,
                                                   ZrCore_String_GetNativeString(expectedSymbol->name)));
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->stackSlot, actualSymbol->stackSlot);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->accessModifier, actualSymbol->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->symbolKind, actualSymbol->symbolKind);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->exportKind, actualSymbol->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->readiness, actualSymbol->readiness);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->callableChildIndex, actualSymbol->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->parameterCount, actualSymbol->parameterCount);
        }

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Function_Free(state, sourceFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(intermediateText);
        free(binaryBytes);
        remove(intermediatePath);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_vm_call_module_export_executes_nested_native_export(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Vm CallModuleExport Executes Nested Native Export";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrTypeValue directArgument;
        SZrTypeValue directResult;
        const TZrChar *source =
                "var system = %import(\"zr.system\");\n"
                "return system.vm.callModuleExport(\"zr.math\", \"sqrt\", [4.0]);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibMath_Register(state->global));

        ZrLib_Value_SetFloat(state, &directArgument, 4.0);
        TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state, "zr.math", "sqrt", &directArgument, 1, &directResult));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(directResult.type));

        sourceName = ZrCore_String_Create(state, "system_vm_call_module_export_test.zr", 37);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(result.type));
        TEST_ASSERT_EQUAL_INT(directResult.type, result.type);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_vector3_constructor_binds_all_numeric_arguments_at_runtime(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Vector3 Constructor Binds Numeric Arguments";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var math = %import(\"zr.math\");\n"
                "var seed = 2.0;\n"
                "return $math.Vector3(seed, seed + 1.0, seed + 2.0);\n";
        const TZrChar *sourceNameText = "native_vector3_constructor_runtime_test.zr";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *vectorObject;
        const SZrTypeValue *xValue;
        const SZrTypeValue *yValue;
        const SZrTypeValue *zValue;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibMath_Register(state->global));

        sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        vectorObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(vectorObject);

        xValue = get_object_field_value(state, vectorObject, "x");
        yValue = get_object_field_value(state, vectorObject, "y");
        zValue = get_object_field_value(state, vectorObject, "z");

        TEST_ASSERT_NOT_NULL(xValue);
        TEST_ASSERT_NOT_NULL(yValue);
        TEST_ASSERT_NOT_NULL(zValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(xValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(yValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(zValue->type));
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 2.0, xValue->value.nativeObject.nativeDouble);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.0, yValue->value.nativeObject.nativeDouble);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 4.0, zValue->value.nativeObject.nativeDouble);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_root_aggregates_leaf_modules_and_reuses_cached_instances(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Root Aggregates Leaf Modules";
    static const struct {
        const TZrChar *exportName;
        const TZrChar *moduleName;
    } kExpectedModules[] = {
            {"console", "zr.system.console"},
            {"fs", "zr.system.fs"},
            {"env", "zr.system.env"},
            {"process", "zr.system.process"},
            {"gc", "zr.system.gc"},
            {"exception", "zr.system.exception"},
            {"vm", "zr.system.vm"},
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *rootModule;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        rootModule = import_native_module(state, "zr.system");
        TEST_ASSERT_NOT_NULL(rootModule);

        for (index = 0; index < ZR_ARRAY_COUNT(kExpectedModules); index++) {
            SZrObjectModule *leafModule = import_native_module(state, kExpectedModules[index].moduleName);
            const SZrTypeValue *exportValue = get_module_export_value(state, rootModule, kExpectedModules[index].exportName);
            SZrObject *exportObject;

            TEST_ASSERT_NOT_NULL(leafModule);
            TEST_ASSERT_NOT_NULL(exportValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, exportValue->type);

            exportObject = ZR_CAST_OBJECT(state, exportValue->value.object);
            TEST_ASSERT_NOT_NULL(exportObject);
            TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_MODULE, exportObject->internalType);
            TEST_ASSERT_EQUAL_PTR(leafModule, exportObject);
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_root_exports_only_submodules(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Root Exports Only Submodules";
    static const TZrChar *kAbsentExports[] = {
            "printLine",
            "println",
            "writeText",
            "vmState",
            "gcDisable",
            "callModuleExport",
            "SystemFileInfo",
            "SystemVmState",
            "SystemLoadedModuleInfo",
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *rootModule;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        rootModule = import_native_module(state, "zr.system");
        TEST_ASSERT_NOT_NULL(rootModule);

        for (index = 0; index < ZR_ARRAY_COUNT(kAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, rootModule, kAbsentExports[index]));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_leaf_modules_expose_new_api_and_owned_types(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Leaf Modules Expose New API";
    static const TZrChar *kConsoleExports[] = {"print", "printLine", "printError", "printErrorLine", "read", "readLine"};
    static const struct {
        const TZrChar *symbolName;
        const TZrChar *signature;
    } kConsoleTypeHints[] = {
            {"print", "print(value: any): null"},
            {"printLine", "printLine(value: any): null"},
            {"printError", "printError(value: any): null"},
            {"printErrorLine", "printErrorLine(value: any): null"},
            {"read", "read(): string?"},
            {"readLine", "readLine(): string?"},
    };
    static const TZrChar *kFsExports[] = {
            "currentDirectory",
            "changeCurrentDirectory",
            "pathExists",
            "isFile",
            "isDirectory",
            "createDirectory",
            "createDirectories",
            "removePath",
            "readText",
            "writeText",
            "appendText",
            "getInfo",
            "SystemFileInfo",
    };
    static const TZrChar *kEnvExports[] = {"getVariable"};
    static const TZrChar *kGcExports[] = {
            "enable",
            "disable",
            "collect",
            "set_heap_limit",
            "set_budget",
            "get_stats",
            "SystemGcStats",
    };
    static const TZrChar *kGcAbsentExports[] = {"start", "stop", "step"};
    static const struct {
        const TZrChar *symbolName;
        const TZrChar *signature;
    } kGcTypeHints[] = {
            {"enable", "enable(): null"},
            {"disable", "disable(): null"},
            {"collect", "collect(kind: string = \"full\"): null"},
            {"set_heap_limit", "set_heap_limit(bytes: int): null"},
            {"set_budget", "set_budget(microseconds: int): null"},
            {"get_stats", "get_stats(): SystemGcStats"},
            {"SystemGcStats",
             "struct SystemGcStats { enabled, heapLimitBytes, managedMemoryBytes, gcDebtBytes, pauseBudgetUs, "
             "remarkBudgetUs, workerCount, ignoredObjectCount, rememberedObjectCount, regionCount, "
             "edenRegionCount, survivorRegionCount, oldRegionCount, pinnedRegionCount, largeRegionCount, "
             "permanentRegionCount, edenUsedBytes, survivorUsedBytes, oldUsedBytes, pinnedUsedBytes, "
             "largeUsedBytes, permanentUsedBytes, edenLiveBytes, survivorLiveBytes, oldLiveBytes, "
             "pinnedLiveBytes, largeLiveBytes, permanentLiveBytes, lastStepDurationUs, lastStepWork, "
             "lastCollectionKind, "
             "lastRequestedCollectionKind, collectionPhase, minorCollectionCount, majorCollectionCount, "
             "fullCollectionCount, minorCollectionTotalDurationUs, majorCollectionTotalDurationUs, "
             "fullCollectionTotalDurationUs, minorCollectionMaxDurationUs, majorCollectionMaxDurationUs, "
             "fullCollectionMaxDurationUs }"},
    };
    static const TZrChar *kExceptionExports[] = {
            "registerUnhandledException",
            "Error",
            "StackFrame",
            "RuntimeError",
            "TypeError",
            "MemoryError",
            "ExceptionError",
    };
    static const TZrChar *kVmExports[] = {"loadedModules", "state", "callModuleExport", "SystemVmState", "SystemLoadedModuleInfo"};
    static const TZrChar *kConsoleAbsentExports[] = {"println", "eprint", "eprintln"};
    static const TZrChar *kProcessAbsentExports[] = {"args", "sleepMs"};

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *consoleModule;
        SZrObjectModule *fsModule;
        SZrObjectModule *envModule;
        SZrObjectModule *processModule;
        SZrObjectModule *gcModule;
        SZrObjectModule *exceptionModule;
        SZrObjectModule *vmModule;
        const SZrTypeValue *consoleModuleInfoValue;
        const SZrTypeValue *consoleTypeHintsValue;
        const SZrTypeValue *gcModuleInfoValue;
        const SZrTypeValue *gcTypeHintsValue;
        const SZrTypeValue *argumentsValue;
        SZrObject *consoleModuleInfo;
        SZrObject *consoleTypeHints;
        SZrObject *gcModuleInfo;
        SZrObject *gcTypeHints;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        consoleModule = import_native_module(state, "zr.system.console");
        fsModule = import_native_module(state, "zr.system.fs");
        envModule = import_native_module(state, "zr.system.env");
        processModule = import_native_module(state, "zr.system.process");
        gcModule = import_native_module(state, "zr.system.gc");
        exceptionModule = import_native_module(state, "zr.system.exception");
        vmModule = import_native_module(state, "zr.system.vm");

        TEST_ASSERT_NOT_NULL(consoleModule);
        TEST_ASSERT_NOT_NULL(fsModule);
        TEST_ASSERT_NOT_NULL(envModule);
        TEST_ASSERT_NOT_NULL(processModule);
        TEST_ASSERT_NOT_NULL(gcModule);
        TEST_ASSERT_NOT_NULL(exceptionModule);
        TEST_ASSERT_NOT_NULL(vmModule);

        for (index = 0; index < ZR_ARRAY_COUNT(kConsoleExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, consoleModule, kConsoleExports[index]));
        }
        for (index = 0; index < ZR_ARRAY_COUNT(kConsoleAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, consoleModule, kConsoleAbsentExports[index]));
        }

        consoleModuleInfoValue = get_module_export_value(state, consoleModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(consoleModuleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, consoleModuleInfoValue->type);
        consoleModuleInfo = ZR_CAST_OBJECT(state, consoleModuleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(consoleModuleInfo);

        consoleTypeHintsValue = get_object_field_value(state, consoleModuleInfo, "typeHints");
        TEST_ASSERT_NOT_NULL(consoleTypeHintsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, consoleTypeHintsValue->type);
        consoleTypeHints = ZR_CAST_OBJECT(state, consoleTypeHintsValue->value.object);
        TEST_ASSERT_NOT_NULL(consoleTypeHints);
        TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(kConsoleTypeHints), get_array_length(consoleTypeHints));

        for (index = 0; index < ZR_ARRAY_COUNT(kConsoleTypeHints); index++) {
            SZrObject *hintEntry = find_named_entry_in_array(state,
                                                             consoleTypeHints,
                                                             "symbolName",
                                                             kConsoleTypeHints[index].symbolName);
            const SZrTypeValue *signatureValue;

            TEST_ASSERT_NOT_NULL(hintEntry);
            signatureValue = get_object_field_value(state, hintEntry, "signature");
            TEST_ASSERT_NOT_NULL(signatureValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, signatureValue->type);
            TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, signatureValue->value.object),
                                                   kConsoleTypeHints[index].signature));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kFsExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, fsModule, kFsExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kEnvExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, envModule, kEnvExports[index]));
        }

        argumentsValue = get_module_export_value(state, processModule, "arguments");
        TEST_ASSERT_NOT_NULL(argumentsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, argumentsValue->type);
        TEST_ASSERT_NOT_NULL(get_module_export_value(state, processModule, "sleepMilliseconds"));
        TEST_ASSERT_NOT_NULL(get_module_export_value(state, processModule, "exit"));
        for (index = 0; index < ZR_ARRAY_COUNT(kProcessAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, processModule, kProcessAbsentExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kGcExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, gcModule, kGcExports[index]));
        }
        for (index = 0; index < ZR_ARRAY_COUNT(kGcAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, gcModule, kGcAbsentExports[index]));
        }

        gcModuleInfoValue = get_module_export_value(state, gcModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(gcModuleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, gcModuleInfoValue->type);
        gcModuleInfo = ZR_CAST_OBJECT(state, gcModuleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(gcModuleInfo);

        gcTypeHintsValue = get_object_field_value(state, gcModuleInfo, "typeHints");
        TEST_ASSERT_NOT_NULL(gcTypeHintsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, gcTypeHintsValue->type);
        gcTypeHints = ZR_CAST_OBJECT(state, gcTypeHintsValue->value.object);
        TEST_ASSERT_NOT_NULL(gcTypeHints);
        TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(kGcTypeHints), get_array_length(gcTypeHints));

        for (index = 0; index < ZR_ARRAY_COUNT(kGcTypeHints); index++) {
            SZrObject *hintEntry = find_named_entry_in_array(state,
                                                             gcTypeHints,
                                                             "symbolName",
                                                             kGcTypeHints[index].symbolName);
            const SZrTypeValue *signatureValue;

            TEST_ASSERT_NOT_NULL(hintEntry);
            signatureValue = get_object_field_value(state, hintEntry, "signature");
            TEST_ASSERT_NOT_NULL(signatureValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, signatureValue->type);
            TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, signatureValue->value.object),
                                                   kGcTypeHints[index].signature));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kExceptionExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, exceptionModule, kExceptionExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kVmExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, vmModule, kVmExports[index]));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_native_types_register_complete_struct_fields(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Native Types Register Complete Struct Fields";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *fsModule;
        SZrObjectModule *gcModule;
        SZrObjectModule *vmModule;
        SZrObjectPrototype *fileInfoPrototype;
        SZrObjectPrototype *gcStatsPrototype;
        SZrObjectPrototype *vmStatePrototype;
        SZrObjectPrototype *loadedModuleInfoPrototype;
        TZrUInt64 offset = 0;

        TEST_ASSERT_NOT_NULL(state);

        fsModule = import_native_module(state, "zr.system.fs");
        gcModule = import_native_module(state, "zr.system.gc");
        vmModule = import_native_module(state, "zr.system.vm");
        TEST_ASSERT_NOT_NULL(fsModule);
        TEST_ASSERT_NOT_NULL(gcModule);
        TEST_ASSERT_NOT_NULL(vmModule);

        fileInfoPrototype = get_module_exported_prototype(state, fsModule, "SystemFileInfo");
        gcStatsPrototype = get_module_exported_prototype(state, gcModule, "SystemGcStats");
        vmStatePrototype = get_module_exported_prototype(state, vmModule, "SystemVmState");
        loadedModuleInfoPrototype = get_module_exported_prototype(state, vmModule, "SystemLoadedModuleInfo");

        TEST_ASSERT_NOT_NULL(fileInfoPrototype);
        TEST_ASSERT_NOT_NULL(gcStatsPrototype);
        TEST_ASSERT_NOT_NULL(vmStatePrototype);
        TEST_ASSERT_NOT_NULL(loadedModuleInfoPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, fileInfoPrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, gcStatsPrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, vmStatePrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, loadedModuleInfoPrototype->type);

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "path", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "size", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "isFile", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "isDirectory", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "modifiedMilliseconds", &offset));

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "enabled", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "heapLimitBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "managedMemoryBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "gcDebtBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "pauseBudgetUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "remarkBudgetUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "workerCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "ignoredObjectCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "rememberedObjectCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "regionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "edenRegionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "survivorRegionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "oldRegionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "pinnedRegionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "largeRegionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "permanentRegionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "edenUsedBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "survivorUsedBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "oldUsedBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "pinnedUsedBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "largeUsedBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "permanentUsedBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "edenLiveBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "survivorLiveBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "oldLiveBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "pinnedLiveBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "largeLiveBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "permanentLiveBytes", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "lastStepDurationUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "lastStepWork", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "lastCollectionKind", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "lastRequestedCollectionKind", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "collectionPhase", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "minorCollectionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "majorCollectionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "fullCollectionCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "minorCollectionTotalDurationUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "majorCollectionTotalDurationUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "fullCollectionTotalDurationUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "minorCollectionMaxDurationUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "majorCollectionMaxDurationUs", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)gcStatsPrototype, "fullCollectionMaxDurationUs", &offset));

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "loadedModuleCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "garbageCollectionMode", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "garbageCollectionDebt", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "garbageCollectionThreshold", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "stackDepth", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "frameDepth", &offset));

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "name", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "sourceKind", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "sourcePath", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "registrationKind", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "hasTypeHints", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "moduleVersion", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "runtimeAbiVersion", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "requiredCapabilities", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "isDescriptorPlugin", &offset));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_root_native_module_info_exposes_module_links(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Root Native Module Info Exposes Module Links";
    static const struct {
        const TZrChar *name;
        const TZrChar *moduleName;
    } kExpectedModules[] = {
            {"console", "zr.system.console"},
            {"fs", "zr.system.fs"},
            {"env", "zr.system.env"},
            {"process", "zr.system.process"},
            {"gc", "zr.system.gc"},
            {"exception", "zr.system.exception"},
            {"vm", "zr.system.vm"},
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *rootModule;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *functionsValue;
        const SZrTypeValue *constantsValue;
        const SZrTypeValue *typesValue;
        const SZrTypeValue *modulesValue;
        SZrObject *moduleInfo;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        rootModule = import_native_module(state, "zr.system");
        TEST_ASSERT_NOT_NULL(rootModule);

        moduleInfoValue = get_module_export_value(state, rootModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        functionsValue = get_object_field_value(state, moduleInfo, "functions");
        constantsValue = get_object_field_value(state, moduleInfo, "constants");
        typesValue = get_object_field_value(state, moduleInfo, "types");
        modulesValue = get_object_field_value(state, moduleInfo, "modules");

        TEST_ASSERT_NOT_NULL(functionsValue);
        TEST_ASSERT_NOT_NULL(constantsValue);
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_NOT_NULL(modulesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, functionsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constantsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, modulesValue->type);
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, functionsValue->value.object)));
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, constantsValue->value.object)));
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, typesValue->value.object)));
        TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(kExpectedModules), get_array_length(ZR_CAST_OBJECT(state, modulesValue->value.object)));

        for (index = 0; index < ZR_ARRAY_COUNT(kExpectedModules); index++) {
            SZrObject *entry = find_named_entry_in_array(state,
                                                         ZR_CAST_OBJECT(state, modulesValue->value.object),
                                                         "name",
                                                         kExpectedModules[index].name);
            const SZrTypeValue *moduleNameValue;

            TEST_ASSERT_NOT_NULL(entry);
            moduleNameValue = get_object_field_value(state, entry, "moduleName");
            TEST_ASSERT_NOT_NULL(moduleNameValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, moduleNameValue->type);
            TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, moduleNameValue->value.object),
                                                   kExpectedModules[index].moduleName));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_module_info_exposes_enum_and_interface_descriptors(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Module Info Exposes Enum And Interface Descriptors";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *typesValue;
        SZrObject *moduleInfo;
        SZrObject *typesArray;
        SZrObject *readableEntry;
        SZrObject *streamReadableEntry;
        SZrObject *enumEntry;
        SZrObject *deviceEntry;
        const SZrTypeValue *prototypeTypeValue;
        const SZrTypeValue *allowValueValue;
        const SZrTypeValue *allowBoxedValue;
        const SZrTypeValue *extendsValue;
        const SZrTypeValue *constructorSignatureValue;
        const SZrTypeValue *implementsValue;
        const SZrTypeValue *enumMembersValue;
        const SZrTypeValue *enumValueTypeValue;
        const SZrTypeValue *firstImplementValue;
        SZrObject *enumMembersArray;
        SZrObject *onMemberEntry;
        const SZrTypeValue *onMemberIntValue;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        typesValue = get_object_field_value(state, moduleInfo, "types");
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);

        typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
        TEST_ASSERT_NOT_NULL(typesArray);
        TEST_ASSERT_EQUAL_UINT64(8, get_array_length(typesArray));

        readableEntry = find_named_entry_in_array(state, typesArray, "name", "NativeReadable");
        streamReadableEntry = find_named_entry_in_array(state, typesArray, "name", "NativeStreamReadable");
        enumEntry = find_named_entry_in_array(state, typesArray, "name", "NativeMode");
        deviceEntry = find_named_entry_in_array(state, typesArray, "name", "NativeDevice");

        TEST_ASSERT_NOT_NULL(readableEntry);
        TEST_ASSERT_NOT_NULL(streamReadableEntry);
        TEST_ASSERT_NOT_NULL(enumEntry);
        TEST_ASSERT_NOT_NULL(deviceEntry);

        prototypeTypeValue = get_object_field_value(state, readableEntry, "prototypeType");
        allowValueValue = get_object_field_value(state, readableEntry, "allowValueConstruction");
        allowBoxedValue = get_object_field_value(state, readableEntry, "allowBoxedConstruction");
        constructorSignatureValue = get_object_field_value(state, readableEntry, "constructorSignature");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(allowValueValue);
        TEST_ASSERT_NOT_NULL(allowBoxedValue);
        TEST_ASSERT_NOT_NULL(constructorSignatureValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, allowValueValue->type);
        TEST_ASSERT_FALSE(allowValueValue->value.nativeObject.nativeBool);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, allowBoxedValue->type);
        TEST_ASSERT_FALSE(allowBoxedValue->value.nativeObject.nativeBool);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, constructorSignatureValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, constructorSignatureValue->value.object),
                                               "NativeReadable()"));

        prototypeTypeValue = get_object_field_value(state, streamReadableEntry, "prototypeType");
        extendsValue = get_object_field_value(state, streamReadableEntry, "extendsTypeName");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(extendsValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, extendsValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, extendsValue->value.object), "NativeReadable"));

        prototypeTypeValue = get_object_field_value(state, enumEntry, "prototypeType");
        allowValueValue = get_object_field_value(state, enumEntry, "allowValueConstruction");
        allowBoxedValue = get_object_field_value(state, enumEntry, "allowBoxedConstruction");
        constructorSignatureValue = get_object_field_value(state, enumEntry, "constructorSignature");
        enumMembersValue = get_object_field_value(state, enumEntry, "enumMembers");
        enumValueTypeValue = get_object_field_value(state, enumEntry, "enumValueTypeName");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(allowValueValue);
        TEST_ASSERT_NOT_NULL(allowBoxedValue);
        TEST_ASSERT_NOT_NULL(constructorSignatureValue);
        TEST_ASSERT_NOT_NULL(enumMembersValue);
        TEST_ASSERT_NOT_NULL(enumValueTypeValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_ENUM, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(allowValueValue->value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(allowBoxedValue->value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, constructorSignatureValue->value.object),
                                               "NativeMode(value: int)"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumValueTypeValue->value.object), "int"));

        enumMembersArray = ZR_CAST_OBJECT(state, enumMembersValue->value.object);
        TEST_ASSERT_NOT_NULL(enumMembersArray);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(enumMembersArray));
        onMemberEntry = find_named_entry_in_array(state, enumMembersArray, "name", "On");
        TEST_ASSERT_NOT_NULL(onMemberEntry);
        onMemberIntValue = get_object_field_value(state, onMemberEntry, "intValue");
        TEST_ASSERT_NOT_NULL(onMemberIntValue);
        TEST_ASSERT_EQUAL_INT64(1, onMemberIntValue->value.nativeObject.nativeInt64);

        prototypeTypeValue = get_object_field_value(state, deviceEntry, "prototypeType");
        implementsValue = get_object_field_value(state, deviceEntry, "implements");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(implementsValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_CLASS, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, implementsValue->type);
        TEST_ASSERT_EQUAL_UINT64(1, get_array_length(ZR_CAST_OBJECT(state, implementsValue->value.object)));
        firstImplementValue = get_array_entry_value(state, ZR_CAST_OBJECT(state, implementsValue->value.object), 0);
        TEST_ASSERT_NOT_NULL(firstImplementValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, firstImplementValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstImplementValue->value.object),
                                               "NativeStreamReadable"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_gc_safe_controls_report_runtime_stats(void) {
    SZrTestTimer timer;
    const char *testSummary = "System GC Safe Controls Report Runtime Stats";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var system = %import(\"zr.system\");\n"
                "system.gc.enable();\n"
                "system.gc.set_heap_limit(4096);\n"
                "system.gc.set_budget(1234);\n"
                "var enabledStats = system.gc.get_stats();\n"
                "system.gc.disable();\n"
                "var disabledStats = system.gc.get_stats();\n"
                "system.gc.enable();\n"
                "system.gc.collect(\"minor\");\n"
                "system.gc.collect(\"major\");\n"
                "var majorStats = system.gc.get_stats();\n"
                "system.gc.collect(\"full\");\n"
                "var finalStats = system.gc.get_stats();\n"
                "return {\n"
                "    enabledBeforeDisable: enabledStats.enabled,\n"
                "    disabledAfterCall: disabledStats.enabled,\n"
                "    heapLimitBytes: finalStats.heapLimitBytes,\n"
                "    managedMemoryBytes: finalStats.managedMemoryBytes,\n"
                "    gcDebtBytes: finalStats.gcDebtBytes,\n"
                "    pauseBudgetUs: finalStats.pauseBudgetUs,\n"
                "    remarkBudgetUs: finalStats.remarkBudgetUs,\n"
                "    regionCount: finalStats.regionCount,\n"
                "    edenRegionCount: finalStats.edenRegionCount,\n"
                "    oldRegionCount: finalStats.oldRegionCount,\n"
                "    pinnedRegionCount: finalStats.pinnedRegionCount,\n"
                "    permanentRegionCount: finalStats.permanentRegionCount,\n"
                "    permanentLiveBytes: finalStats.permanentLiveBytes,\n"
                "    majorRequestedCollectionKind: majorStats.lastRequestedCollectionKind,\n"
                "    lastRequestedCollectionKind: finalStats.lastRequestedCollectionKind,\n"
                "    minorCollectionCount: finalStats.minorCollectionCount,\n"
                "    majorCollectionCount: finalStats.majorCollectionCount,\n"
                "    fullCollectionCount: finalStats.fullCollectionCount,\n"
                "    fullCollectionTotalDurationUs: finalStats.fullCollectionTotalDurationUs,\n"
                "    fullCollectionMaxDurationUs: finalStats.fullCollectionMaxDurationUs\n"
                "};\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;
        const SZrTypeValue *enabledBeforeDisableValue;
        const SZrTypeValue *disabledAfterCallValue;
        const SZrTypeValue *heapLimitBytesValue;
        const SZrTypeValue *managedMemoryBytesValue;
        const SZrTypeValue *gcDebtBytesValue;
        const SZrTypeValue *pauseBudgetUsValue;
        const SZrTypeValue *remarkBudgetUsValue;
        const SZrTypeValue *regionCountValue;
        const SZrTypeValue *edenRegionCountValue;
        const SZrTypeValue *oldRegionCountValue;
        const SZrTypeValue *pinnedRegionCountValue;
        const SZrTypeValue *permanentRegionCountValue;
        const SZrTypeValue *permanentLiveBytesValue;
        const SZrTypeValue *majorRequestedCollectionKindValue;
        const SZrTypeValue *lastRequestedCollectionKindValue;
        const SZrTypeValue *minorCollectionCountValue;
        const SZrTypeValue *majorCollectionCountValue;
        const SZrTypeValue *fullCollectionCountValue;
        const SZrTypeValue *fullCollectionTotalDurationUsValue;
        const SZrTypeValue *fullCollectionMaxDurationUsValue;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "system_gc_safe_controls_runtime_test.zr", 39);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        enabledBeforeDisableValue = get_object_field_value(state, resultObject, "enabledBeforeDisable");
        disabledAfterCallValue = get_object_field_value(state, resultObject, "disabledAfterCall");
        heapLimitBytesValue = get_object_field_value(state, resultObject, "heapLimitBytes");
        managedMemoryBytesValue = get_object_field_value(state, resultObject, "managedMemoryBytes");
        gcDebtBytesValue = get_object_field_value(state, resultObject, "gcDebtBytes");
        pauseBudgetUsValue = get_object_field_value(state, resultObject, "pauseBudgetUs");
        remarkBudgetUsValue = get_object_field_value(state, resultObject, "remarkBudgetUs");
        regionCountValue = get_object_field_value(state, resultObject, "regionCount");
        edenRegionCountValue = get_object_field_value(state, resultObject, "edenRegionCount");
        oldRegionCountValue = get_object_field_value(state, resultObject, "oldRegionCount");
        pinnedRegionCountValue = get_object_field_value(state, resultObject, "pinnedRegionCount");
        permanentRegionCountValue = get_object_field_value(state, resultObject, "permanentRegionCount");
        permanentLiveBytesValue = get_object_field_value(state, resultObject, "permanentLiveBytes");
        majorRequestedCollectionKindValue = get_object_field_value(state, resultObject, "majorRequestedCollectionKind");
        lastRequestedCollectionKindValue = get_object_field_value(state, resultObject, "lastRequestedCollectionKind");
        minorCollectionCountValue = get_object_field_value(state, resultObject, "minorCollectionCount");
        majorCollectionCountValue = get_object_field_value(state, resultObject, "majorCollectionCount");
        fullCollectionCountValue = get_object_field_value(state, resultObject, "fullCollectionCount");
        fullCollectionTotalDurationUsValue = get_object_field_value(state, resultObject, "fullCollectionTotalDurationUs");
        fullCollectionMaxDurationUsValue = get_object_field_value(state, resultObject, "fullCollectionMaxDurationUs");

        TEST_ASSERT_NOT_NULL(enabledBeforeDisableValue);
        TEST_ASSERT_NOT_NULL(disabledAfterCallValue);
        TEST_ASSERT_NOT_NULL(heapLimitBytesValue);
        TEST_ASSERT_NOT_NULL(managedMemoryBytesValue);
        TEST_ASSERT_NOT_NULL(gcDebtBytesValue);
        TEST_ASSERT_NOT_NULL(pauseBudgetUsValue);
        TEST_ASSERT_NOT_NULL(remarkBudgetUsValue);
        TEST_ASSERT_NOT_NULL(regionCountValue);
        TEST_ASSERT_NOT_NULL(edenRegionCountValue);
        TEST_ASSERT_NOT_NULL(oldRegionCountValue);
        TEST_ASSERT_NOT_NULL(pinnedRegionCountValue);
        TEST_ASSERT_NOT_NULL(permanentRegionCountValue);
        TEST_ASSERT_NOT_NULL(permanentLiveBytesValue);
        TEST_ASSERT_NOT_NULL(majorRequestedCollectionKindValue);
        TEST_ASSERT_NOT_NULL(lastRequestedCollectionKindValue);
        TEST_ASSERT_NOT_NULL(minorCollectionCountValue);
        TEST_ASSERT_NOT_NULL(majorCollectionCountValue);
        TEST_ASSERT_NOT_NULL(fullCollectionCountValue);
        TEST_ASSERT_NOT_NULL(fullCollectionTotalDurationUsValue);
        TEST_ASSERT_NOT_NULL(fullCollectionMaxDurationUsValue);

        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, enabledBeforeDisableValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, disabledAfterCallValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, heapLimitBytesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, managedMemoryBytesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, gcDebtBytesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pauseBudgetUsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, remarkBudgetUsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, regionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, edenRegionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, oldRegionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, pinnedRegionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, permanentRegionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, permanentLiveBytesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, majorRequestedCollectionKindValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, lastRequestedCollectionKindValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, minorCollectionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, majorCollectionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, fullCollectionCountValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, fullCollectionTotalDurationUsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, fullCollectionMaxDurationUsValue->type);

        TEST_ASSERT_TRUE(enabledBeforeDisableValue->value.nativeObject.nativeBool);
        TEST_ASSERT_FALSE(disabledAfterCallValue->value.nativeObject.nativeBool);
        TEST_ASSERT_EQUAL_INT64(4096, heapLimitBytesValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(managedMemoryBytesValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(gcDebtBytesValue->value.nativeObject.nativeInt64 >= 0);
        TEST_ASSERT_EQUAL_INT64(1234, pauseBudgetUsValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1234, remarkBudgetUsValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(regionCountValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(edenRegionCountValue->value.nativeObject.nativeInt64 >= 0);
        TEST_ASSERT_TRUE(oldRegionCountValue->value.nativeObject.nativeInt64 >= 0);
        TEST_ASSERT_TRUE(pinnedRegionCountValue->value.nativeObject.nativeInt64 >= 0);
        TEST_ASSERT_TRUE(permanentRegionCountValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(permanentLiveBytesValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(regionCountValue->value.nativeObject.nativeInt64 >=
                         permanentRegionCountValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1, majorRequestedCollectionKindValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(2, lastRequestedCollectionKindValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1, minorCollectionCountValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(majorCollectionCountValue->value.nativeObject.nativeInt64 >= 0);
        TEST_ASSERT_EQUAL_INT64(1, fullCollectionCountValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(fullCollectionTotalDurationUsValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(fullCollectionMaxDurationUsValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(fullCollectionMaxDurationUsValue->value.nativeObject.nativeInt64 <=
                         fullCollectionTotalDurationUsValue->value.nativeObject.nativeInt64);

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_module_info_exposes_ffi_wrapper_lowering_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Module Info Exposes FFI Wrapper Lowering Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *typesValue;
        SZrObject *moduleInfo;
        SZrObject *typesArray;
        SZrObject *pointerEntry;
        SZrObject *bufferEntry;
        const SZrTypeValue *pointerLoweringValue;
        const SZrTypeValue *pointerOwnerModeValue;
        const SZrTypeValue *bufferLoweringValue;
        const SZrTypeValue *bufferOwnerModeValue;
        SZrObjectPrototype *pointerPrototype;
        SZrObjectPrototype *bufferPrototype;
        const SZrTypeValue *pointerPrototypeLoweringValue;
        const SZrTypeValue *bufferPrototypeLoweringValue;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibFfi_Register(state->global));

        module = import_native_module(state, "zr.ffi");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        typesValue = get_object_field_value(state, moduleInfo, "types");
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);

        typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
        TEST_ASSERT_NOT_NULL(typesArray);

        pointerEntry = find_named_entry_in_array(state, typesArray, "name", "PointerHandle");
        bufferEntry = find_named_entry_in_array(state, typesArray, "name", "BufferHandle");
        TEST_ASSERT_NOT_NULL(pointerEntry);
        TEST_ASSERT_NOT_NULL(bufferEntry);

        pointerLoweringValue = get_object_field_value(state, pointerEntry, "ffiLoweringKind");
        pointerOwnerModeValue = get_object_field_value(state, pointerEntry, "ffiOwnerMode");
        bufferLoweringValue = get_object_field_value(state, bufferEntry, "ffiLoweringKind");
        bufferOwnerModeValue = get_object_field_value(state, bufferEntry, "ffiOwnerMode");
        TEST_ASSERT_NOT_NULL(pointerLoweringValue);
        TEST_ASSERT_NOT_NULL(pointerOwnerModeValue);
        TEST_ASSERT_NOT_NULL(bufferLoweringValue);
        TEST_ASSERT_NOT_NULL(bufferOwnerModeValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, pointerLoweringValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, pointerOwnerModeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, bufferLoweringValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, bufferOwnerModeValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, pointerLoweringValue->value.object), "pointer"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, pointerOwnerModeValue->value.object), "borrowed"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, bufferLoweringValue->value.object), "pointer"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, bufferOwnerModeValue->value.object), "owned"));

        pointerPrototype = get_module_exported_prototype(state, module, "PointerHandle");
        bufferPrototype = get_module_exported_prototype(state, module, "BufferHandle");
        TEST_ASSERT_NOT_NULL(pointerPrototype);
        TEST_ASSERT_NOT_NULL(bufferPrototype);

        pointerPrototypeLoweringValue =
                get_object_field_value(state, &pointerPrototype->super, "__zr_ffiLoweringKind");
        bufferPrototypeLoweringValue =
                get_object_field_value(state, &bufferPrototype->super, "__zr_ffiLoweringKind");
        TEST_ASSERT_NOT_NULL(pointerPrototypeLoweringValue);
        TEST_ASSERT_NOT_NULL(bufferPrototypeLoweringValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, pointerPrototypeLoweringValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, bufferPrototypeLoweringValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, pointerPrototypeLoweringValue->value.object),
                                               "pointer"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, bufferPrototypeLoweringValue->value.object),
                                               "pointer"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_wrapper_metadata_exposes_ffi_wrapper_fields_and_runtime_hidden_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_ffi_wrapper_metadata",
                    "%extern(\"fixture\") {\n"
                    "    struct ModeHandleView {\n"
                    "        var raw: i32;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "#zr.ffi.lowering(\"handle_id\")#\n"
                    "#zr.ffi.viewType(\"ModeHandleView\")#\n"
                    "#zr.ffi.underlying(\"i32\")#\n"
                    "#zr.ffi.ownerMode(\"borrowed\")#\n"
                    "#zr.ffi.releaseHook(\"close_mode_handle\")#\n"
                    "pub class ModeHandle {\n"
                    "    pub var handleId: i32 = 1;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source Module Wrapper Metadata Exposes FFI Wrapper Fields And Runtime Hidden Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var wrappers = %import(\"reflect_ffi_wrapper_metadata\");\n"
                "return %type(wrappers.ModeHandle);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *metadataValue;
        SZrObject *metadataObject;
        const SZrTypeValue *loweringValue;
        const SZrTypeValue *viewTypeValue;
        const SZrTypeValue *underlyingValue;
        const SZrTypeValue *ownerModeValue;
        const SZrTypeValue *releaseHookValue;
        SZrString *modulePath;
        SZrObjectModule *module;
        SZrObjectPrototype *prototype;
        const SZrTypeValue *prototypeLoweringValue;
        const SZrTypeValue *prototypeViewTypeValue;
        const SZrTypeValue *prototypeUnderlyingValue;
        const SZrTypeValue *prototypeOwnerModeValue;
        const SZrTypeValue *prototypeReleaseHookValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_ffi_wrapper_metadata_test.zr", 40);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        metadataValue = get_object_field_value(state, reflectionObject, "metadata");
        TEST_ASSERT_NOT_NULL(metadataValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
        metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
        TEST_ASSERT_NOT_NULL(metadataObject);

        loweringValue = get_object_field_value(state, metadataObject, "ffiLoweringKind");
        viewTypeValue = get_object_field_value(state, metadataObject, "ffiViewTypeName");
        underlyingValue = get_object_field_value(state, metadataObject, "ffiUnderlyingTypeName");
        ownerModeValue = get_object_field_value(state, metadataObject, "ffiOwnerMode");
        releaseHookValue = get_object_field_value(state, metadataObject, "ffiReleaseHook");
        TEST_ASSERT_NOT_NULL(loweringValue);
        TEST_ASSERT_NOT_NULL(viewTypeValue);
        TEST_ASSERT_NOT_NULL(underlyingValue);
        TEST_ASSERT_NOT_NULL(ownerModeValue);
        TEST_ASSERT_NOT_NULL(releaseHookValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, loweringValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, viewTypeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, underlyingValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, ownerModeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, releaseHookValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, loweringValue->value.object), "handle_id"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, viewTypeValue->value.object), "ModeHandleView"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, underlyingValue->value.object), "i32"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, ownerModeValue->value.object), "borrowed"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, releaseHookValue->value.object),
                                               "close_mode_handle"));

        modulePath = ZrCore_String_Create(state, "reflect_ffi_wrapper_metadata", 28);
        TEST_ASSERT_NOT_NULL(modulePath);
        module = ZrCore_Module_ImportByPath(state, modulePath);
        TEST_ASSERT_NOT_NULL(module);
        prototype = get_module_exported_prototype(state, module, "ModeHandle");
        TEST_ASSERT_NOT_NULL(prototype);

        prototypeLoweringValue = get_object_field_value(state, &prototype->super, "__zr_ffiLoweringKind");
        prototypeViewTypeValue = get_object_field_value(state, &prototype->super, "__zr_ffiViewTypeName");
        prototypeUnderlyingValue = get_object_field_value(state, &prototype->super, "__zr_ffiUnderlyingTypeName");
        prototypeOwnerModeValue = get_object_field_value(state, &prototype->super, "__zr_ffiOwnerMode");
        prototypeReleaseHookValue = get_object_field_value(state, &prototype->super, "__zr_ffiReleaseHook");
        TEST_ASSERT_NOT_NULL(prototypeLoweringValue);
        TEST_ASSERT_NOT_NULL(prototypeViewTypeValue);
        TEST_ASSERT_NOT_NULL(prototypeUnderlyingValue);
        TEST_ASSERT_NOT_NULL(prototypeOwnerModeValue);
        TEST_ASSERT_NOT_NULL(prototypeReleaseHookValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, prototypeLoweringValue->value.object),
                                               "handle_id"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, prototypeViewTypeValue->value.object),
                                               "ModeHandleView"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, prototypeUnderlyingValue->value.object),
                                               "i32"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, prototypeOwnerModeValue->value.object),
                                               "borrowed"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, prototypeReleaseHookValue->value.object),
                                               "close_mode_handle"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_module_runtime_registers_enum_members_and_interface_inheritance(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Module Runtime Registers Enum Members And Interface Inheritance";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        SZrObjectPrototype *readablePrototype;
        SZrObjectPrototype *streamReadablePrototype;
        SZrObjectPrototype *enumPrototype;
        const SZrTypeValue *onValue;
        SZrObject *onObject;
        const SZrTypeValue *enumValueField;
        const SZrTypeValue *enumNameField;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);

        readablePrototype = get_module_exported_prototype(state, module, "NativeReadable");
        streamReadablePrototype = get_module_exported_prototype(state, module, "NativeStreamReadable");
        enumPrototype = get_module_exported_prototype(state, module, "NativeMode");

        TEST_ASSERT_NOT_NULL(readablePrototype);
        TEST_ASSERT_NOT_NULL(streamReadablePrototype);
        TEST_ASSERT_NOT_NULL(enumPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, readablePrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, streamReadablePrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_ENUM, enumPrototype->type);
        TEST_ASSERT_EQUAL_PTR(readablePrototype, streamReadablePrototype->superPrototype);

        onValue = get_object_field_value(state, &enumPrototype->super, "On");
        TEST_ASSERT_NOT_NULL(onValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, onValue->type);

        onObject = ZR_CAST_OBJECT(state, onValue->value.object);
        TEST_ASSERT_NOT_NULL(onObject);
        TEST_ASSERT_EQUAL_PTR(enumPrototype, onObject->prototype);

        enumValueField = get_object_field_value(state, onObject, "__zr_enumValue");
        enumNameField = get_object_field_value(state, onObject, "__zr_enumName");
        TEST_ASSERT_NOT_NULL(enumValueField);
        TEST_ASSERT_NOT_NULL(enumNameField);
        TEST_ASSERT_EQUAL_INT64(1, enumValueField->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, enumNameField->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumNameField->value.object), "On"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_enum_construction_returns_runtime_enum_instance(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Enum Construction Returns Runtime Enum Instance";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var probe = %import(\"probe.native_shapes\");\n"
                "return $probe.NativeMode(1);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObjectModule *module;
        SZrObjectPrototype *enumPrototype;
        SZrObject *resultObject;
        const SZrTypeValue *enumValueField;
        const SZrTypeValue *enumNameField;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        sourceName = ZrCore_String_Create(state, "probe_native_enum_runtime_test.zr", 33);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);
        enumPrototype = get_module_exported_prototype(state, module, "NativeMode");
        TEST_ASSERT_NOT_NULL(enumPrototype);
        TEST_ASSERT_EQUAL_PTR(enumPrototype, resultObject->prototype);

        enumValueField = get_object_field_value(state, resultObject, "__zr_enumValue");
        enumNameField = get_object_field_value(state, resultObject, "__zr_enumName");
        TEST_ASSERT_NOT_NULL(enumValueField);
        TEST_ASSERT_NOT_NULL(enumNameField);
        TEST_ASSERT_EQUAL_INT64(1, enumValueField->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, enumNameField->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumNameField->value.object), "On"));

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_binding_helpers_root_fresh_values_across_gc_retry(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Binding Helpers Root Fresh Values Across Gc Retry";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        ZrTestAllocatorFailureConfig failureConfig = {0};
        SZrState *state = create_test_state_with_allocator_context(&failureConfig);
        SZrObject *targetObject;
        SZrObject *sourceObject;
        SZrObject *targetArray;
        SZrObject *arraySourceObject;
        SZrTypeValue sourceValue;
        SZrTypeValue arrayValue;
        const SZrTypeValue *capturedField;
        const SZrTypeValue *capturedEntry;

        TEST_ASSERT_NOT_NULL(state);

        targetObject = ZrLib_Object_New(state);
        sourceObject = ZrLib_Object_New(state);
        targetArray = ZrLib_Array_New(state);
        arraySourceObject = ZrLib_Object_New(state);
        TEST_ASSERT_NOT_NULL(targetObject);
        TEST_ASSERT_NOT_NULL(sourceObject);
        TEST_ASSERT_NOT_NULL(targetArray);
        TEST_ASSERT_NOT_NULL(arraySourceObject);

        ZrLib_Value_SetObject(state, &sourceValue, sourceObject, ZR_VALUE_TYPE_OBJECT);
        failureConfig.failType = ZR_MEMORY_NATIVE_TYPE_HASH_PAIR;
        failureConfig.armed = ZR_TRUE;
        failureConfig.fired = ZR_FALSE;
        ZrLib_Object_SetFieldCString(state, targetObject, "captured", &sourceValue);
        TEST_ASSERT_TRUE(failureConfig.fired);

        capturedField = ZrLib_Object_GetFieldCString(state, targetObject, "captured");
        TEST_ASSERT_NOT_NULL(capturedField);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, capturedField->type);
        TEST_ASSERT_EQUAL_PTR(sourceObject, ZR_CAST_OBJECT(state, capturedField->value.object));

        ZrLib_Value_SetObject(state, &arrayValue, arraySourceObject, ZR_VALUE_TYPE_OBJECT);
        failureConfig.armed = ZR_TRUE;
        failureConfig.fired = ZR_FALSE;
        TEST_ASSERT_TRUE(ZrLib_Array_PushValue(state, targetArray, &arrayValue));
        TEST_ASSERT_TRUE(failureConfig.fired);

        capturedEntry = get_array_entry_value(state, targetArray, 0);
        TEST_ASSERT_NOT_NULL(capturedEntry);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, capturedEntry->type);
        TEST_ASSERT_EQUAL_PTR(arraySourceObject, ZR_CAST_OBJECT(state, capturedEntry->value.object));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_module_info_exposes_callable_parameters_and_generic_constraints(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Module Info Exposes Callable Parameters And Generic Constraints";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *functionsValue;
        const SZrTypeValue *typesValue;
        SZrObject *moduleInfo;
        SZrObject *functionsArray;
        SZrObject *typesArray;
        SZrObject *createDeviceEntry;
        SZrObject *deviceEntry;
        SZrObject *boxEntry;
        SZrObject *lookupEntry;
        const SZrTypeValue *parametersValue;
        const SZrTypeValue *methodsValue;
        const SZrTypeValue *genericParametersValue;
        SZrObject *parametersArray;
        SZrObject *methodsArray;
        SZrObject *genericParametersArray;
        SZrObject *configureEntry;
        SZrObject *firstParameterEntry;
        SZrObject *firstGenericEntry;
        SZrObject *lookupKeyGenericEntry;
        const SZrTypeValue *parameterNameValue;
        const SZrTypeValue *parameterTypeValue;
        const SZrTypeValue *genericNameValue;
        const SZrTypeValue *constraintsValue;
        const SZrTypeValue *firstConstraintValue;
        const SZrTypeValue *secondConstraintValue;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        functionsValue = get_object_field_value(state, moduleInfo, "functions");
        typesValue = get_object_field_value(state, moduleInfo, "types");
        TEST_ASSERT_NOT_NULL(functionsValue);
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, functionsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);

        functionsArray = ZR_CAST_OBJECT(state, functionsValue->value.object);
        typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
        TEST_ASSERT_NOT_NULL(functionsArray);
        TEST_ASSERT_NOT_NULL(typesArray);

        createDeviceEntry = find_named_entry_in_array(state, functionsArray, "name", "createDevice");
        TEST_ASSERT_NOT_NULL(createDeviceEntry);
        parametersValue = get_object_field_value(state, createDeviceEntry, "parameters");
        TEST_ASSERT_NOT_NULL(parametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parametersValue->type);
        parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
        TEST_ASSERT_NOT_NULL(parametersArray);
        TEST_ASSERT_EQUAL_UINT64(1, get_array_length(parametersArray));
        firstParameterEntry = get_array_entry_object(state, parametersArray, 0);
        TEST_ASSERT_NOT_NULL(firstParameterEntry);
        parameterNameValue = get_object_field_value(state, firstParameterEntry, "name");
        parameterTypeValue = get_object_field_value(state, firstParameterEntry, "typeName");
        TEST_ASSERT_NOT_NULL(parameterNameValue);
        TEST_ASSERT_NOT_NULL(parameterTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterNameValue->value.object), "mode"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterTypeValue->value.object), "NativeMode"));

        deviceEntry = find_named_entry_in_array(state, typesArray, "name", "NativeDevice");
        TEST_ASSERT_NOT_NULL(deviceEntry);
        methodsValue = get_object_field_value(state, deviceEntry, "methods");
        TEST_ASSERT_NOT_NULL(methodsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, methodsValue->type);
        methodsArray = ZR_CAST_OBJECT(state, methodsValue->value.object);
        TEST_ASSERT_NOT_NULL(methodsArray);
        configureEntry = find_named_entry_in_array(state, methodsArray, "name", "configure");
        TEST_ASSERT_NOT_NULL(configureEntry);
        parametersValue = get_object_field_value(state, configureEntry, "parameters");
        TEST_ASSERT_NOT_NULL(parametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parametersValue->type);
        parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
        TEST_ASSERT_NOT_NULL(parametersArray);
        TEST_ASSERT_EQUAL_UINT64(1, get_array_length(parametersArray));
        firstParameterEntry = get_array_entry_object(state, parametersArray, 0);
        TEST_ASSERT_NOT_NULL(firstParameterEntry);
        parameterNameValue = get_object_field_value(state, firstParameterEntry, "name");
        parameterTypeValue = get_object_field_value(state, firstParameterEntry, "typeName");
        TEST_ASSERT_NOT_NULL(parameterNameValue);
        TEST_ASSERT_NOT_NULL(parameterTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterNameValue->value.object), "mode"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterTypeValue->value.object), "NativeMode"));

        boxEntry = find_named_entry_in_array(state, typesArray, "name", "NativeBox");
        TEST_ASSERT_NOT_NULL(boxEntry);
        genericParametersValue = get_object_field_value(state, boxEntry, "genericParameters");
        TEST_ASSERT_NOT_NULL(genericParametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, genericParametersValue->type);
        genericParametersArray = ZR_CAST_OBJECT(state, genericParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(genericParametersArray);
        TEST_ASSERT_EQUAL_UINT64(1, get_array_length(genericParametersArray));
        firstGenericEntry = get_array_entry_object(state, genericParametersArray, 0);
        TEST_ASSERT_NOT_NULL(firstGenericEntry);
        genericNameValue = get_object_field_value(state, firstGenericEntry, "name");
        constraintsValue = get_object_field_value(state, firstGenericEntry, "constraints");
        TEST_ASSERT_NOT_NULL(genericNameValue);
        TEST_ASSERT_NOT_NULL(constraintsValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, genericNameValue->value.object), "T"));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constraintsValue->type);
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, constraintsValue->value.object)));

        lookupEntry = find_named_entry_in_array(state, typesArray, "name", "NativeLookup");
        TEST_ASSERT_NOT_NULL(lookupEntry);
        genericParametersValue = get_object_field_value(state, lookupEntry, "genericParameters");
        TEST_ASSERT_NOT_NULL(genericParametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, genericParametersValue->type);
        genericParametersArray = ZR_CAST_OBJECT(state, genericParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(genericParametersArray);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(genericParametersArray));
        lookupKeyGenericEntry = get_array_entry_object(state, genericParametersArray, 0);
        TEST_ASSERT_NOT_NULL(lookupKeyGenericEntry);
        genericNameValue = get_object_field_value(state, lookupKeyGenericEntry, "name");
        constraintsValue = get_object_field_value(state, lookupKeyGenericEntry, "constraints");
        TEST_ASSERT_NOT_NULL(genericNameValue);
        TEST_ASSERT_NOT_NULL(constraintsValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, genericNameValue->value.object), "K"));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constraintsValue->type);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(ZR_CAST_OBJECT(state, constraintsValue->value.object)));
        firstConstraintValue = get_array_entry_value(state, ZR_CAST_OBJECT(state, constraintsValue->value.object), 0);
        secondConstraintValue = get_array_entry_value(state, ZR_CAST_OBJECT(state, constraintsValue->value.object), 1);
        TEST_ASSERT_NOT_NULL(firstConstraintValue);
        TEST_ASSERT_NOT_NULL(secondConstraintValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstConstraintValue->value.object), "NativeReadable"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, secondConstraintValue->value.object), "NativeStreamReadable"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_module_exports_generic_interfaces_and_constraints(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Module Exports Generic Interfaces And Constraints";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *typesValue;
        SZrObject *moduleInfo;
        SZrObject *typesArray;
        SZrObject *arrayLikeEntry;
        SZrObject *mapEntry;
        SZrObject *setEntry;
        const SZrTypeValue *metaMethodsValue;
        const SZrTypeValue *genericParametersValue;
        const SZrTypeValue *constraintsValue;
        SZrObject *metaMethodsArray;
        SZrObject *genericParametersArray;
        SZrObject *firstGenericEntry;
        const SZrTypeValue *firstMetaTypeValue;
        const SZrTypeValue *secondMetaTypeValue;
        const SZrTypeValue *firstConstraintValue;
        const SZrTypeValue *secondConstraintValue;

        TEST_ASSERT_NOT_NULL(state);

        module = import_native_module(state, "zr.container");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        typesValue = get_object_field_value(state, moduleInfo, "types");
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
        typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
        TEST_ASSERT_NOT_NULL(typesArray);
        TEST_ASSERT_EQUAL_UINT64(6, get_array_length(typesArray));

        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Array"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Map"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Set"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Pair"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "LinkedList"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "LinkedNode"));

        arrayLikeEntry = find_named_entry_in_array(state, typesArray, "name", "Array");
        TEST_ASSERT_NOT_NULL(arrayLikeEntry);
        metaMethodsValue = get_object_field_value(state, arrayLikeEntry, "implements");
        TEST_ASSERT_NOT_NULL(metaMethodsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, metaMethodsValue->type);
        metaMethodsArray = ZR_CAST_OBJECT(state, metaMethodsValue->value.object);
        TEST_ASSERT_NOT_NULL(metaMethodsArray);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(metaMethodsArray));
        firstMetaTypeValue = get_array_entry_value(state, metaMethodsArray, 0);
        secondMetaTypeValue = get_array_entry_value(state, metaMethodsArray, 1);
        TEST_ASSERT_NOT_NULL(firstMetaTypeValue);
        TEST_ASSERT_NOT_NULL(secondMetaTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstMetaTypeValue->value.object),
                                              "zr.builtin.IArrayLike<T>"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, secondMetaTypeValue->value.object),
                                              "zr.builtin.IEnumerable<T>"));

        mapEntry = find_named_entry_in_array(state, typesArray, "name", "Map");
        TEST_ASSERT_NOT_NULL(mapEntry);
        genericParametersValue = get_object_field_value(state, mapEntry, "genericParameters");
        TEST_ASSERT_NOT_NULL(genericParametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, genericParametersValue->type);
        genericParametersArray = ZR_CAST_OBJECT(state, genericParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(genericParametersArray);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(genericParametersArray));
        firstGenericEntry = get_array_entry_object(state, genericParametersArray, 0);
        TEST_ASSERT_NOT_NULL(firstGenericEntry);
        constraintsValue = get_object_field_value(state, firstGenericEntry, "constraints");
        TEST_ASSERT_NOT_NULL(constraintsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constraintsValue->type);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(ZR_CAST_OBJECT(state, constraintsValue->value.object)));
        firstConstraintValue = get_array_entry_value(state, ZR_CAST_OBJECT(state, constraintsValue->value.object), 0);
        secondConstraintValue = get_array_entry_value(state, ZR_CAST_OBJECT(state, constraintsValue->value.object), 1);
        TEST_ASSERT_NOT_NULL(firstConstraintValue);
        TEST_ASSERT_NOT_NULL(secondConstraintValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstConstraintValue->value.object),
                                              "zr.builtin.IHashable"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, secondConstraintValue->value.object),
                                              "zr.builtin.IEquatable<K>"));

        setEntry = find_named_entry_in_array(state, typesArray, "name", "Set");
        TEST_ASSERT_NOT_NULL(setEntry);
        genericParametersValue = get_object_field_value(state, setEntry, "genericParameters");
        TEST_ASSERT_NOT_NULL(genericParametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, genericParametersValue->type);
        genericParametersArray = ZR_CAST_OBJECT(state, genericParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(genericParametersArray);
        TEST_ASSERT_EQUAL_UINT64(1, get_array_length(genericParametersArray));
        firstGenericEntry = get_array_entry_object(state, genericParametersArray, 0);
        TEST_ASSERT_NOT_NULL(firstGenericEntry);
        constraintsValue = get_object_field_value(state, firstGenericEntry, "constraints");
        TEST_ASSERT_NOT_NULL(constraintsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constraintsValue->type);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(ZR_CAST_OBJECT(state, constraintsValue->value.object)));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_builtin_module_exports_protocols_roots_and_wrappers(void) {
    SZrTestTimer timer;
    const char *testSummary = "Builtin Module Exports Protocols Roots And Wrappers";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *typesValue;
        SZrObject *moduleInfo;
        SZrObject *typesArray;

        TEST_ASSERT_NOT_NULL(state);

        module = import_native_module(state, "zr.builtin");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        typesValue = get_object_field_value(state, moduleInfo, "types");
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
        typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
        TEST_ASSERT_NOT_NULL(typesArray);

        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IEnumerable"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IEnumerator"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IArrayLike"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IEquatable"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IHashable"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IComparable"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "IComparer"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Object"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Module"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "TypeInfo"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Integer"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Float"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Double"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "String"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Bool"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Byte"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "Char"));
        TEST_ASSERT_NOT_NULL(find_named_entry_in_array(state, typesArray, "name", "UInt64"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_builtin_module_exports_complete_type_hints(void) {
    SZrTestTimer timer;
    const char *testSummary = "Builtin Module Exports Complete Type Hints";
    static const struct {
        const TZrChar *symbolName;
        const TZrChar *signature;
    } kBuiltinTypeHints[] = {
            {"IEnumerable", "interface IEnumerable<T>"},
            {"IEnumerator", "interface IEnumerator<T>"},
            {"IArrayLike", "interface IArrayLike<T>"},
            {"IEquatable", "interface IEquatable<T>"},
            {"IHashable", "interface IHashable"},
            {"IComparable", "interface IComparable<T>"},
            {"IComparer", "interface IComparer<T>"},
            {"Object", "class Object"},
            {"Module", "class Module"},
            {"TypeInfo", "class TypeInfo"},
            {"Integer", "class Integer"},
            {"Float", "class Float"},
            {"Double", "class Double"},
            {"String", "class String"},
            {"Bool", "class Bool"},
            {"Byte", "class Byte"},
            {"Char", "class Char"},
            {"UInt64", "class UInt64"},
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *typeHintsValue;
        SZrObject *moduleInfo;
        SZrObject *typeHintsArray;

        TEST_ASSERT_NOT_NULL(state);

        module = import_native_module(state, "zr.builtin");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        typeHintsValue = get_object_field_value(state, moduleInfo, "typeHints");
        TEST_ASSERT_NOT_NULL(typeHintsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typeHintsValue->type);

        typeHintsArray = ZR_CAST_OBJECT(state, typeHintsValue->value.object);
        TEST_ASSERT_NOT_NULL(typeHintsArray);
        TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(kBuiltinTypeHints), get_array_length(typeHintsArray));

        for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kBuiltinTypeHints); index++) {
            SZrObject *hintEntry = find_named_entry_in_array(state,
                                                             typeHintsArray,
                                                             "symbolName",
                                                             kBuiltinTypeHints[index].symbolName);
            const SZrTypeValue *signatureValue;

            TEST_ASSERT_NOT_NULL(hintEntry);
            signatureValue = get_object_field_value(state, hintEntry, "signature");
            TEST_ASSERT_NOT_NULL(signatureValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, signatureValue->type);
            TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, signatureValue->value.object),
                                                   kBuiltinTypeHints[index].signature));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_builtin_wrapper_box_helpers_preserve_runtime_semantics(void) {
    SZrTestTimer timer;
    const char *testSummary = "Builtin Wrapper Box Helpers Preserve Runtime Semantics";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var builtin = %import(\"zr.builtin\");\n"
                "var {TypeInfo} = %import(\"zr.builtin\");\n"
                "var left = builtin.Object.box(7);\n"
                "var right = TypeInfo.box(7);\n"
                "var other = builtin.Object.box(9);\n"
                "return {\n"
                "    leftType: builtin.Object.type(left),\n"
                "    rightType: builtin.Object.type(right),\n"
                "    reflectionName: %type(left).name,\n"
                "    baseType: %type(left).extendsTypeName,\n"
                "    equalsRight: left.equals(right),\n"
                "    compareRight: left.compareTo(right),\n"
                "    compareOther: left.compareTo(other),\n"
                "    hashEqual: left.hashCode() == right.hashCode()\n"
                "};\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;
        const SZrTypeValue *leftTypeValue;
        const SZrTypeValue *rightTypeValue;
        const SZrTypeValue *reflectionNameValue;
        const SZrTypeValue *baseTypeValue;
        const SZrTypeValue *equalsRightValue;
        const SZrTypeValue *compareRightValue;
        const SZrTypeValue *compareOtherValue;
        const SZrTypeValue *hashEqualValue;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "builtin_box_helpers_runtime_test.zr", 35);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        leftTypeValue = get_object_field_value(state, resultObject, "leftType");
        rightTypeValue = get_object_field_value(state, resultObject, "rightType");
        reflectionNameValue = get_object_field_value(state, resultObject, "reflectionName");
        baseTypeValue = get_object_field_value(state, resultObject, "baseType");
        equalsRightValue = get_object_field_value(state, resultObject, "equalsRight");
        compareRightValue = get_object_field_value(state, resultObject, "compareRight");
        compareOtherValue = get_object_field_value(state, resultObject, "compareOther");
        hashEqualValue = get_object_field_value(state, resultObject, "hashEqual");

        TEST_ASSERT_NOT_NULL(leftTypeValue);
        TEST_ASSERT_NOT_NULL(rightTypeValue);
        TEST_ASSERT_NOT_NULL(reflectionNameValue);
        TEST_ASSERT_NOT_NULL(baseTypeValue);
        TEST_ASSERT_NOT_NULL(equalsRightValue);
        TEST_ASSERT_NOT_NULL(compareRightValue);
        TEST_ASSERT_NOT_NULL(compareOtherValue);
        TEST_ASSERT_NOT_NULL(hashEqualValue);

        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, leftTypeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, rightTypeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, reflectionNameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, baseTypeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, equalsRightValue->type);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(compareRightValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(compareOtherValue->type));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, hashEqualValue->type);

        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, leftTypeValue->value.object), "Integer"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, rightTypeValue->value.object), "Integer"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, reflectionNameValue->value.object), "Integer"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, baseTypeValue->value.object), "zr.builtin.Object"));
        TEST_ASSERT_TRUE(equalsRightValue->value.nativeObject.nativeBool);
        TEST_ASSERT_EQUAL_INT64(0, compareRightValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(-1, compareOtherValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(hashEqualValue->value.nativeObject.nativeBool);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_supports_add_and_computed_index_access(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Array Runtime Supports Add And Computed Index Access";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var container = %import(\"zr.container\");\n"
                "var xs = new container.Array<int>();\n"
                "xs.add(10);\n"
                "xs.add(20);\n"
                "xs[1] = 25;\n"
                "return xs[1];\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "container_array_runtime_test.zr", 31);
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(25, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_fixed_array_runtime_supports_foreach_iteration(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Fixed Array Runtime Supports Foreach Iteration";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var sum = 0;\n"
                "var xs = [1, 2, 3];\n"
                "for (var item in xs) {\n"
                "    sum = sum + item;\n"
                "}\n"
                "return sum;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "container_fixed_array_foreach_runtime_test.zr", 45);
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(6, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_supports_foreach_iteration(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Array Runtime Supports Foreach Iteration";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var container = %import(\"zr.container\");\n"
                "var xs = new container.Array<int>();\n"
                "xs.add(1);\n"
                "xs.add(2);\n"
                "xs.add(3);\n"
                "var sum = 0;\n"
                "for (var item in xs) {\n"
                "    sum = sum + item;\n"
                "}\n"
                "return sum;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "container_array_foreach_runtime_test.zr", 39);
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(6, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_map_runtime_supports_computed_key_access(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Map Runtime Supports Computed Key Access";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var container = %import(\"zr.container\");\n"
                "var map = new container.Map<string,int>();\n"
                "map[\"answer\"] = 42;\n"
                "map[\"answer\"] = 43;\n"
                "return map[\"answer\"];\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "container_map_runtime_test.zr", 29);
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(43, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_set_runtime_enforces_uniqueness(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Set Runtime Enforces Uniqueness";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var container = %import(\"zr.container\");\n"
                "var values = new container.Set<int>();\n"
                "values.add(1);\n"
                "values.add(1);\n"
                "values.add(2);\n"
                "return values.count;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "container_set_runtime_test.zr", 29);
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(2, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_container_linked_list_runtime_updates_head_and_tail(void) {
    SZrTestTimer timer;
    const char *testSummary = "Container Linked List Runtime Updates Head And Tail";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var container = %import(\"zr.container\");\n"
                "var list = new container.LinkedList<int>();\n"
                "list.addLast(10);\n"
                "list.addLast(20);\n"
                "return list.count + list.first.value + list.last.value;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "container_linked_list_runtime_test.zr", 37);
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(32, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_module_reflection_exposes_expected_fields(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Module Reflection Exposes Expected Fields";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var probe = %import(\"probe.native_shapes\");\n"
                "return %type(probe);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *nameValue;
        const SZrTypeValue *qualifiedNameValue;
        const SZrTypeValue *kindValue;
        const SZrTypeValue *hashValue;
        const SZrTypeValue *declarationsValue;
        const SZrTypeValue *variablesValue;
        const SZrTypeValue *entryValue;
        const SZrTypeValue *membersValue;
        const SZrTypeValue *deviceEntriesValue;
        SZrObject *deviceEntries;
        SZrObject *deviceReflection;
        const SZrTypeValue *deviceKindValue;
        SZrString *resultString;
        SZrObjectModule *module;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        sourceName = ZrCore_String_Create(state, "type_module_reflection_test.zr", 30);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        nameValue = get_object_field_value(state, reflectionObject, "name");
        qualifiedNameValue = get_object_field_value(state, reflectionObject, "qualifiedName");
        kindValue = get_object_field_value(state, reflectionObject, "kind");
        hashValue = get_object_field_value(state, reflectionObject, "hash");
        declarationsValue = get_object_field_value(state, reflectionObject, "declarations");
        variablesValue = get_object_field_value(state, reflectionObject, "variables");
        entryValue = get_object_field_value(state, reflectionObject, "entry");
        membersValue = get_object_field_value(state, reflectionObject, "members");

        TEST_ASSERT_NOT_NULL(nameValue);
        TEST_ASSERT_NOT_NULL(qualifiedNameValue);
        TEST_ASSERT_NOT_NULL(kindValue);
        TEST_ASSERT_NOT_NULL(hashValue);
        TEST_ASSERT_NOT_NULL(declarationsValue);
        TEST_ASSERT_NOT_NULL(variablesValue);
        TEST_ASSERT_NOT_NULL(entryValue);
        TEST_ASSERT_NOT_NULL(membersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, nameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, qualifiedNameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, kindValue->type);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(hashValue->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(hashValue->type));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, declarationsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, variablesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, membersValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, nameValue->value.object), "probe.native_shapes"));
        TEST_ASSERT_TRUE(
                string_equals_cstring(ZR_CAST_STRING(state, qualifiedNameValue->value.object), "probe.native_shapes"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), "module"));

        deviceEntriesValue =
                get_object_field_value(state, ZR_CAST_OBJECT(state, declarationsValue->value.object), "NativeDevice");
        TEST_ASSERT_NOT_NULL(deviceEntriesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, deviceEntriesValue->type);

        deviceEntries = ZR_CAST_OBJECT(state, deviceEntriesValue->value.object);
        TEST_ASSERT_NOT_NULL(deviceEntries);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(deviceEntries));

        deviceReflection = get_array_entry_object(state, deviceEntries, 0);
        TEST_ASSERT_NOT_NULL(deviceReflection);
        deviceKindValue = get_object_field_value(state, deviceReflection, "kind");
        TEST_ASSERT_NOT_NULL(deviceKindValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, deviceKindValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, deviceKindValue->value.object), "class"));

        resultString = ZrCore_Value_ConvertToString(state, &result);
        TEST_ASSERT_NOT_NULL(resultString);
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(resultString), "module probe.native_shapes"));
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(resultString), "NativeDevice"));

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);
        TEST_ASSERT_EQUAL_UINT64(module->pathHash,
                                 (TZrUInt64)(ZR_VALUE_IS_TYPE_UNSIGNED_INT(hashValue->type)
                                                     ? hashValue->value.nativeObject.nativeUInt64
                                                     : (TZrUInt64)hashValue->value.nativeObject.nativeInt64));

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_instance_reflection_uses_runtime_prototype(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Instance Reflection Uses Runtime Prototype";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "class Vector2 { var x: int = 0; var y: int = 0; }\n"
                "var v = new Vector2();\n"
                "return %type(v);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *nameValue;
        const SZrTypeValue *kindValue;
        const SZrTypeValue *membersValue;
        const SZrTypeValue *xEntriesValue;
        const SZrTypeValue *yEntriesValue;
        SZrObject *xEntries;
        SZrObject *yEntries;
        SZrString *resultString;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "type_instance_reflection_test.zr", 32);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        nameValue = get_object_field_value(state, reflectionObject, "name");
        kindValue = get_object_field_value(state, reflectionObject, "kind");
        membersValue = get_object_field_value(state, reflectionObject, "members");
        TEST_ASSERT_NOT_NULL(nameValue);
        TEST_ASSERT_NOT_NULL(kindValue);
        TEST_ASSERT_NOT_NULL(membersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, nameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, kindValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, membersValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, nameValue->value.object), "Vector2"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), "class"));

        xEntriesValue = get_object_field_value(state, ZR_CAST_OBJECT(state, membersValue->value.object), "x");
        yEntriesValue = get_object_field_value(state, ZR_CAST_OBJECT(state, membersValue->value.object), "y");
        TEST_ASSERT_NOT_NULL(xEntriesValue);
        TEST_ASSERT_NOT_NULL(yEntriesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, xEntriesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, yEntriesValue->type);

        xEntries = ZR_CAST_OBJECT(state, xEntriesValue->value.object);
        yEntries = ZR_CAST_OBJECT(state, yEntriesValue->value.object);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(xEntries));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(yEntries));

        resultString = ZrCore_Value_ConvertToString(state, &result);
        TEST_ASSERT_NOT_NULL(resultString);
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(resultString), "class Vector2"));
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(resultString), "x:int;"));
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(resultString), "y:int;"));

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_module_reflection_uses_ordered_script_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_math",
                    "pub class Vector2 {\n"
                    "    pub var x: int = 0;\n"
                    "    pub var y: int = 0;\n"
                    "    pub length(scale: int): int {\n"
                    "        return this.x + this.y + scale;\n"
                    "    }\n"
                    "    pub static @add(left: Vector2, right: Vector2): Vector2 {\n"
                    "        return left;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "normalizeImpl(value: int, delta: int): int {\n"
                    "    return value + delta;\n"
                    "}\n"
                    "\n"
                    "pub var normalize = normalizeImpl;\n"
                    "pub var version: int = 1;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Module Reflection Uses Ordered Script Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var math = %import(\"reflect_math\");\n"
                "return %type(math);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *declarationsValue;
        const SZrTypeValue *variablesValue;
        const SZrTypeValue *entryValue;
        const SZrTypeValue *vectorEntriesValue;
        const SZrTypeValue *normalizeEntriesValue;
        const SZrTypeValue *normalizeImplEntriesValue;
        const SZrTypeValue *versionEntriesValue;
        SZrObject *vectorEntries;
        SZrObject *normalizeImplEntries;
        SZrObject *normalizeEntries;
        SZrObject *versionEntries;
        SZrString *resultString;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_module_reflection_test.zr", 37);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        declarationsValue = get_object_field_value(state, reflectionObject, "declarations");
        variablesValue = get_object_field_value(state, reflectionObject, "variables");
        entryValue = get_object_field_value(state, reflectionObject, "entry");
        TEST_ASSERT_NOT_NULL(declarationsValue);
        TEST_ASSERT_NOT_NULL(variablesValue);
        TEST_ASSERT_NOT_NULL(entryValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, declarationsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, variablesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, entryValue->type);

        vectorEntriesValue = get_object_field_value(state, ZR_CAST_OBJECT(state, declarationsValue->value.object), "Vector2");
        normalizeEntriesValue = get_object_field_value(state,
                                                       ZR_CAST_OBJECT(state, declarationsValue->value.object),
                                                       "normalize");
        normalizeImplEntriesValue = get_object_field_value(state,
                                                           ZR_CAST_OBJECT(state, declarationsValue->value.object),
                                                           "normalizeImpl");
        versionEntriesValue = get_object_field_value(state, ZR_CAST_OBJECT(state, variablesValue->value.object), "version");
        TEST_ASSERT_NOT_NULL(vectorEntriesValue);
        TEST_ASSERT_NOT_NULL(normalizeEntriesValue);
        TEST_ASSERT_NOT_NULL(normalizeImplEntriesValue);
        TEST_ASSERT_NOT_NULL(versionEntriesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, vectorEntriesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, normalizeImplEntriesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, normalizeEntriesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, versionEntriesValue->type);

        vectorEntries = ZR_CAST_OBJECT(state, vectorEntriesValue->value.object);
        normalizeImplEntries = ZR_CAST_OBJECT(state, normalizeImplEntriesValue->value.object);
        normalizeEntries = ZR_CAST_OBJECT(state, normalizeEntriesValue->value.object);
        versionEntries = ZR_CAST_OBJECT(state, versionEntriesValue->value.object);
        TEST_ASSERT_NOT_NULL(vectorEntries);
        TEST_ASSERT_NOT_NULL(normalizeImplEntries);
        TEST_ASSERT_NOT_NULL(normalizeEntries);
        TEST_ASSERT_NOT_NULL(versionEntries);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(vectorEntries));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(normalizeImplEntries));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(normalizeEntries));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(versionEntries));

        resultString = ZrCore_Value_ConvertToString(state, &result);
        TEST_ASSERT_NOT_NULL(resultString);
        TEST_ASSERT_TRUE(string_equals_cstring(resultString,
                                               "module reflect_math{\n"
                                               "class Vector2;\n"
                                               "normalizeImpl(value:int, delta:int): int;\n"
                                               "normalize(value:int, delta:int): int;\n"
                                               "version:int;\n"
                                               "__entry(): void;\n"
                                               "}"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_type_reflection_exposes_parameters_layout_and_codeblocks(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_math",
                    "pub class Vector2 {\n"
                    "    pub var x: int = 0;\n"
                    "    pub var y: int = 0;\n"
                    "    pub length(scale: int): int {\n"
                    "        return this.x + this.y + scale;\n"
                    "    }\n"
                    "    pub static @add(left: Vector2, right: Vector2): Vector2 {\n"
                    "        return left;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "normalizeImpl(value: int, delta: int): int {\n"
                    "    return value + delta;\n"
                    "}\n"
                    "\n"
                    "pub var normalize = normalizeImpl;\n"
                    "pub var version: int = 1;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Type Reflection Exposes Parameters Layout And CodeBlocks";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var math = %import(\"reflect_math\");\n"
                "return %type(math.Vector2);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *layoutValue;
        const SZrTypeValue *membersValue;
        const SZrTypeValue *lengthEntriesValue;
        const SZrTypeValue *addEntriesValue;
        SZrObject *lengthEntries;
        SZrObject *addEntries;
        SZrObject *lengthReflection;
        SZrObject *addReflection;
        const SZrTypeValue *parametersValue;
        SZrObject *parametersArray;
        SZrObject *firstParameter;
        const SZrTypeValue *parameterNameValue;
        const SZrTypeValue *parameterTypeValue;
        const SZrTypeValue *sourceInfoValue;
        const SZrTypeValue *irValue;
        const SZrTypeValue *codeBlocksValue;
        const SZrTypeValue *fieldCountValue;
        const SZrTypeValue *sizeValue;
        const SZrTypeValue *alignValue;
        SZrString *resultString;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_type_reflection_test.zr", 35);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        layoutValue = get_object_field_value(state, reflectionObject, "layout");
        membersValue = get_object_field_value(state, reflectionObject, "members");
        TEST_ASSERT_NOT_NULL(layoutValue);
        TEST_ASSERT_NOT_NULL(membersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, layoutValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, membersValue->type);

        fieldCountValue = get_object_field_value(state, ZR_CAST_OBJECT(state, layoutValue->value.object), "fieldCount");
        sizeValue = get_object_field_value(state, ZR_CAST_OBJECT(state, layoutValue->value.object), "size");
        alignValue = get_object_field_value(state, ZR_CAST_OBJECT(state, layoutValue->value.object), "alignment");
        TEST_ASSERT_NOT_NULL(fieldCountValue);
        TEST_ASSERT_NOT_NULL(sizeValue);
        TEST_ASSERT_NOT_NULL(alignValue);
        TEST_ASSERT_EQUAL_INT64(2, fieldCountValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(sizeValue->value.nativeObject.nativeInt64 > 0);
        TEST_ASSERT_TRUE(alignValue->value.nativeObject.nativeInt64 > 0);

        lengthEntriesValue = get_object_field_value(state, ZR_CAST_OBJECT(state, membersValue->value.object), "length");
        addEntriesValue = get_object_field_value(state, ZR_CAST_OBJECT(state, membersValue->value.object), "@add");
        TEST_ASSERT_NOT_NULL(lengthEntriesValue);
        TEST_ASSERT_NOT_NULL(addEntriesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, lengthEntriesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, addEntriesValue->type);

        lengthEntries = ZR_CAST_OBJECT(state, lengthEntriesValue->value.object);
        addEntries = ZR_CAST_OBJECT(state, addEntriesValue->value.object);
        TEST_ASSERT_NOT_NULL(lengthEntries);
        TEST_ASSERT_NOT_NULL(addEntries);
        lengthReflection = get_array_entry_object(state, lengthEntries, 0);
        addReflection = get_array_entry_object(state, addEntries, 0);
        TEST_ASSERT_NOT_NULL(lengthReflection);
        TEST_ASSERT_NOT_NULL(addReflection);

        parametersValue = get_object_field_value(state, lengthReflection, "parameters");
        sourceInfoValue = get_object_field_value(state, lengthReflection, "source");
        irValue = get_object_field_value(state, lengthReflection, "ir");
        codeBlocksValue = get_object_field_value(state, lengthReflection, "codeBlocks");
        TEST_ASSERT_NOT_NULL(parametersValue);
        TEST_ASSERT_NOT_NULL(sourceInfoValue);
        TEST_ASSERT_NOT_NULL(irValue);
        TEST_ASSERT_NOT_NULL(codeBlocksValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parametersValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, sourceInfoValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, irValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, codeBlocksValue->type);

        parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
        TEST_ASSERT_NOT_NULL(parametersArray);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(parametersArray));
        firstParameter = get_array_entry_object(state, parametersArray, 0);
        TEST_ASSERT_NOT_NULL(firstParameter);

        parameterNameValue = get_object_field_value(state, firstParameter, "name");
        parameterTypeValue = get_object_field_value(state, firstParameter, "typeName");
        TEST_ASSERT_NOT_NULL(parameterNameValue);
        TEST_ASSERT_NOT_NULL(parameterTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterNameValue->value.object), "scale"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterTypeValue->value.object), "int"));

        TEST_ASSERT_TRUE(get_array_length(ZR_CAST_OBJECT(state, codeBlocksValue->value.object)) > 0);
        TEST_ASSERT_TRUE(get_object_field_value(state, ZR_CAST_OBJECT(state, sourceInfoValue->value.object), "startLine") != ZR_NULL);
        TEST_ASSERT_TRUE(get_object_field_value(state, ZR_CAST_OBJECT(state, irValue->value.object), "instructionCount") != ZR_NULL);
        TEST_ASSERT_TRUE(get_object_field_value(state, addReflection, "parameters") != ZR_NULL);

        resultString = ZrCore_Value_ConvertToString(state, &result);
        TEST_ASSERT_NOT_NULL(resultString);
        TEST_ASSERT_TRUE(string_equals_cstring(resultString,
                                               "class Vector2{\n"
                                               "x:int;\n"
                                               "y:int;\n"
                                               "length(scale:int): int;\n"
                                               "static @add(left:Vector2, right:Vector2): Vector2;\n"
                                               "}"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_function_reflection_exposes_parameter_metadata_and_ir(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_math",
                    "pub class Vector2 {\n"
                    "    pub var x: int = 0;\n"
                    "    pub var y: int = 0;\n"
                    "    pub length(scale: int): int {\n"
                    "        return this.x + this.y + scale;\n"
                    "    }\n"
                    "    pub static @add(left: Vector2, right: Vector2): Vector2 {\n"
                    "        return left;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "normalizeImpl(value: int, delta: int): int {\n"
                    "    return value + delta;\n"
                    "}\n"
                    "\n"
                    "pub var normalize = normalizeImpl;\n"
                    "pub var version: int = 1;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Function Reflection Exposes Parameter Metadata And Ir";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var math = %import(\"reflect_math\");\n"
                "return %type(math.normalize);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *kindValue;
        const SZrTypeValue *parametersValue;
        const SZrTypeValue *sourceInfoValue;
        const SZrTypeValue *irValue;
        const SZrTypeValue *codeBlocksValue;
        SZrObject *parametersArray;
        SZrObject *firstParameter;
        SZrObject *secondParameter;
        const SZrTypeValue *firstParameterNameValue;
        const SZrTypeValue *secondParameterNameValue;
        const SZrTypeValue *firstParameterTypeValue;
        const SZrTypeValue *secondParameterTypeValue;
        SZrString *resultString;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_function_reflection_test.zr", 39);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        kindValue = get_object_field_value(state, reflectionObject, "kind");
        parametersValue = get_object_field_value(state, reflectionObject, "parameters");
        sourceInfoValue = get_object_field_value(state, reflectionObject, "source");
        irValue = get_object_field_value(state, reflectionObject, "ir");
        codeBlocksValue = get_object_field_value(state, reflectionObject, "codeBlocks");
        TEST_ASSERT_NOT_NULL(kindValue);
        TEST_ASSERT_NOT_NULL(parametersValue);
        TEST_ASSERT_NOT_NULL(sourceInfoValue);
        TEST_ASSERT_NOT_NULL(irValue);
        TEST_ASSERT_NOT_NULL(codeBlocksValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), "function"));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parametersValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, sourceInfoValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, irValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, codeBlocksValue->type);

        parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
        TEST_ASSERT_NOT_NULL(parametersArray);
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)get_array_length(parametersArray));
        firstParameter = get_array_entry_object(state, parametersArray, 0);
        secondParameter = get_array_entry_object(state, parametersArray, 1);
        TEST_ASSERT_NOT_NULL(firstParameter);
        TEST_ASSERT_NOT_NULL(secondParameter);

        firstParameterNameValue = get_object_field_value(state, firstParameter, "name");
        secondParameterNameValue = get_object_field_value(state, secondParameter, "name");
        firstParameterTypeValue = get_object_field_value(state, firstParameter, "typeName");
        secondParameterTypeValue = get_object_field_value(state, secondParameter, "typeName");
        TEST_ASSERT_NOT_NULL(firstParameterNameValue);
        TEST_ASSERT_NOT_NULL(secondParameterNameValue);
        TEST_ASSERT_NOT_NULL(firstParameterTypeValue);
        TEST_ASSERT_NOT_NULL(secondParameterTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstParameterNameValue->value.object), "value"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, secondParameterNameValue->value.object), "delta"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstParameterTypeValue->value.object), "int"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, secondParameterTypeValue->value.object), "int"));

        TEST_ASSERT_TRUE(get_object_field_value(state, ZR_CAST_OBJECT(state, sourceInfoValue->value.object), "startLine") != ZR_NULL);
        TEST_ASSERT_TRUE(get_object_field_value(state, ZR_CAST_OBJECT(state, irValue->value.object), "instructionCount") != ZR_NULL);
        TEST_ASSERT_TRUE(get_array_length(ZR_CAST_OBJECT(state, codeBlocksValue->value.object)) > 0);

        resultString = ZrCore_Value_ConvertToString(state, &result);
        TEST_ASSERT_NOT_NULL(resultString);
        TEST_ASSERT_TRUE(string_equals_cstring(resultString, "function reflect_math.normalize(value:int, delta:int): int"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_function_type_literal_runtime_materializes_callable_reflection(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Type Literal Runtime Materializes Callable Reflection";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var literal = %func(int)->int;\n"
                "return {\n"
                "    direct: literal,\n"
                "    query: %type(literal)\n"
                "};\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;
        const SZrTypeValue *directValue;
        const SZrTypeValue *queryValue;
        SZrObject *directObject;
        SZrObject *queryObject;
        const SZrTypeValue *directNameValue;
        const SZrTypeValue *queryNameValue;
        const SZrTypeValue *directParameterCountValue;
        const SZrTypeValue *queryParameterCountValue;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "function_type_literal_runtime_test.zr", 37);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        directValue = get_object_field_value(state, resultObject, "direct");
        queryValue = get_object_field_value(state, resultObject, "query");
        TEST_ASSERT_NOT_NULL(directValue);
        TEST_ASSERT_NOT_NULL(queryValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, directValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, queryValue->type);

        directObject = ZR_CAST_OBJECT(state, directValue->value.object);
        queryObject = ZR_CAST_OBJECT(state, queryValue->value.object);
        TEST_ASSERT_NOT_NULL(directObject);
        TEST_ASSERT_NOT_NULL(queryObject);

        directNameValue = get_object_field_value(state, directObject, "name");
        queryNameValue = get_object_field_value(state, queryObject, "name");
        directParameterCountValue = get_object_field_value(state, directObject, "parameterCount");
        queryParameterCountValue = get_object_field_value(state, queryObject, "parameterCount");
        TEST_ASSERT_NOT_NULL(directNameValue);
        TEST_ASSERT_NOT_NULL(queryNameValue);
        TEST_ASSERT_NOT_NULL(directParameterCountValue);
        TEST_ASSERT_NOT_NULL(queryParameterCountValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, directNameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, queryNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, directNameValue->value.object), "%func(int)->int"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, queryNameValue->value.object), "%func(int)->int"));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(directParameterCountValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(queryParameterCountValue->type));
        TEST_ASSERT_EQUAL_INT64(1, directParameterCountValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1, queryParameterCountValue->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_local_callable_reflection_preserves_callable_shape_in_source_and_binary(void) {
    SZrTestTimer timer;
    const char *testSummary =
            "Percent Type Local Callable Reflection Preserves Callable Shape In Source And Binary";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        const TZrChar *source =
                "localFunction(value: int, delta: int): int {\n"
                "    return value + delta;\n"
                "}\n"
                "var localLambda = (seed: int, bump: int) => {\n"
                "    return seed + bump;\n"
                "};\n"
                "return {\n"
                "    named: %type(localFunction),\n"
                "    lambda: %type(localLambda)\n"
                "};\n";
        const TZrChar *binaryPath = "local_callable_type_reflection_roundtrip.zro";
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *entryFunction = ZR_NULL;
        SZrTypeValue result;
        SZrObject *resultObject;
        const SZrTypeValue *namedValue;
        const SZrTypeValue *lambdaValue;
        SZrObject *namedReflection;
        SZrObject *lambdaReflection;
        const SZrTypeValue *namedReturnTypeName;
        const SZrTypeValue *lambdaReturnTypeName;
        const SZrTypeValue *namedParametersValue;
        const SZrTypeValue *lambdaParametersValue;
        SZrObject *namedParameters;
        SZrObject *lambdaParameters;
        SZrObject *namedFirstParameter;
        SZrObject *lambdaFirstParameter;
        const SZrTypeValue *namedFirstParameterName;
        const SZrTypeValue *lambdaFirstParameterName;
        const SZrTypeValue *namedKindValue;
        const SZrTypeValue *lambdaKindValue;
        TZrByte *binaryBytes = ZR_NULL;
        TZrSize binaryLength = 0;
        SZrModuleFixtureReader reader = {0};
        SZrIo io;
        SZrIoSource *sourceObject = ZR_NULL;
        SZrFunction *runtimeFunction = ZR_NULL;
        SZrTypeValue runtimeResult;
        SZrObject *runtimeResultObject;
        const SZrTypeValue *runtimeLambdaValue;
        SZrObject *runtimeLambdaReflection;
        const SZrTypeValue *runtimeLambdaReturnTypeName;
        const SZrTypeValue *runtimeLambdaParametersValue;
        SZrObject *runtimeLambdaParameters;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "local_callable_type_reflection_test.zr", 38);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        namedValue = get_object_field_value(state, resultObject, "named");
        lambdaValue = get_object_field_value(state, resultObject, "lambda");
        TEST_ASSERT_NOT_NULL(namedValue);
        TEST_ASSERT_NOT_NULL(lambdaValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, namedValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, lambdaValue->type);

        namedReflection = ZR_CAST_OBJECT(state, namedValue->value.object);
        lambdaReflection = ZR_CAST_OBJECT(state, lambdaValue->value.object);
        TEST_ASSERT_NOT_NULL(namedReflection);
        TEST_ASSERT_NOT_NULL(lambdaReflection);

        namedKindValue = get_object_field_value(state, namedReflection, "kind");
        lambdaKindValue = get_object_field_value(state, lambdaReflection, "kind");
        namedReturnTypeName = get_object_field_value(state, namedReflection, "returnTypeName");
        lambdaReturnTypeName = get_object_field_value(state, lambdaReflection, "returnTypeName");
        namedParametersValue = get_object_field_value(state, namedReflection, "parameters");
        lambdaParametersValue = get_object_field_value(state, lambdaReflection, "parameters");
        TEST_ASSERT_NOT_NULL(namedKindValue);
        TEST_ASSERT_NOT_NULL(lambdaKindValue);
        TEST_ASSERT_NOT_NULL(namedReturnTypeName);
        TEST_ASSERT_NOT_NULL(lambdaReturnTypeName);
        TEST_ASSERT_NOT_NULL(namedParametersValue);
        TEST_ASSERT_NOT_NULL(lambdaParametersValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, namedKindValue->value.object), "function"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, lambdaKindValue->value.object), "function"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, namedReturnTypeName->value.object), "int"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, lambdaReturnTypeName->value.object), "int"));

        namedParameters = ZR_CAST_OBJECT(state, namedParametersValue->value.object);
        lambdaParameters = ZR_CAST_OBJECT(state, lambdaParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(namedParameters);
        TEST_ASSERT_NOT_NULL(lambdaParameters);
        TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)get_array_length(namedParameters));
        TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)get_array_length(lambdaParameters));

        namedFirstParameter = get_array_entry_object(state, namedParameters, 0);
        lambdaFirstParameter = get_array_entry_object(state, lambdaParameters, 0);
        TEST_ASSERT_NOT_NULL(namedFirstParameter);
        TEST_ASSERT_NOT_NULL(lambdaFirstParameter);

        namedFirstParameterName = get_object_field_value(state, namedFirstParameter, "name");
        lambdaFirstParameterName = get_object_field_value(state, lambdaFirstParameter, "name");
        TEST_ASSERT_NOT_NULL(namedFirstParameterName);
        TEST_ASSERT_NOT_NULL(lambdaFirstParameterName);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, namedFirstParameterName->value.object), "value"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, lambdaFirstParameterName->value.object), "seed"));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, entryFunction, binaryPath));
        binaryBytes = read_test_file_bytes(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        ZrCore_Memory_RawSet(&io, 0, sizeof(io));
        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;
        ZrCore_Io_Init(state, &io, module_fixture_reader_read, ZR_NULL, &reader);
        io.isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(&io);
        TEST_ASSERT_NOT_NULL(sourceObject);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, runtimeFunction, &runtimeResult));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, runtimeResult.type);

        runtimeResultObject = ZR_CAST_OBJECT(state, runtimeResult.value.object);
        TEST_ASSERT_NOT_NULL(runtimeResultObject);

        runtimeLambdaValue = get_object_field_value(state, runtimeResultObject, "lambda");
        TEST_ASSERT_NOT_NULL(runtimeLambdaValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, runtimeLambdaValue->type);

        runtimeLambdaReflection = ZR_CAST_OBJECT(state, runtimeLambdaValue->value.object);
        TEST_ASSERT_NOT_NULL(runtimeLambdaReflection);
        runtimeLambdaReturnTypeName = get_object_field_value(state, runtimeLambdaReflection, "returnTypeName");
        runtimeLambdaParametersValue = get_object_field_value(state, runtimeLambdaReflection, "parameters");
        TEST_ASSERT_NOT_NULL(runtimeLambdaReturnTypeName);
        TEST_ASSERT_NOT_NULL(runtimeLambdaParametersValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, runtimeLambdaReturnTypeName->value.object), "int"));

        runtimeLambdaParameters = ZR_CAST_OBJECT(state, runtimeLambdaParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(runtimeLambdaParameters);
        TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)get_array_length(runtimeLambdaParameters));

        ZrCore_Function_Free(state, runtimeFunction);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_function_type_literal_reflection_exposes_callable_shape(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Function Type Literal Reflection Exposes Callable Shape";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source = "return %type(%func(%ref value:int)->%async int);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *kindValue;
        const SZrTypeValue *nameValue;
        const SZrTypeValue *returnTypeNameValue;
        const SZrTypeValue *returnTypeValue;
        const SZrTypeValue *parametersValue;
        const SZrTypeValue *parameterModesValue;
        const SZrTypeValue *genericParametersValue;
        const SZrTypeValue *isVariadicValue;
        SZrObject *returnTypeObject;
        SZrObject *parametersArray;
        SZrObject *parameterModesArray;
        SZrObject *genericParametersArray;
        SZrObject *firstParameter;
        const SZrTypeValue *firstParameterNameValue;
        const SZrTypeValue *firstParameterTypeValue;
        const SZrTypeValue *firstParameterModeValue;
        const SZrTypeValue *firstModeValue;
        const SZrTypeValue *returnTypeNameFieldValue;
        SZrString *resultString;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "callable_type_literal_reflection_test.zr", 39);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        kindValue = get_object_field_value(state, reflectionObject, "kind");
        nameValue = get_object_field_value(state, reflectionObject, "name");
        returnTypeNameValue = get_object_field_value(state, reflectionObject, "returnTypeName");
        returnTypeValue = get_object_field_value(state, reflectionObject, "returnType");
        parametersValue = get_object_field_value(state, reflectionObject, "parameters");
        parameterModesValue = get_object_field_value(state, reflectionObject, "parameterModes");
        genericParametersValue = get_object_field_value(state, reflectionObject, "genericParameters");
        isVariadicValue = get_object_field_value(state, reflectionObject, "isVariadic");

        TEST_ASSERT_NOT_NULL(kindValue);
        TEST_ASSERT_NOT_NULL(nameValue);
        TEST_ASSERT_NOT_NULL(returnTypeNameValue);
        TEST_ASSERT_NOT_NULL(returnTypeValue);
        TEST_ASSERT_NOT_NULL(parametersValue);
        TEST_ASSERT_NOT_NULL(parameterModesValue);
        TEST_ASSERT_NOT_NULL(genericParametersValue);
        TEST_ASSERT_NOT_NULL(isVariadicValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, kindValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, nameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, returnTypeNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), "function"));
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(ZR_CAST_STRING(state, nameValue->value.object)),
                                    "%func("));
        TEST_ASSERT_TRUE(
                string_equals_cstring(ZR_CAST_STRING(state, returnTypeNameValue->value.object), "%async int"));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, returnTypeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parametersValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parameterModesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, genericParametersValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, isVariadicValue->type);
        TEST_ASSERT_FALSE(isVariadicValue->value.nativeObject.nativeBool);

        returnTypeObject = ZR_CAST_OBJECT(state, returnTypeValue->value.object);
        TEST_ASSERT_NOT_NULL(returnTypeObject);
        returnTypeNameFieldValue = get_object_field_value(state, returnTypeObject, "name");
        TEST_ASSERT_NOT_NULL(returnTypeNameFieldValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, returnTypeNameFieldValue->type);
        TEST_ASSERT_TRUE(
                string_equals_cstring(ZR_CAST_STRING(state, returnTypeNameFieldValue->value.object), "%async int"));

        parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
        parameterModesArray = ZR_CAST_OBJECT(state, parameterModesValue->value.object);
        genericParametersArray = ZR_CAST_OBJECT(state, genericParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(parametersArray);
        TEST_ASSERT_NOT_NULL(parameterModesArray);
        TEST_ASSERT_NOT_NULL(genericParametersArray);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(parametersArray));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(parameterModesArray));
        TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)get_array_length(genericParametersArray));

        firstParameter = get_array_entry_object(state, parametersArray, 0);
        firstModeValue = get_array_entry_value(state, parameterModesArray, 0);
        TEST_ASSERT_NOT_NULL(firstParameter);
        TEST_ASSERT_NOT_NULL(firstModeValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, firstModeValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstModeValue->value.object), "%ref"));

        firstParameterNameValue = get_object_field_value(state, firstParameter, "name");
        firstParameterTypeValue = get_object_field_value(state, firstParameter, "typeName");
        firstParameterModeValue = get_object_field_value(state, firstParameter, "passingMode");
        TEST_ASSERT_NOT_NULL(firstParameterNameValue);
        TEST_ASSERT_NOT_NULL(firstParameterTypeValue);
        TEST_ASSERT_NOT_NULL(firstParameterModeValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, firstParameterNameValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, firstParameterTypeValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, firstParameterModeValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstParameterNameValue->value.object), "value"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstParameterTypeValue->value.object), "int"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstParameterModeValue->value.object), "%ref"));

        resultString = ZrCore_Value_ConvertToString(state, &result);
        TEST_ASSERT_NOT_NULL(resultString);
        TEST_ASSERT_NOT_NULL(strstr(ZrCore_String_GetNativeString(resultString), "%func("));

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_module_reflection_exposes_compile_time_and_test_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_meta",
                    "%compileTime var MAX_SCALE: int = 8;\n"
                    "%compileTime buildBias(seed: int): int {\n"
                    "    return seed + MAX_SCALE;\n"
                    "}\n"
                    "\n"
                    "pub var runtimeValue: int = MAX_SCALE;\n"
                    "\n"
                    "%test(\"vector_meta\") {\n"
                    "    return runtimeValue;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Module Reflection Exposes Compile Time And Test Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var meta = %import(\"reflect_meta\");\n"
                "return %type(meta);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *testsValue;
        const SZrTypeValue *compileTimeValue;
        SZrObject *testsArray;
        SZrObject *compileTimeObject;
        const SZrTypeValue *compileTimeVariablesValue;
        const SZrTypeValue *compileTimeFunctionsValue;
        SZrObject *compileTimeVariables;
        SZrObject *compileTimeFunctions;
        SZrObject *testInfo;
        SZrObject *compileTimeVariable;
        SZrObject *compileTimeFunction;
        const SZrTypeValue *testNameValue;
        const SZrTypeValue *variableNameValue;
        const SZrTypeValue *variableTypeValue;
        const SZrTypeValue *functionNameValue;
        const SZrTypeValue *functionReturnTypeValue;
        const SZrTypeValue *functionParametersValue;
        SZrObject *functionParameters;
        SZrObject *firstParameter;
        const SZrTypeValue *parameterNameValue;
        const SZrTypeValue *parameterTypeValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_module_compiletime_reflection_test.zr", 49);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        testsValue = get_object_field_value(state, reflectionObject, "tests");
        compileTimeValue = get_object_field_value(state, reflectionObject, "compileTime");
        TEST_ASSERT_NOT_NULL(testsValue);
        TEST_ASSERT_NOT_NULL(compileTimeValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, testsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, compileTimeValue->type);

        testsArray = ZR_CAST_OBJECT(state, testsValue->value.object);
        compileTimeObject = ZR_CAST_OBJECT(state, compileTimeValue->value.object);
        TEST_ASSERT_NOT_NULL(testsArray);
        TEST_ASSERT_NOT_NULL(compileTimeObject);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(testsArray));

        testInfo = get_array_entry_object(state, testsArray, 0);
        TEST_ASSERT_NOT_NULL(testInfo);
        testNameValue = get_object_field_value(state, testInfo, "name");
        TEST_ASSERT_NOT_NULL(testNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, testNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, testNameValue->value.object), "vector_meta"));

        compileTimeVariablesValue = get_object_field_value(state, compileTimeObject, "variables");
        compileTimeFunctionsValue = get_object_field_value(state, compileTimeObject, "functions");
        TEST_ASSERT_NOT_NULL(compileTimeVariablesValue);
        TEST_ASSERT_NOT_NULL(compileTimeFunctionsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeVariablesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeFunctionsValue->type);

        compileTimeVariables = ZR_CAST_OBJECT(state, compileTimeVariablesValue->value.object);
        compileTimeFunctions = ZR_CAST_OBJECT(state, compileTimeFunctionsValue->value.object);
        TEST_ASSERT_NOT_NULL(compileTimeVariables);
        TEST_ASSERT_NOT_NULL(compileTimeFunctions);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(compileTimeVariables));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(compileTimeFunctions));

        compileTimeVariable = get_array_entry_object(state, compileTimeVariables, 0);
        compileTimeFunction = get_array_entry_object(state, compileTimeFunctions, 0);
        TEST_ASSERT_NOT_NULL(compileTimeVariable);
        TEST_ASSERT_NOT_NULL(compileTimeFunction);

        variableNameValue = get_object_field_value(state, compileTimeVariable, "name");
        variableTypeValue = get_object_field_value(state, compileTimeVariable, "typeName");
        TEST_ASSERT_NOT_NULL(variableNameValue);
        TEST_ASSERT_NOT_NULL(variableTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, variableNameValue->value.object), "MAX_SCALE"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, variableTypeValue->value.object), "int"));

        functionNameValue = get_object_field_value(state, compileTimeFunction, "name");
        functionReturnTypeValue = get_object_field_value(state, compileTimeFunction, "returnTypeName");
        functionParametersValue = get_object_field_value(state, compileTimeFunction, "parameters");
        TEST_ASSERT_NOT_NULL(functionNameValue);
        TEST_ASSERT_NOT_NULL(functionReturnTypeValue);
        TEST_ASSERT_NOT_NULL(functionParametersValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, functionNameValue->value.object), "buildBias"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, functionReturnTypeValue->value.object), "int"));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, functionParametersValue->type);

        functionParameters = ZR_CAST_OBJECT(state, functionParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(functionParameters);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(functionParameters));

        firstParameter = get_array_entry_object(state, functionParameters, 0);
        TEST_ASSERT_NOT_NULL(firstParameter);
        parameterNameValue = get_object_field_value(state, firstParameter, "name");
        parameterTypeValue = get_object_field_value(state, firstParameter, "typeName");
        TEST_ASSERT_NOT_NULL(parameterNameValue);
        TEST_ASSERT_NOT_NULL(parameterTypeValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterNameValue->value.object), "seed"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterTypeValue->value.object), "int"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_binary_module_reflection_restores_compile_time_and_test_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Binary Module Reflection Restores Compile Time And Test Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        static const TZrChar *kModuleSource =
                "%compileTime var MAX_SCALE: int = 8;\n"
                "%compileTime buildBias(seed: int): int {\n"
                "    return seed + MAX_SCALE;\n"
                "}\n"
                "\n"
                "pub var runtimeValue: int = MAX_SCALE;\n"
                "\n"
                "%test(\"vector_meta\") {\n"
                "    return runtimeValue;\n"
                "}\n";
        const TZrChar *source =
                "var meta = %import(\"reflect_meta_binary\");\n"
                "return %type(meta);\n";
        TZrByte *binaryBytes = ZR_NULL;
        TZrSize binaryLength = 0;
        const TZrChar *binaryPath = "test_reflect_meta_binary_fixture.zro";
        SZrModuleFixtureSource fixtures[1] = {0};
        const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
        TZrSize previousFixtureCount = g_module_fixture_source_count;
        SZrString *sourceName;
        SZrFunction *entryFunction = ZR_NULL;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *testsValue;
        const SZrTypeValue *compileTimeValue;
        SZrObject *testsArray;
        SZrObject *compileTimeObject;
        const SZrTypeValue *compileTimeVariablesValue;
        const SZrTypeValue *compileTimeFunctionsValue;

        TEST_ASSERT_NOT_NULL(state);

        binaryBytes = build_module_binary_fixture(state, kModuleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "reflect_meta_binary";
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_binary_module_compiletime_reflection_test.zr", 49);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        testsValue = get_object_field_value(state, reflectionObject, "tests");
        compileTimeValue = get_object_field_value(state, reflectionObject, "compileTime");
        TEST_ASSERT_NOT_NULL(testsValue);
        TEST_ASSERT_NOT_NULL(compileTimeValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, testsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, compileTimeValue->type);

        testsArray = ZR_CAST_OBJECT(state, testsValue->value.object);
        compileTimeObject = ZR_CAST_OBJECT(state, compileTimeValue->value.object);
        TEST_ASSERT_NOT_NULL(testsArray);
        TEST_ASSERT_NOT_NULL(compileTimeObject);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(testsArray));

        compileTimeVariablesValue = get_object_field_value(state, compileTimeObject, "variables");
        compileTimeFunctionsValue = get_object_field_value(state, compileTimeObject, "functions");
        TEST_ASSERT_NOT_NULL(compileTimeVariablesValue);
        TEST_ASSERT_NOT_NULL(compileTimeFunctionsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeVariablesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeFunctionsValue->type);
        TEST_ASSERT_EQUAL_UINT32(1,
                                 (TZrUInt32)get_array_length(ZR_CAST_OBJECT(state,
                                                                            compileTimeVariablesValue->value.object)));
        TEST_ASSERT_EQUAL_UINT32(1,
                                 (TZrUInt32)get_array_length(ZR_CAST_OBJECT(state,
                                                                            compileTimeFunctionsValue->value.object)));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_type_reflection_exposes_decorator_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_decorators",
                    "%compileTime class Serializable {\n"
                    "    @decorate(target: %type Class): zr.DecoratorPatch {\n"
                    "        return { metadata: { serializable: true } };\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "#Serializable#\n"
                    "pub class User {\n"
                    "    pub var id: int = 1;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Type Reflection Exposes Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"reflect_decorators\");\n"
                "return %type(decorated.User);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *metadataValue;
        const SZrTypeValue *decoratorsValue;
        const SZrTypeValue *mutableValue;
        const SZrTypeValue *phaseValue;
        SZrObject *metadataObject;
        SZrObject *decoratorsArray;
        SZrObject *decoratorEntry;
        const SZrTypeValue *serializableValue;
        const SZrTypeValue *decoratorNameValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_decorator_reflection_test.zr", 39);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        metadataValue = get_object_field_value(state, reflectionObject, "metadata");
        decoratorsValue = get_object_field_value(state, reflectionObject, "decorators");
        mutableValue = get_object_field_value(state, reflectionObject, "mutable");
        phaseValue = get_object_field_value(state, reflectionObject, "phase");
        TEST_ASSERT_NOT_NULL(metadataValue);
        TEST_ASSERT_NOT_NULL(decoratorsValue);
        TEST_ASSERT_NOT_NULL(mutableValue);
        TEST_ASSERT_NOT_NULL(phaseValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, mutableValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, phaseValue->type);

        metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
        decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
        TEST_ASSERT_NOT_NULL(metadataObject);
        TEST_ASSERT_NOT_NULL(decoratorsArray);
        TEST_ASSERT_TRUE(mutableValue->value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, phaseValue->value.object), "runtime"));

        serializableValue = get_object_field_value(state, metadataObject, "serializable");
        TEST_ASSERT_NOT_NULL(serializableValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, serializableValue->type);
        TEST_ASSERT_TRUE(serializableValue->value.nativeObject.nativeBool);

        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
        decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
        TEST_ASSERT_NOT_NULL(decoratorEntry);
        decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
        TEST_ASSERT_NOT_NULL(decoratorNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object), "Serializable"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_type_reflection_exposes_runtime_class_decorator_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_runtime_class_decorators",
                    "class RuntimeSerializable {\n"
                    "    @decorate(target: %type Class): void {\n"
                    "        target.metadata.runtimeSerializable = true;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "#RuntimeSerializable#\n"
                    "pub class User {\n"
                    "    pub var id: int = 1;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Type Reflection Exposes Runtime Class Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"reflect_runtime_class_decorators\");\n"
                "return %type(decorated.User);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *metadataValue;
        const SZrTypeValue *decoratorsValue;
        SZrObject *metadataObject;
        SZrObject *decoratorsArray;
        SZrObject *decoratorEntry;
        const SZrTypeValue *runtimeSerializableValue;
        const SZrTypeValue *decoratorNameValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_runtime_class_decorator_reflection_test.zr", 53);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        metadataValue = get_object_field_value(state, reflectionObject, "metadata");
        decoratorsValue = get_object_field_value(state, reflectionObject, "decorators");
        TEST_ASSERT_NOT_NULL(metadataValue);
        TEST_ASSERT_NOT_NULL(decoratorsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);

        metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
        decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
        TEST_ASSERT_NOT_NULL(metadataObject);
        TEST_ASSERT_NOT_NULL(decoratorsArray);

        runtimeSerializableValue = get_object_field_value(state, metadataObject, "runtimeSerializable");
        TEST_ASSERT_NOT_NULL(runtimeSerializableValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, runtimeSerializableValue->type);
        TEST_ASSERT_TRUE(runtimeSerializableValue->value.nativeObject.nativeBool);

        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
        decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
        TEST_ASSERT_NOT_NULL(decoratorEntry);
        decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
        TEST_ASSERT_NOT_NULL(decoratorNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object),
                                               "RuntimeSerializable"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_function_reflection_exposes_runtime_function_decorator_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_runtime_function_decorators",
                    "func markRuntime(target: %type Function): void {\n"
                    "    target.metadata.instrumented = true;\n"
                    "}\n"
                    "\n"
                    "#markRuntime#\n"
                    "load(id: int): int {\n"
                    "    return id;\n"
                    "}\n"
                    "\n"
                    "pub var exportedLoad = load;\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Function Reflection Exposes Runtime Function Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"reflect_runtime_function_decorators\");\n"
                "return %type(decorated.exportedLoad);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *metadataValue;
        const SZrTypeValue *decoratorsValue;
        SZrObject *metadataObject;
        SZrObject *decoratorsArray;
        SZrObject *decoratorEntry;
        const SZrTypeValue *instrumentedValue;
        const SZrTypeValue *decoratorNameValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_runtime_function_decorator_reflection_test.zr", 56);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        metadataValue = get_object_field_value(state, reflectionObject, "metadata");
        decoratorsValue = get_object_field_value(state, reflectionObject, "decorators");
        TEST_ASSERT_NOT_NULL(metadataValue);
        TEST_ASSERT_NOT_NULL(decoratorsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);

        metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
        decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
        TEST_ASSERT_NOT_NULL(metadataObject);
        TEST_ASSERT_NOT_NULL(decoratorsArray);

        instrumentedValue = get_object_field_value(state, metadataObject, "instrumented");
        TEST_ASSERT_NOT_NULL(instrumentedValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, instrumentedValue->type);
        TEST_ASSERT_TRUE(instrumentedValue->value.nativeObject.nativeBool);

        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
        decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
        TEST_ASSERT_NOT_NULL(decoratorEntry);
        decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
        TEST_ASSERT_NOT_NULL(decoratorNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object), "markRuntime"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_runtime_decorated_pub_function_is_directly_callable_from_imported_module(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "call_runtime_function_decorators",
                    "func markRuntime(target: %type Function): void {\n"
                    "    target.metadata.instrumented = true;\n"
                    "}\n"
                    "\n"
                    "#markRuntime#\n"
                    "pub load(id: int): int {\n"
                    "    var meta = %type(load).metadata;\n"
                    "    return meta.instrumented ? id + 1 : id;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source runtime decorated pub function is directly callable from imported module";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"call_runtime_function_decorators\");\n"
                "return decorated.load(41);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "source_runtime_decorated_pub_function_import_test.zr", 52);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_runtime_registers_enum_members_and_imported_access(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "source_enum_runtime",
                    "%module \"source_enum_runtime\";\n"
                    "pub enum Mode: int {\n"
                    "    Idle = 1;\n"
                    "    Hot = 2;\n"
                    "}\n"
                    "\n"
                    "pub score(): int {\n"
                    "    var mode = Mode.Hot;\n"
                    "    if (mode == Mode.Hot) {\n"
                    "        return 7;\n"
                    "    }\n"
                    "    return 0;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source module runtime registers enum members and imported access";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var sample = %import(\"source_enum_runtime\");\n"
                "return sample.score();\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObjectModule *module;
        SZrObjectPrototype *enumPrototype;
        const SZrTypeValue *hotValue;
        SZrObject *hotObject;
        const SZrTypeValue *enumValueField;
        const SZrTypeValue *enumNameField;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "source_enum_runtime");
        TEST_ASSERT_NOT_NULL(module);

        enumPrototype = get_module_exported_prototype(state, module, "Mode");
        TEST_ASSERT_NOT_NULL(enumPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_ENUM, enumPrototype->type);

        hotValue = get_object_field_value(state, &enumPrototype->super, "Hot");
        TEST_ASSERT_NOT_NULL(hotValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, hotValue->type);

        hotObject = ZR_CAST_OBJECT(state, hotValue->value.object);
        TEST_ASSERT_NOT_NULL(hotObject);
        TEST_ASSERT_EQUAL_PTR(enumPrototype, hotObject->prototype);

        enumValueField = get_object_field_value(state, hotObject, "__zr_enumValue");
        enumNameField = get_object_field_value(state, hotObject, "__zr_enumName");
        TEST_ASSERT_NOT_NULL(enumValueField);
        TEST_ASSERT_NOT_NULL(enumNameField);
        TEST_ASSERT_EQUAL_INT64(2, enumValueField->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, enumNameField->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumNameField->value.object), "Hot"));

        sourceName = ZrCore_String_Create(state, "source_module_enum_runtime_test.zr", 34);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(7, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_struct_constructor_runtime_invocation_preserves_fields(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "source_struct_runtime_ctor",
                    "%module \"source_struct_runtime_ctor\";\n"
                    "pub struct Pair {\n"
                    "    pub var left: int;\n"
                    "    pub var right: int;\n"
                    "\n"
                    "    pub @constructor(left: int, right: int) {\n"
                    "        this.left = left;\n"
                    "        this.right = right;\n"
                    "    }\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source module struct constructor runtime invocation preserves fields";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        SZrObjectPrototype *pairPrototype;
        SZrObject *pairObject;
        SZrString *constructorName;
        SZrTypeValue receiver;
        SZrTypeValue args[2];
        SZrTypeValue result;
        const SZrTypeValue *leftValue;
        const SZrTypeValue *rightValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "source_struct_runtime_ctor");
        TEST_ASSERT_NOT_NULL(module);

        pairPrototype = get_module_exported_prototype(state, module, "Pair");
        TEST_ASSERT_NOT_NULL(pairPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, pairPrototype->type);

        pairObject = ZrCore_Object_New(state, pairPrototype);
        TEST_ASSERT_NOT_NULL(pairObject);
        ZrCore_Object_Init(state, pairObject);
        pairObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(pairObject)));

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(pairObject));
        receiver.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &args[0], 1);
        ZrCore_Value_InitAsInt(state, &args[1], 2);
        ZrCore_Value_ResetAsNull(&result);

        constructorName = ZrCore_String_CreateFromNative(state, "__constructor");
        TEST_ASSERT_NOT_NULL(constructorName);
        TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, &receiver, constructorName, args, 2, &result));

        leftValue = get_object_field_value(state, pairObject, "left");
        rightValue = get_object_field_value(state, pairObject, "right");
        TEST_ASSERT_NOT_NULL(leftValue);
        TEST_ASSERT_NOT_NULL(rightValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(leftValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(rightValue->type));
        TEST_ASSERT_EQUAL_INT64(1, leftValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(2, rightValue->value.nativeObject.nativeInt64);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(pairObject)));

        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_struct_constructor_manual_vm_call_preserves_fields(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "source_struct_runtime_ctor_manual",
                    "%module \"source_struct_runtime_ctor_manual\";\n"
                    "pub struct Pair {\n"
                    "    pub var left: int;\n"
                    "    pub var right: int;\n"
                    "\n"
                    "    pub @constructor(left: int, right: int) {\n"
                    "        this.left = left;\n"
                    "        this.right = right;\n"
                    "    }\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source module struct constructor manual vm call preserves fields";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        SZrObjectPrototype *pairPrototype;
        SZrObject *pairObject;
        const SZrTypeValue *constructorValue;
        SZrFunction *constructorFunction;
        SZrTypeValue receiver;
        SZrTypeValue args[2];
        TZrStackValuePointer callBase;
        SZrCallInfo *callInfo;
        SZrTypeValue *stackReceiverValue;
        SZrObject *stackReceiverObject;
        const SZrTypeValue *stackLeftValue;
        const SZrTypeValue *stackRightValue;
        SZrTypeValue copiedReceiver;
        SZrObject *copiedReceiverObject;
        const SZrTypeValue *copiedLeftValue;
        const SZrTypeValue *copiedRightValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "source_struct_runtime_ctor_manual");
        TEST_ASSERT_NOT_NULL(module);

        pairPrototype = get_module_exported_prototype(state, module, "Pair");
        TEST_ASSERT_NOT_NULL(pairPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, pairPrototype->type);

        constructorValue = get_object_field_value(state, &pairPrototype->super, "__constructor");
        TEST_ASSERT_NOT_NULL(constructorValue);
        constructorFunction = debug_resolve_function_from_value(state, constructorValue);
        TEST_ASSERT_NOT_NULL(constructorFunction);

        pairObject = ZrCore_Object_New(state, pairPrototype);
        TEST_ASSERT_NOT_NULL(pairObject);
        ZrCore_Object_Init(state, pairObject);
        pairObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(pairObject)));

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(pairObject));
        receiver.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &args[0], 1);
        ZrCore_Value_InitAsInt(state, &args[1], 2);

        callBase = state->stackTop.valuePointer;
        callBase = ZrCore_Function_ReserveScratchSlots(state, 4, callBase);
        TEST_ASSERT_NOT_NULL(callBase);

        ZrCore_Stack_CopyValue(state, callBase, constructorValue);
        ZrCore_Stack_CopyValue(state, callBase + 1, &receiver);
        ZrCore_Stack_CopyValue(state, callBase + 2, &args[0]);
        ZrCore_Stack_CopyValue(state, callBase + 3, &args[1]);
        state->stackTop.valuePointer = callBase + 4;

        callInfo = ZrCore_Function_PreCall(state, callBase, 1, ZR_NULL);
        TEST_ASSERT_NOT_NULL(callInfo);
        callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        ZrCore_Execute(state, callInfo);
        callBase = callInfo->functionBase.valuePointer;
        TEST_ASSERT_NOT_NULL(callBase);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

        stackReceiverValue = ZrCore_Stack_GetValue(callBase + 1);
        TEST_ASSERT_NOT_NULL(stackReceiverValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, stackReceiverValue->type);
        stackReceiverObject = ZR_CAST_OBJECT(state, stackReceiverValue->value.object);
        TEST_ASSERT_NOT_NULL(stackReceiverObject);
        stackLeftValue = get_object_field_value(state, stackReceiverObject, "left");
        stackRightValue = get_object_field_value(state, stackReceiverObject, "right");
        TEST_ASSERT_NOT_NULL(stackLeftValue);
        TEST_ASSERT_NOT_NULL(stackRightValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(stackLeftValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(stackRightValue->type));
        TEST_ASSERT_EQUAL_INT64(1, stackLeftValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(2, stackRightValue->value.nativeObject.nativeInt64);

        ZrCore_Value_ResetAsNull(&copiedReceiver);
        ZrCore_Value_Copy(state, &copiedReceiver, stackReceiverValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, copiedReceiver.type);
        copiedReceiverObject = ZR_CAST_OBJECT(state, copiedReceiver.value.object);
        TEST_ASSERT_NOT_NULL(copiedReceiverObject);
        copiedLeftValue = get_object_field_value(state, copiedReceiverObject, "left");
        copiedRightValue = get_object_field_value(state, copiedReceiverObject, "right");
        TEST_ASSERT_NOT_NULL(copiedLeftValue);
        TEST_ASSERT_NOT_NULL(copiedRightValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(copiedLeftValue->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(copiedRightValue->type));
        TEST_ASSERT_EQUAL_INT64(1, copiedLeftValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(2, copiedRightValue->value.nativeObject.nativeInt64);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(pairObject)));

        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_struct_method_call_writebacks_receiver_state(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "source_struct_runtime_method",
                    "%module \"source_struct_runtime_method\";\n"
                    "pub struct Counter {\n"
                    "    pub var value: int;\n"
                    "\n"
                    "    pub @constructor(start: int) {\n"
                    "        this.value = start;\n"
                    "    }\n"
                    "\n"
                    "    pub bump(delta: int): int {\n"
                    "        this.value = this.value + delta;\n"
                    "        return this.value;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "pub score(): int {\n"
                    "    var counter = $Counter(5);\n"
                    "    var bumped = counter.bump(2);\n"
                    "    return bumped * 10 + counter.value;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source module struct method call writebacks receiver state";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var sample = %import(\"source_struct_runtime_method\");\n"
                "return sample.score();\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "source_module_struct_method_runtime_test.zr", 43);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(77, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_struct_value_construction_preserves_field_access(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "source_struct_runtime",
                    "%module \"source_struct_runtime\";\n"
                    "pub struct Pair {\n"
                    "    pub var left: int;\n"
                    "    pub var right: int;\n"
                    "\n"
                    "    pub @constructor(left: int, right: int) {\n"
                    "        this.left = left;\n"
                    "        this.right = right;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "pub score(): int {\n"
                    "    var pair = $Pair(1, 2);\n"
                    "    return pair.left + pair.right;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source module struct value construction preserves field access";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var sample = %import(\"source_struct_runtime\");\n"
                "return sample.score();\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObjectModule *module;
        SZrObjectPrototype *pairPrototype;
        TZrUInt64 leftOffset = 0;
        TZrUInt64 rightOffset = 0;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "source_struct_runtime");
        TEST_ASSERT_NOT_NULL(module);

        pairPrototype = get_module_exported_prototype(state, module, "Pair");
        TEST_ASSERT_NOT_NULL(pairPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, pairPrototype->type);
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)pairPrototype, "left", &leftOffset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)pairPrototype, "right", &rightOffset));
        TEST_ASSERT_EQUAL_UINT64(0, leftOffset);
        TEST_ASSERT_TRUE(rightOffset > leftOffset);

        sourceName = ZrCore_String_Create(state, "source_module_struct_runtime_test.zr", 36);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(3, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_source_module_class_boxed_construction_preserves_field_access(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "source_class_runtime",
                    "%module \"source_class_runtime\";\n"
                    "pub class Box {\n"
                    "    pub var value: int;\n"
                    "\n"
                    "    pub @constructor(value: int) {\n"
                    "        this.value = value;\n"
                    "    }\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Source module class boxed construction preserves field access";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var sample = %import(\"source_class_runtime\");\n"
                "var box = new sample.Box(7);\n"
                "return box.value;\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObjectModule *module;
        SZrObjectPrototype *boxPrototype;
        const SZrTypeValue *constructorValue;
        SZrFunction *constructorFunction;
        SZrObject *seedObject;
        SZrTypeValue seedValue;
        SZrTypeValue typeNameValue;
        SZrTypeValue convertedValue;
        SZrObject *convertedObject;
        TZrBool runtimeSucceeded;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "source_class_runtime");
        TEST_ASSERT_NOT_NULL(module);

        boxPrototype = get_module_exported_prototype(state, module, "Box");
        TEST_ASSERT_NOT_NULL(boxPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_CLASS, boxPrototype->type);

        constructorValue = get_object_field_value(state, &boxPrototype->super, "__constructor");
        TEST_ASSERT_NOT_NULL(constructorValue);
        constructorFunction = debug_resolve_function_from_value(state, constructorValue);
        TEST_ASSERT_NOT_NULL(constructorFunction);

        seedObject = ZrCore_Object_New(state, ZR_NULL);
        TEST_ASSERT_NOT_NULL(seedObject);
        ZrCore_Object_Init(state, seedObject);
        ZrCore_Value_InitAsRawObject(state, &seedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(seedObject));
        seedValue.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsRawObject(state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(boxPrototype->name));
        typeNameValue.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Value_ResetAsNull(&convertedValue);
        TEST_ASSERT_TRUE(ZrCore_Execution_ToObject(state, ZR_NULL, &convertedValue, &seedValue, &typeNameValue));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, convertedValue.type);
        convertedObject = ZR_CAST_OBJECT(state, convertedValue.value.object);
        TEST_ASSERT_NOT_NULL(convertedObject);
        TEST_ASSERT_EQUAL_PTR(boxPrototype, convertedObject->prototype);

        sourceName = ZrCore_String_Create(state, "source_module_class_runtime_test.zr", 35);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(entryFunction, ZR_INSTRUCTION_ENUM(CREATE_OBJECT)));
        TEST_ASSERT_TRUE(function_contains_to_object_type_name(entryFunction, "Box"));
        TEST_ASSERT_TRUE(function_contains_get_member_name(entryFunction, "__constructor"));
        TEST_ASSERT_TRUE(function_contains_opcode(entryFunction, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));

        runtimeSucceeded = ZrTests_Runtime_Function_Execute(state, entryFunction, &result);
        TEST_ASSERT_TRUE(runtimeSucceeded);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(7, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

typedef struct {
    const TZrChar *modulePath;
    const TZrChar *moduleSource;
    const TZrChar *memberReflectionExpression;
    const TZrChar *sourceNameText;
    const TZrChar *expectedKind;
    const TZrChar *expectedMetadataFlagName;
    const TZrChar *expectedDecoratorName;
    const TZrChar *binaryPath;
} SZrRuntimeMemberDecoratorReflectionCase;

static void assert_runtime_member_decorator_reflection_object(
        SZrState *state,
        SZrObject *reflectionObject,
        const SZrRuntimeMemberDecoratorReflectionCase *testCase) {
    const SZrTypeValue *kindValue;
    const SZrTypeValue *metadataValue;
    const SZrTypeValue *decoratorsValue;
    const SZrTypeValue *flagValue;
    const SZrTypeValue *decoratorNameValue;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    SZrObject *decoratorEntry;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(reflectionObject);
    TEST_ASSERT_NOT_NULL(testCase);

    kindValue = get_object_field_value(state, reflectionObject, "kind");
    metadataValue = get_object_field_value(state, reflectionObject, "metadata");
    decoratorsValue = get_object_field_value(state, reflectionObject, "decorators");
    TEST_ASSERT_NOT_NULL(kindValue);
    TEST_ASSERT_NOT_NULL(metadataValue);
    TEST_ASSERT_NOT_NULL(decoratorsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, kindValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), testCase->expectedKind));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);

    metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
    decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
    TEST_ASSERT_NOT_NULL(metadataObject);
    TEST_ASSERT_NOT_NULL(decoratorsArray);

    flagValue = get_object_field_value(state, metadataObject, testCase->expectedMetadataFlagName);
    TEST_ASSERT_NOT_NULL(flagValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, flagValue->type);
    TEST_ASSERT_TRUE(flagValue->value.nativeObject.nativeBool);

    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
    decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
    TEST_ASSERT_NOT_NULL(decoratorEntry);
    decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
    TEST_ASSERT_NOT_NULL(decoratorNameValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object),
                                           testCase->expectedDecoratorName));
}

static void run_runtime_member_decorator_reflection_test(
        const char *testSummary,
        const SZrRuntimeMemberDecoratorReflectionCase *testCase,
        TZrBool useBinaryFixture) {
    SZrTestTimer timer;
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    char sourceBuffer[256];
    int sourceLength;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrModuleFixtureSource fixture = {0};
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(testCase);

        sourceLength = snprintf(sourceBuffer,
                                sizeof(sourceBuffer),
                                "var decorated = %%import(\"%s\");\nreturn %s;\n",
                                testCase->modulePath,
                                testCase->memberReflectionExpression);
        TEST_ASSERT_TRUE(sourceLength > 0 && (TZrSize)sourceLength < sizeof(sourceBuffer));

        if (useBinaryFixture) {
            TEST_ASSERT_NOT_NULL(testCase->binaryPath);
            binaryBytes = build_module_binary_fixture(state, testCase->moduleSource, testCase->binaryPath, &binaryLength);
            TEST_ASSERT_NOT_NULL(binaryBytes);
            TEST_ASSERT_TRUE(binaryLength > 0);

            fixture.path = testCase->modulePath;
            fixture.source = ZR_NULL;
            fixture.bytes = binaryBytes;
            fixture.length = binaryLength;
            fixture.isBinary = ZR_TRUE;
        } else {
            fixture.path = testCase->modulePath;
            fixture.source = testCase->moduleSource;
            fixture.bytes = ZR_NULL;
            fixture.length = 0;
            fixture.isBinary = ZR_FALSE;
        }

        g_module_fixture_sources = &fixture;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          (TZrNativeString)testCase->sourceNameText,
                                          strlen(testCase->sourceNameText));
        TEST_ASSERT_NOT_NULL(sourceName);
        entryFunction = ZrParser_Source_Compile(state, sourceBuffer, strlen(sourceBuffer), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);
        assert_runtime_member_decorator_reflection_object(state, reflectionObject, testCase);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        if (binaryBytes != ZR_NULL) {
            free(binaryBytes);
        }
        if (useBinaryFixture && testCase->binaryPath != ZR_NULL) {
            remove(testCase->binaryPath);
        }
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_field_reflection_exposes_runtime_field_decorator_metadata(void) {
    static const SZrRuntimeMemberDecoratorReflectionCase kCase = {
            "reflect_runtime_field_decorators",
            "class MarkField {\n"
            "    @decorate(target: %type Field): void {\n"
            "        target.metadata.isRuntimeField = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "pub class User {\n"
            "    #MarkField#\n"
            "    pub var id: int = 1;\n"
            "}\n",
            "%type(decorated.User).members.id[0]",
            "type_source_runtime_field_decorator_reflection_test.zr",
            "field",
            "isRuntimeField",
            "MarkField",
            "test_reflect_runtime_field_decorators_binary_fixture.zro",
    };

    run_runtime_member_decorator_reflection_test(
            "Percent Type Source Field Reflection Exposes Runtime Field Decorator Metadata",
            &kCase,
            ZR_FALSE);
}

static void test_percent_type_source_method_reflection_exposes_runtime_method_decorator_metadata(void) {
    static const SZrRuntimeMemberDecoratorReflectionCase kCase = {
            "reflect_runtime_method_decorators",
            "class MarkMethod {\n"
            "    @decorate(target: %type Method): void {\n"
            "        target.metadata.instrumented = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "pub class User {\n"
            "    #MarkMethod#\n"
            "    pub load(id: int): int {\n"
            "        return id;\n"
            "    }\n"
            "}\n",
            "%type(decorated.User).members.load[0]",
            "type_source_runtime_method_decorator_reflection_test.zr",
            "method",
            "instrumented",
            "MarkMethod",
            "test_reflect_runtime_method_decorators_binary_fixture.zro",
    };

    run_runtime_member_decorator_reflection_test(
            "Percent Type Source Method Reflection Exposes Runtime Method Decorator Metadata",
            &kCase,
            ZR_FALSE);
}

static void test_percent_type_source_property_reflection_exposes_runtime_property_decorator_metadata(void) {
    static const SZrRuntimeMemberDecoratorReflectionCase kCase = {
            "reflect_runtime_property_decorators",
            "class MarkProperty {\n"
            "    @decorate(target: %type Property): void {\n"
            "        target.metadata.observable = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "pub class User {\n"
            "    pri var _value: int = 1;\n"
            "\n"
            "    #MarkProperty#\n"
            "    pub get value: int {\n"
            "        return this._value;\n"
            "    }\n"
            "}\n",
            "%type(decorated.User).members.value[0]",
            "type_source_runtime_property_decorator_reflection_test.zr",
            "property",
            "observable",
            "MarkProperty",
            "test_reflect_runtime_property_decorators_binary_fixture.zro",
    };

    run_runtime_member_decorator_reflection_test(
            "Percent Type Source Property Reflection Exposes Runtime Property Decorator Metadata",
            &kCase,
            ZR_FALSE);
}

static void test_percent_type_source_reflection_exposes_advanced_oop_metadata(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_advanced_oop",
                    "pub abstract class Base {\n"
                    "    pub abstract ping(): int;\n"
                    "    pub abstract get score: int;\n"
                    "}\n"
                    "pub final class Derived : Base {\n"
                    "    pub override final ping(): int { return 1; }\n"
                    "    pub override get final score: int { return 2; }\n"
                    "}\n"
                    "interface Readable { read(): int; }\n"
                    "pub class Device : Readable {\n"
                    "    pub read(): int { return 1; }\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Reflection Exposes Advanced OOP Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var oop = %import(\"reflect_advanced_oop\");\n"
                "return {\n"
                "    base: %type(oop.Base),\n"
                "    derived: %type(oop.Derived),\n"
                "    ping: %type(oop.Derived).members.ping[0],\n"
                "    score: %type(oop.Derived).members.score[0],\n"
                "    read: %type(oop.Device).members.read[0]\n"
                "};\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;
        SZrObject *baseReflection;
        SZrObject *derivedReflection;
        SZrObject *pingReflection;
        SZrObject *scoreReflection;
        SZrObject *readReflection;
        const SZrTypeValue *baseValue;
        const SZrTypeValue *derivedValue;
        const SZrTypeValue *pingValue;
        const SZrTypeValue *scoreValue;
        const SZrTypeValue *readValue;
        const SZrTypeValue *boolValue;
        const SZrTypeValue *intValue;
        const SZrTypeValue *stringValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_source_advanced_oop_reflection_test.zr", 43);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        baseValue = get_object_field_value(state, resultObject, "base");
        derivedValue = get_object_field_value(state, resultObject, "derived");
        pingValue = get_object_field_value(state, resultObject, "ping");
        scoreValue = get_object_field_value(state, resultObject, "score");
        readValue = get_object_field_value(state, resultObject, "read");
        TEST_ASSERT_NOT_NULL(baseValue);
        TEST_ASSERT_NOT_NULL(derivedValue);
        TEST_ASSERT_NOT_NULL(pingValue);
        TEST_ASSERT_NOT_NULL(scoreValue);
        TEST_ASSERT_NOT_NULL(readValue);

        baseReflection = ZR_CAST_OBJECT(state, baseValue->value.object);
        derivedReflection = ZR_CAST_OBJECT(state, derivedValue->value.object);
        pingReflection = ZR_CAST_OBJECT(state, pingValue->value.object);
        scoreReflection = ZR_CAST_OBJECT(state, scoreValue->value.object);
        readReflection = ZR_CAST_OBJECT(state, readValue->value.object);
        TEST_ASSERT_NOT_NULL(baseReflection);
        TEST_ASSERT_NOT_NULL(derivedReflection);
        TEST_ASSERT_NOT_NULL(pingReflection);
        TEST_ASSERT_NOT_NULL(scoreReflection);
        TEST_ASSERT_NOT_NULL(readReflection);

        boolValue = get_object_field_value(state, baseReflection, "isAbstract");
        TEST_ASSERT_NOT_NULL(boolValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, boolValue->type);
        TEST_ASSERT_TRUE(boolValue->value.nativeObject.nativeBool);

        boolValue = get_object_field_value(state, derivedReflection, "isFinal");
        intValue = get_object_field_value(state, derivedReflection, "nextVirtualSlotIndex");
        TEST_ASSERT_NOT_NULL(boolValue);
        TEST_ASSERT_NOT_NULL(intValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, boolValue->type);
        TEST_ASSERT_TRUE(boolValue->value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(intValue->type));
        TEST_ASSERT_EQUAL_INT64(2, intValue->value.nativeObject.nativeInt64);

        stringValue = get_object_field_value(state, pingReflection, "kind");
        TEST_ASSERT_NOT_NULL(stringValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, stringValue->value.object), "method"));

        boolValue = get_object_field_value(state, pingReflection, "isOverride");
        TEST_ASSERT_NOT_NULL(boolValue);
        TEST_ASSERT_TRUE(boolValue->value.nativeObject.nativeBool);
        boolValue = get_object_field_value(state, pingReflection, "isFinal");
        TEST_ASSERT_NOT_NULL(boolValue);
        TEST_ASSERT_TRUE(boolValue->value.nativeObject.nativeBool);
        stringValue = get_object_field_value(state, pingReflection, "ownerTypeName");
        TEST_ASSERT_NOT_NULL(stringValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, stringValue->value.object), "Derived"));
        stringValue = get_object_field_value(state, pingReflection, "baseDefinitionOwnerTypeName");
        TEST_ASSERT_NOT_NULL(stringValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, stringValue->value.object), "Base"));
        stringValue = get_object_field_value(state, pingReflection, "baseDefinitionName");
        TEST_ASSERT_NOT_NULL(stringValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, stringValue->value.object), "ping"));
        intValue = get_object_field_value(state, pingReflection, "virtualSlotIndex");
        TEST_ASSERT_NOT_NULL(intValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(intValue->type));
        TEST_ASSERT_NOT_EQUAL_INT64(-1, intValue->value.nativeObject.nativeInt64);

        stringValue = get_object_field_value(state, scoreReflection, "kind");
        TEST_ASSERT_NOT_NULL(stringValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, stringValue->value.object), "property"));
        boolValue = get_object_field_value(state, scoreReflection, "isOverride");
        TEST_ASSERT_NOT_NULL(boolValue);
        TEST_ASSERT_TRUE(boolValue->value.nativeObject.nativeBool);
        intValue = get_object_field_value(state, scoreReflection, "propertyIdentity");
        TEST_ASSERT_NOT_NULL(intValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(intValue->type));
        TEST_ASSERT_NOT_EQUAL_INT64(-1, intValue->value.nativeObject.nativeInt64);
        intValue = get_object_field_value(state, scoreReflection, "accessorRole");
        TEST_ASSERT_NOT_NULL(intValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(intValue->type));
        TEST_ASSERT_EQUAL_INT64(1, intValue->value.nativeObject.nativeInt64);
        stringValue = get_object_field_value(state, scoreReflection, "baseDefinitionName");
        TEST_ASSERT_NOT_NULL(stringValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, stringValue->value.object), "__get_score"));

        intValue = get_object_field_value(state, readReflection, "interfaceContractSlot");
        TEST_ASSERT_NOT_NULL(intValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(intValue->type));
        TEST_ASSERT_NOT_EQUAL_INT64(-1, intValue->value.nativeObject.nativeInt64);
        boolValue = get_object_field_value(state, readReflection, "isOverride");
        TEST_ASSERT_NOT_NULL(boolValue);
        TEST_ASSERT_FALSE(boolValue->value.nativeObject.nativeBool);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_binary_type_reflection_exposes_runtime_class_decorator_metadata(void) {
    static const TZrChar *kModuleSource =
            "class RuntimeSerializable {\n"
            "    @decorate(target: %type Class): void {\n"
            "        target.metadata.runtimeSerializable = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "#RuntimeSerializable#\n"
            "pub class User {\n"
            "    pub var id: int = 1;\n"
            "}\n";
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Binary Type Reflection Exposes Runtime Class Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "test_reflect_runtime_class_decorators_binary_fixture.zro";
    SZrModuleFixtureSource fixtures[1];

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"reflect_runtime_class_decorators_binary\");\n"
                "return %type(decorated.User);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *metadataValue;
        const SZrTypeValue *decoratorsValue;
        SZrObject *metadataObject;
        SZrObject *decoratorsArray;
        SZrObject *decoratorEntry;
        const SZrTypeValue *runtimeSerializableValue;
        const SZrTypeValue *decoratorNameValue;

        TEST_ASSERT_NOT_NULL(state);

        binaryBytes = build_module_binary_fixture(state, kModuleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "reflect_runtime_class_decorators_binary";
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;
        fixtures[0].source = ZR_NULL;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_binary_runtime_class_decorator_reflection_test.zr", 55);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        metadataValue = get_object_field_value(state, reflectionObject, "metadata");
        decoratorsValue = get_object_field_value(state, reflectionObject, "decorators");
        TEST_ASSERT_NOT_NULL(metadataValue);
        TEST_ASSERT_NOT_NULL(decoratorsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);

        metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
        decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
        TEST_ASSERT_NOT_NULL(metadataObject);
        TEST_ASSERT_NOT_NULL(decoratorsArray);

        runtimeSerializableValue = get_object_field_value(state, metadataObject, "runtimeSerializable");
        TEST_ASSERT_NOT_NULL(runtimeSerializableValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, runtimeSerializableValue->type);
        TEST_ASSERT_TRUE(runtimeSerializableValue->value.nativeObject.nativeBool);

        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
        decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
        TEST_ASSERT_NOT_NULL(decoratorEntry);
        decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
        TEST_ASSERT_NOT_NULL(decoratorNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object),
                                               "RuntimeSerializable"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_binary_function_reflection_exposes_runtime_function_decorator_metadata(void) {
    static const TZrChar *kModuleSource =
            "func markRuntime(target: %type Function): void {\n"
            "    target.metadata.instrumented = true;\n"
            "}\n"
            "\n"
            "#markRuntime#\n"
            "load(id: int): int {\n"
            "    return id;\n"
            "}\n"
            "\n"
            "pub var exportedLoad = load;\n";
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Binary Function Reflection Exposes Runtime Function Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "test_reflect_runtime_function_decorators_binary_fixture.zro";
    SZrModuleFixtureSource fixtures[1];

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"reflect_runtime_function_decorators_binary\");\n"
                "return %type(decorated.exportedLoad);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *metadataValue;
        const SZrTypeValue *decoratorsValue;
        SZrObject *metadataObject;
        SZrObject *decoratorsArray;
        SZrObject *decoratorEntry;
        const SZrTypeValue *instrumentedValue;
        const SZrTypeValue *decoratorNameValue;

        TEST_ASSERT_NOT_NULL(state);

        binaryBytes = build_module_binary_fixture(state, kModuleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "reflect_runtime_function_decorators_binary";
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;
        fixtures[0].source = ZR_NULL;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_binary_runtime_function_decorator_reflection_test.zr", 58);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        metadataValue = get_object_field_value(state, reflectionObject, "metadata");
        decoratorsValue = get_object_field_value(state, reflectionObject, "decorators");
        TEST_ASSERT_NOT_NULL(metadataValue);
        TEST_ASSERT_NOT_NULL(decoratorsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);

        metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
        decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
        TEST_ASSERT_NOT_NULL(metadataObject);
        TEST_ASSERT_NOT_NULL(decoratorsArray);

        instrumentedValue = get_object_field_value(state, metadataObject, "instrumented");
        TEST_ASSERT_NOT_NULL(instrumentedValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, instrumentedValue->type);
        TEST_ASSERT_TRUE(instrumentedValue->value.nativeObject.nativeBool);

        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
        decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
        TEST_ASSERT_NOT_NULL(decoratorEntry);
        decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
        TEST_ASSERT_NOT_NULL(decoratorNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object), "markRuntime"));

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_binary_runtime_decorated_pub_function_is_directly_callable_from_imported_module(void) {
    static const TZrChar *kModuleSource =
            "func markRuntime(target: %type Function): void {\n"
            "    target.metadata.instrumented = true;\n"
            "}\n"
            "\n"
            "#markRuntime#\n"
            "pub load(id: int): int {\n"
            "    var meta = %type(load).metadata;\n"
            "    return meta.instrumented ? id + 1 : id;\n"
            "}\n";
    SZrTestTimer timer;
    const char *testSummary = "Binary runtime decorated pub function is directly callable from imported module";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "test_call_runtime_function_decorators_binary_fixture.zro";
    SZrModuleFixtureSource fixtures[1];

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var decorated = %import(\"call_runtime_function_decorators_binary\");\n"
                "return decorated.load(41);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        binaryBytes = build_module_binary_fixture(state, kModuleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "call_runtime_function_decorators_binary";
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;
        fixtures[0].source = ZR_NULL;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "binary_runtime_decorated_pub_function_import_test.zr", 52);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_binary_field_reflection_exposes_runtime_field_decorator_metadata(void) {
    static const SZrRuntimeMemberDecoratorReflectionCase kCase = {
            "reflect_runtime_field_decorators_binary",
            "class MarkField {\n"
            "    @decorate(target: %type Field): void {\n"
            "        target.metadata.isRuntimeField = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "pub class User {\n"
            "    #MarkField#\n"
            "    pub var id: int = 1;\n"
            "}\n",
            "%type(decorated.User).members.id[0]",
            "type_binary_runtime_field_decorator_reflection_test.zr",
            "field",
            "isRuntimeField",
            "MarkField",
            "test_reflect_runtime_field_decorators_binary_fixture.zro",
    };

    run_runtime_member_decorator_reflection_test(
            "Percent Type Binary Field Reflection Exposes Runtime Field Decorator Metadata",
            &kCase,
            ZR_TRUE);
}

static void test_percent_type_binary_method_reflection_exposes_runtime_method_decorator_metadata(void) {
    static const SZrRuntimeMemberDecoratorReflectionCase kCase = {
            "reflect_runtime_method_decorators_binary",
            "class MarkMethod {\n"
            "    @decorate(target: %type Method): void {\n"
            "        target.metadata.instrumented = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "pub class User {\n"
            "    #MarkMethod#\n"
            "    pub load(id: int): int {\n"
            "        return id;\n"
            "    }\n"
            "}\n",
            "%type(decorated.User).members.load[0]",
            "type_binary_runtime_method_decorator_reflection_test.zr",
            "method",
            "instrumented",
            "MarkMethod",
            "test_reflect_runtime_method_decorators_binary_fixture.zro",
    };

    run_runtime_member_decorator_reflection_test(
            "Percent Type Binary Method Reflection Exposes Runtime Method Decorator Metadata",
            &kCase,
            ZR_TRUE);
}

static void test_percent_type_binary_property_reflection_exposes_runtime_property_decorator_metadata(void) {
    static const SZrRuntimeMemberDecoratorReflectionCase kCase = {
            "reflect_runtime_property_decorators_binary",
            "class MarkProperty {\n"
            "    @decorate(target: %type Property): void {\n"
            "        target.metadata.observable = true;\n"
            "    }\n"
            "}\n"
            "\n"
            "pub class User {\n"
            "    pri var _value: int = 1;\n"
            "\n"
            "    #MarkProperty#\n"
            "    pub get value: int {\n"
            "        return this._value;\n"
            "    }\n"
            "}\n",
            "%type(decorated.User).members.value[0]",
            "type_binary_runtime_property_decorator_reflection_test.zr",
            "property",
            "observable",
            "MarkProperty",
            "test_reflect_runtime_property_decorators_binary_fixture.zro",
    };

    run_runtime_member_decorator_reflection_test(
            "Percent Type Binary Property Reflection Exposes Runtime Property Decorator Metadata",
            &kCase,
            ZR_TRUE);
}

static void test_percent_type_binary_reflection_restores_advanced_oop_metadata(void) {
    static const TZrChar *kModuleSource =
            "pub abstract class Base {\n"
            "    pub abstract ping(): int;\n"
            "    pub abstract get score: int;\n"
            "}\n"
            "pub final class Derived : Base {\n"
            "    pub override final ping(): int { return 1; }\n"
            "    pub override get final score: int { return 2; }\n"
            "}\n"
            "interface Readable { read(): int; }\n"
            "pub class Device : Readable {\n"
            "    pub read(): int { return 1; }\n"
            "}\n";
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Binary Reflection Restores Advanced OOP Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "test_reflect_advanced_oop_binary_fixture.zro";
    SZrModuleFixtureSource fixtures[1];

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var oop = %import(\"reflect_advanced_oop_binary\");\n"
                "return {\n"
                "    base: %type(oop.Base),\n"
                "    derived: %type(oop.Derived),\n"
                "    ping: %type(oop.Derived).members.ping[0],\n"
                "    score: %type(oop.Derived).members.score[0],\n"
                "    read: %type(oop.Device).members.read[0]\n"
                "};\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;
        SZrObject *baseReflection;
        SZrObject *derivedReflection;
        SZrObject *pingReflection;
        SZrObject *scoreReflection;
        SZrObject *readReflection;
        const SZrTypeValue *fieldValue;

        TEST_ASSERT_NOT_NULL(state);

        binaryBytes = build_module_binary_fixture(state, kModuleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "reflect_advanced_oop_binary";
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;
        fixtures[0].source = ZR_NULL;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state, "type_binary_advanced_oop_reflection_test.zr", 43);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        fieldValue = get_object_field_value(state, resultObject, "base");
        TEST_ASSERT_NOT_NULL(fieldValue);
        baseReflection = ZR_CAST_OBJECT(state, fieldValue->value.object);
        fieldValue = get_object_field_value(state, resultObject, "derived");
        TEST_ASSERT_NOT_NULL(fieldValue);
        derivedReflection = ZR_CAST_OBJECT(state, fieldValue->value.object);
        fieldValue = get_object_field_value(state, resultObject, "ping");
        TEST_ASSERT_NOT_NULL(fieldValue);
        pingReflection = ZR_CAST_OBJECT(state, fieldValue->value.object);
        fieldValue = get_object_field_value(state, resultObject, "score");
        TEST_ASSERT_NOT_NULL(fieldValue);
        scoreReflection = ZR_CAST_OBJECT(state, fieldValue->value.object);
        fieldValue = get_object_field_value(state, resultObject, "read");
        TEST_ASSERT_NOT_NULL(fieldValue);
        readReflection = ZR_CAST_OBJECT(state, fieldValue->value.object);

        TEST_ASSERT_NOT_NULL(baseReflection);
        TEST_ASSERT_NOT_NULL(derivedReflection);
        TEST_ASSERT_NOT_NULL(pingReflection);
        TEST_ASSERT_NOT_NULL(scoreReflection);
        TEST_ASSERT_NOT_NULL(readReflection);

        fieldValue = get_object_field_value(state, baseReflection, "isAbstract");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_TRUE(fieldValue->value.nativeObject.nativeBool);

        fieldValue = get_object_field_value(state, derivedReflection, "isFinal");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_TRUE(fieldValue->value.nativeObject.nativeBool);
        fieldValue = get_object_field_value(state, derivedReflection, "nextPropertyIdentity");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_EQUAL_INT64(1, fieldValue->value.nativeObject.nativeInt64);

        fieldValue = get_object_field_value(state, pingReflection, "baseDefinitionOwnerTypeName");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, fieldValue->value.object), "Base"));
        fieldValue = get_object_field_value(state, pingReflection, "baseDefinitionName");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, fieldValue->value.object), "ping"));
        fieldValue = get_object_field_value(state, pingReflection, "virtualSlotIndex");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_NOT_EQUAL_INT64(-1, fieldValue->value.nativeObject.nativeInt64);

        fieldValue = get_object_field_value(state, scoreReflection, "propertyIdentity");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_NOT_EQUAL_INT64(-1, fieldValue->value.nativeObject.nativeInt64);
        fieldValue = get_object_field_value(state, scoreReflection, "accessorRole");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_EQUAL_INT64(1, fieldValue->value.nativeObject.nativeInt64);

        fieldValue = get_object_field_value(state, readReflection, "interfaceContractSlot");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_NOT_EQUAL_INT64(-1, fieldValue->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_percent_type_source_module_reflection_preserves_compile_time_metadata_across_full_gc(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "reflect_meta_gc",
                    "%compileTime var MAX_SCALE: int = 8;\n"
                    "%compileTime buildBias(seed: int): int {\n"
                    "    return seed + MAX_SCALE;\n"
                    "}\n"
                    "\n"
                    "pub var runtimeValue: int = MAX_SCALE;\n"
                    "\n"
                    "%test(\"vector_meta\") {\n"
                    "    return runtimeValue;\n"
                    "}\n"),
    };
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Module Reflection Preserves Compile Time Metadata Across Full GC";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *loadSource = "return %import(\"reflect_meta_gc\");\n";
        const TZrChar *reflectSource =
                "var meta = %import(\"reflect_meta_gc\");\n"
                "return %type(meta);\n";
        SZrString *loadSourceName;
        SZrString *reflectSourceName;
        SZrFunction *loadFunction = ZR_NULL;
        SZrFunction *reflectFunction = ZR_NULL;
        SZrTypeValue result;
        SZrObject *reflectionObject;
        const SZrTypeValue *compileTimeValue;
        const SZrTypeValue *compileTimeFunctionsValue;
        SZrObject *compileTimeObject;
        SZrObject *compileTimeFunctions;
        SZrObject *compileTimeFunction;
        const SZrTypeValue *functionParametersValue;
        SZrObject *functionParameters;
        SZrObject *firstParameter;
        const SZrTypeValue *parameterNameValue;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        loadSourceName = ZrCore_String_Create(state, "type_source_module_gc_load_test.zr", 34);
        loadFunction = ZrParser_Source_Compile(state, loadSource, strlen(loadSource), loadSourceName);
        TEST_ASSERT_NOT_NULL(loadFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, loadFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
        ZrCore_Function_Free(state, loadFunction);
        loadFunction = ZR_NULL;

        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);

        reflectSourceName = ZrCore_String_Create(state, "type_source_module_gc_reflect_test.zr", 37);
        reflectFunction = ZrParser_Source_Compile(state, reflectSource, strlen(reflectSource), reflectSourceName);
        TEST_ASSERT_NOT_NULL(reflectFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, reflectFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(reflectionObject);

        compileTimeValue = get_object_field_value(state, reflectionObject, "compileTime");
        TEST_ASSERT_NOT_NULL(compileTimeValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, compileTimeValue->type);

        compileTimeObject = ZR_CAST_OBJECT(state, compileTimeValue->value.object);
        TEST_ASSERT_NOT_NULL(compileTimeObject);

        compileTimeFunctionsValue = get_object_field_value(state, compileTimeObject, "functions");
        TEST_ASSERT_NOT_NULL(compileTimeFunctionsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeFunctionsValue->type);

        compileTimeFunctions = ZR_CAST_OBJECT(state, compileTimeFunctionsValue->value.object);
        TEST_ASSERT_NOT_NULL(compileTimeFunctions);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(compileTimeFunctions));

        compileTimeFunction = get_array_entry_object(state, compileTimeFunctions, 0);
        TEST_ASSERT_NOT_NULL(compileTimeFunction);

        functionParametersValue = get_object_field_value(state, compileTimeFunction, "parameters");
        TEST_ASSERT_NOT_NULL(functionParametersValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, functionParametersValue->type);

        functionParameters = ZR_CAST_OBJECT(state, functionParametersValue->value.object);
        TEST_ASSERT_NOT_NULL(functionParameters);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(functionParameters));

        firstParameter = get_array_entry_object(state, functionParameters, 0);
        TEST_ASSERT_NOT_NULL(firstParameter);
        parameterNameValue = get_object_field_value(state, firstParameter, "name");
        TEST_ASSERT_NOT_NULL(parameterNameValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, parameterNameValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterNameValue->value.object), "seed"));

        ZrCore_Function_Free(state, reflectFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_registry_rejects_future_runtime_abi(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Registry Rejects Future Runtime ABI";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *errorMessage;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &kProbeFutureAbiModuleDescriptor));
        TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_VERSION_MISMATCH,
                              ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
        errorMessage = ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global);
        TEST_ASSERT_NOT_NULL(errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "probe.future_abi"));
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "requires runtime ABI"));
        TEST_ASSERT_NULL(ZrLibrary_NativeRegistry_FindModule(state->global, "probe.future_abi"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_registry_rejects_unsupported_capabilities(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Registry Rejects Unsupported Capabilities";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *errorMessage;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_FALSE(
                ZrLibrary_NativeRegistry_RegisterModule(state->global, &kProbeUnsupportedCapabilityModuleDescriptor));
        TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_CAPABILITY_MISMATCH,
                              ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
        errorMessage = ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global);
        TEST_ASSERT_NOT_NULL(errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "probe.unsupported_capability"));
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "unsupported capabilities"));
        TEST_ASSERT_NULL(ZrLibrary_NativeRegistry_FindModule(state->global, "probe.unsupported_capability"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_reference_protocols_native_iterable_fixture_uses_registered_contracts(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Protocols Native Iterable Fixture Uses Registered Contracts";
    SZrState *state = ZR_NULL;
    TZrSize manifestSize = 0;
    TZrSize sourceSize = 0;
    char *manifestText = ZR_NULL;
    char *source = ZR_NULL;
    SZrObjectModule *module = ZR_NULL;
    const SZrTypeValue *moduleInfoValue = ZR_NULL;
    const SZrTypeValue *typesValue = ZR_NULL;
    SZrObject *moduleInfo = ZR_NULL;
    SZrObject *typesArray = ZR_NULL;
    SZrObject *iterableEntry = ZR_NULL;
    SZrObject *iteratorEntry = ZR_NULL;
    const SZrTypeValue *protocolMaskValue = ZR_NULL;
    const SZrTypeValue *implementsValue = ZR_NULL;
    const SZrTypeValue *methodsValue = ZR_NULL;
    const SZrTypeValue *fieldsValue = ZR_NULL;
    SZrObject *implementsArray = ZR_NULL;
    SZrObject *methodsArray = ZR_NULL;
    SZrObject *fieldsArray = ZR_NULL;
    SZrObject *getIteratorEntry = ZR_NULL;
    SZrObject *moveNextEntry = ZR_NULL;
    SZrObject *currentFieldEntry = ZR_NULL;
    const SZrTypeValue *contractRoleValue = ZR_NULL;
    SZrObjectPrototype *iterablePrototype = ZR_NULL;
    SZrObjectPrototype *iteratorPrototype = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;
    TZrInt64 result = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(register_probe_native_module(state));

    manifestText = read_reference_file("core_semantics/protocols_iteration_comparable/manifest.json", &manifestSize);
    TEST_ASSERT_NOT_NULL(manifestText);
    TEST_ASSERT_TRUE(manifestSize > 0);
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "protocols-native-iterable-pass"));

    module = import_native_module(state, "probe.native_shapes");
    TEST_ASSERT_NOT_NULL(module);

    moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    TEST_ASSERT_NOT_NULL(moduleInfoValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);

    typesValue = get_object_field_value(state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
    typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
    TEST_ASSERT_NOT_NULL(typesArray);

    iterableEntry = find_named_entry_in_array(state, typesArray, "name", "NativeCounterIterable");
    iteratorEntry = find_named_entry_in_array(state, typesArray, "name", "NativeCounterIterator");
    TEST_ASSERT_NOT_NULL(iterableEntry);
    TEST_ASSERT_NOT_NULL(iteratorEntry);

    protocolMaskValue = get_object_field_value(state, iterableEntry, "protocolMask");
    TEST_ASSERT_NOT_NULL(protocolMaskValue);
    TEST_ASSERT_TRUE((((TZrUInt64)protocolMaskValue->value.nativeObject.nativeInt64) &
                      ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)) != 0);

    protocolMaskValue = get_object_field_value(state, iteratorEntry, "protocolMask");
    TEST_ASSERT_NOT_NULL(protocolMaskValue);
    TEST_ASSERT_TRUE((((TZrUInt64)protocolMaskValue->value.nativeObject.nativeInt64) &
                      ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR)) != 0);

    implementsValue = get_object_field_value(state, iterableEntry, "implements");
    TEST_ASSERT_NOT_NULL(implementsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, implementsValue->type);
    implementsArray = ZR_CAST_OBJECT(state, implementsValue->value.object);
    TEST_ASSERT_NOT_NULL(implementsArray);
    TEST_ASSERT_NOT_NULL(find_string_entry_in_array(state, implementsArray, "zr.builtin.IEnumerable<int>"));

    implementsValue = get_object_field_value(state, iteratorEntry, "implements");
    TEST_ASSERT_NOT_NULL(implementsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, implementsValue->type);
    implementsArray = ZR_CAST_OBJECT(state, implementsValue->value.object);
    TEST_ASSERT_NOT_NULL(implementsArray);
    TEST_ASSERT_NOT_NULL(find_string_entry_in_array(state, implementsArray, "zr.builtin.IEnumerator<int>"));

    methodsValue = get_object_field_value(state, iterableEntry, "methods");
    TEST_ASSERT_NOT_NULL(methodsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, methodsValue->type);
    methodsArray = ZR_CAST_OBJECT(state, methodsValue->value.object);
    TEST_ASSERT_NOT_NULL(methodsArray);
    getIteratorEntry = find_named_entry_in_array(state, methodsArray, "name", "getIterator");
    TEST_ASSERT_NOT_NULL(getIteratorEntry);
    contractRoleValue = get_object_field_value(state, getIteratorEntry, "contractRole");
    TEST_ASSERT_NOT_NULL(contractRoleValue);
    TEST_ASSERT_EQUAL_INT64(ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT, contractRoleValue->value.nativeObject.nativeInt64);

    methodsValue = get_object_field_value(state, iteratorEntry, "methods");
    TEST_ASSERT_NOT_NULL(methodsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, methodsValue->type);
    methodsArray = ZR_CAST_OBJECT(state, methodsValue->value.object);
    TEST_ASSERT_NOT_NULL(methodsArray);
    moveNextEntry = find_named_entry_in_array(state, methodsArray, "name", "moveNext");
    TEST_ASSERT_NOT_NULL(moveNextEntry);
    contractRoleValue = get_object_field_value(state, moveNextEntry, "contractRole");
    TEST_ASSERT_NOT_NULL(contractRoleValue);
    TEST_ASSERT_EQUAL_INT64(ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT,
                            contractRoleValue->value.nativeObject.nativeInt64);

    fieldsValue = get_object_field_value(state, iteratorEntry, "fields");
    TEST_ASSERT_NOT_NULL(fieldsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, fieldsValue->type);
    fieldsArray = ZR_CAST_OBJECT(state, fieldsValue->value.object);
    TEST_ASSERT_NOT_NULL(fieldsArray);
    currentFieldEntry = find_named_entry_in_array(state, fieldsArray, "name", "current");
    TEST_ASSERT_NOT_NULL(currentFieldEntry);
    contractRoleValue = get_object_field_value(state, currentFieldEntry, "contractRole");
    TEST_ASSERT_NOT_NULL(contractRoleValue);
    TEST_ASSERT_EQUAL_INT64(ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD,
                            contractRoleValue->value.nativeObject.nativeInt64);

    iterablePrototype = get_module_exported_prototype(state, module, "NativeCounterIterable");
    iteratorPrototype = get_module_exported_prototype(state, module, "NativeCounterIterator");
    TEST_ASSERT_NOT_NULL(iterablePrototype);
    TEST_ASSERT_NOT_NULL(iteratorPrototype);
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_ImplementsProtocol(iterablePrototype, ZR_PROTOCOL_ID_ITERABLE));
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_ImplementsProtocol(iteratorPrototype, ZR_PROTOCOL_ID_ITERATOR));

    source = read_reference_file("core_semantics/protocols_iteration_comparable/native_iterable_pass.zr", &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    sourceName = ZrCore_String_Create(state, "reference_native_iterable_pass.zr", 33);
    TEST_ASSERT_NOT_NULL(sourceName);
    entryFunction = ZrParser_Source_Compile(state, source, sourceSize, sourceName);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(469, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    free(manifestText);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_import_manifest_and_native_member_chain_fixture(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Import Manifest And Native Member Chain Fixture";
    SZrState *state = ZR_NULL;
    TZrSize manifestSize = 0;
    TZrSize sourceSize = 0;
    char *manifestText = ZR_NULL;
    char *source = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;
    TZrInt64 result = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    manifestText = read_reference_file("core_semantics/imports/manifest.json", &manifestSize);
    TEST_ASSERT_NOT_NULL(manifestText);
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "\"feature_group\""));
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "\"imports\""));
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"id\"") >= 6);
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"zr_decision\"") >= 6);

    source = read_reference_file("core_semantics/imports/native_root_member_chain_pass.zr", &sourceSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_native_root_member_chain_pass.zr", 42);
    entryFunction = ZrParser_Source_Compile(state, source, sourceSize, sourceName);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    free(manifestText);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_modules_manifest_duplicate_import_and_hidden_api_cases(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Modules Manifest Duplicate Import And Hidden API Cases";
    SZrState *state = ZR_NULL;
    TZrSize manifestSize = 0;
    char *manifestText = ZR_NULL;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    manifestText = read_reference_file("core_semantics/modules_imports_artifacts/manifest.json", &manifestSize);
    TEST_ASSERT_NOT_NULL(manifestText);
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "modules-duplicate-import-identity"));
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "modules-hidden-internal-import-api-reject"));
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"module/project\"") >= 6);

    {
        SZrString *modulePath = ZrCore_String_Create(state, "zr.system", 9);
        SZrObjectModule *firstImport = ZR_NULL;
        SZrObjectModule *cachedImport = ZR_NULL;

        TEST_ASSERT_NOT_NULL(modulePath);

        for (TZrSize index = 0; index < 16; index++) {
            cachedImport = ZrCore_Module_ImportByPath(state, modulePath);
            TEST_ASSERT_NOT_NULL(cachedImport);
            if (index == 0) {
                firstImport = cachedImport;
            } else {
                TEST_ASSERT_EQUAL_PTR(firstImport, cachedImport);
            }
        }
    }

    TEST_ASSERT_NULL(get_zr_global_value(state, "import"));

    free(manifestText);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_hidden_internal_import_api_fixture_is_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Hidden Internal Import API Fixture Is Rejected";
    SZrState *state = ZR_NULL;
    TZrSize sourceSize = 0;
    char *source = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrCapturedParserDiagnostic diagnostic;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/modules_imports_artifacts/hidden_internal_import_api_reject.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);

    ast = parse_reference_source_with_diagnostic(state,
                                                 source,
                                                 sourceSize,
                                                 "reference_hidden_internal_import_api_reject.zr",
                                                 &diagnostic);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Legacy import() syntax is not supported; use %import"));
    TEST_ASSERT_EQUAL_INT(3, diagnostic.location.start.line);

    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_source_and_binary_import_forms_share_logical_identity(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Source And Binary Import Forms Share Logical Identity";
    SZrState *state = ZR_NULL;
    TZrSize sourceSize = 0;
    char *moduleSource = ZR_NULL;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "reference_same_identity_fixture.zro";
    SZrModuleFixtureSource fixture = {0};
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    SZrString *modulePath = ZR_NULL;
    SZrObjectModule *sourceModule = ZR_NULL;
    SZrObjectModule *cachedBinaryModule = ZR_NULL;
    const SZrTypeValue *markerValue = ZR_NULL;
    TZrUInt64 expectedPathHash = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    moduleSource = read_reference_file("core_semantics/modules_imports_artifacts/source_binary_same_logical_path_module.zr",
                                       &sourceSize);
    TEST_ASSERT_NOT_NULL(moduleSource);

    binaryBytes = build_module_binary_fixture(state, moduleSource, binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    fixture.path = "reference.same_identity";
    fixture.source = moduleSource;
    fixture.bytes = ZR_NULL;
    fixture.length = 0;
    fixture.isBinary = ZR_FALSE;

    g_module_fixture_sources = &fixture;
    g_module_fixture_source_count = 1;
    state->global->sourceLoader = module_fixture_source_loader;

    modulePath = ZrCore_String_Create(state,
                                      "reference.same_identity",
                                      strlen("reference.same_identity"));
    TEST_ASSERT_NOT_NULL(modulePath);
    expectedPathHash = ZrCore_Module_CalculatePathHash(state, modulePath);

    sourceModule = ZrCore_Module_ImportByPath(state, modulePath);
    TEST_ASSERT_NOT_NULL(sourceModule);
    TEST_ASSERT_TRUE(string_equals_cstring(sourceModule->fullPath, "reference.same_identity"));
    TEST_ASSERT_EQUAL_UINT64(expectedPathHash, sourceModule->pathHash);

    markerValue = get_object_field_value(state, &sourceModule->super, "marker");
    TEST_ASSERT_NOT_NULL(markerValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(markerValue->type));
    TEST_ASSERT_EQUAL_INT64(11, markerValue->value.nativeObject.nativeInt64);

    fixture.source = ZR_NULL;
    fixture.bytes = binaryBytes;
    fixture.length = binaryLength;
    fixture.isBinary = ZR_TRUE;

    cachedBinaryModule = ZrCore_Module_ImportByPath(state, modulePath);
    TEST_ASSERT_NOT_NULL(cachedBinaryModule);
    TEST_ASSERT_EQUAL_PTR(sourceModule, cachedBinaryModule);
    TEST_ASSERT_TRUE(string_equals_cstring(cachedBinaryModule->fullPath, "reference.same_identity"));
    TEST_ASSERT_EQUAL_UINT64(expectedPathHash, cachedBinaryModule->pathHash);

    markerValue = get_object_field_value(state, &cachedBinaryModule->super, "marker");
    TEST_ASSERT_NOT_NULL(markerValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(markerValue->type));
    TEST_ASSERT_EQUAL_INT64(11, markerValue->value.nativeObject.nativeInt64);

    state->global->sourceLoader = ZR_NULL;
    g_module_fixture_sources = previousFixtures;
    g_module_fixture_source_count = previousFixtureCount;
    free(binaryBytes);
    free(moduleSource);
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_binary_module_metadata_roundtrip_fixture(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Binary Module Metadata Roundtrip Fixture";
    SZrState *state = ZR_NULL;
    TZrSize moduleSourceSize = 0;
    TZrSize importSourceSize = 0;
    char *moduleSource = ZR_NULL;
    char *importSource = ZR_NULL;
    const TZrChar *intermediatePath = "reference_binary_metadata_roundtrip_fixture.zri";
    const TZrChar *binaryPath = "reference_binary_metadata_roundtrip_fixture.zro";
    SZrString *moduleSourceName = ZR_NULL;
    SZrString *importSourceName = ZR_NULL;
    SZrFunction *moduleFunction = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;
    char *intermediateText = ZR_NULL;
    TZrSize intermediateSize = 0;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    SZrModuleFixtureSource fixture = {0};
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    SZrTypeValue result;
    SZrObject *reflectionObject = ZR_NULL;
    const SZrTypeValue *testsValue = ZR_NULL;
    const SZrTypeValue *compileTimeValue = ZR_NULL;
    const SZrTypeValue *compileTimeVariablesValue = ZR_NULL;
    const SZrTypeValue *compileTimeFunctionsValue = ZR_NULL;
    SZrObject *testsArray = ZR_NULL;
    SZrObject *compileTimeObject = ZR_NULL;
    SZrObject *testInfo = ZR_NULL;
    SZrObject *compileTimeVariable = ZR_NULL;
    SZrObject *compileTimeFunction = ZR_NULL;
    const SZrTypeValue *testNameValue = ZR_NULL;
    const SZrTypeValue *variableNameValue = ZR_NULL;
    const SZrTypeValue *variableTypeValue = ZR_NULL;
    const SZrTypeValue *functionNameValue = ZR_NULL;
    const SZrTypeValue *functionReturnTypeValue = ZR_NULL;
    const SZrTypeValue *functionParametersValue = ZR_NULL;
    SZrObject *functionParameters = ZR_NULL;
    SZrObject *firstParameter = ZR_NULL;
    const SZrTypeValue *parameterNameValue = ZR_NULL;
    const SZrTypeValue *parameterTypeValue = ZR_NULL;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    moduleSource = read_reference_file("core_semantics/modules_imports_artifacts/binary_metadata_roundtrip_module.zr",
                                       &moduleSourceSize);
    importSource = read_reference_file("core_semantics/modules_imports_artifacts/binary_metadata_roundtrip_import.zr",
                                       &importSourceSize);
    TEST_ASSERT_NOT_NULL(moduleSource);
    TEST_ASSERT_NOT_NULL(importSource);

    moduleSourceName = ZrCore_String_Create(state,
                                            "reference.binary_meta.zr",
                                            strlen("reference.binary_meta.zr"));
    TEST_ASSERT_NOT_NULL(moduleSourceName);
    moduleFunction = ZrParser_Source_Compile(state, moduleSource, moduleSourceSize, moduleSourceName);
    TEST_ASSERT_NOT_NULL(moduleFunction);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, moduleFunction, intermediatePath));
    intermediateText = ZrTests_ReadTextFile(intermediatePath, &intermediateSize);
    TEST_ASSERT_NOT_NULL(intermediateText);
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "COMPILE_TIME_VARIABLES (1):"));
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "MAX_SCALE: int"));
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "COMPILE_TIME_FUNCTIONS (1):"));
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "fn buildBias(seed: int): int"));
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "TESTS (1):"));
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "test binary_meta()"));

    binaryBytes = build_module_binary_fixture(state, moduleSource, binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    fixture.path = "reference.binary_meta";
    fixture.source = ZR_NULL;
    fixture.bytes = binaryBytes;
    fixture.length = binaryLength;
    fixture.isBinary = ZR_TRUE;

    g_module_fixture_sources = &fixture;
    g_module_fixture_source_count = 1;
    state->global->sourceLoader = module_fixture_source_loader;

    importSourceName = ZrCore_String_Create(state,
                                            "reference_binary_metadata_roundtrip_import.zr",
                                            strlen("reference_binary_metadata_roundtrip_import.zr"));
    TEST_ASSERT_NOT_NULL(importSourceName);
    entryFunction = ZrParser_Source_Compile(state, importSource, importSourceSize, importSourceName);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

    reflectionObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(reflectionObject);

    testsValue = get_object_field_value(state, reflectionObject, "tests");
    compileTimeValue = get_object_field_value(state, reflectionObject, "compileTime");
    TEST_ASSERT_NOT_NULL(testsValue);
    TEST_ASSERT_NOT_NULL(compileTimeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, testsValue->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, compileTimeValue->type);

    testsArray = ZR_CAST_OBJECT(state, testsValue->value.object);
    compileTimeObject = ZR_CAST_OBJECT(state, compileTimeValue->value.object);
    TEST_ASSERT_NOT_NULL(testsArray);
    TEST_ASSERT_NOT_NULL(compileTimeObject);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(testsArray));

    testInfo = get_array_entry_object(state, testsArray, 0);
    TEST_ASSERT_NOT_NULL(testInfo);
    testNameValue = get_object_field_value(state, testInfo, "name");
    TEST_ASSERT_NOT_NULL(testNameValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, testNameValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, testNameValue->value.object), "binary_meta"));

    compileTimeVariablesValue = get_object_field_value(state, compileTimeObject, "variables");
    compileTimeFunctionsValue = get_object_field_value(state, compileTimeObject, "functions");
    TEST_ASSERT_NOT_NULL(compileTimeVariablesValue);
    TEST_ASSERT_NOT_NULL(compileTimeFunctionsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeVariablesValue->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, compileTimeFunctionsValue->type);
    TEST_ASSERT_EQUAL_UINT32(1,
                             (TZrUInt32)get_array_length(ZR_CAST_OBJECT(state,
                                                                        compileTimeVariablesValue->value.object)));
    TEST_ASSERT_EQUAL_UINT32(1,
                             (TZrUInt32)get_array_length(ZR_CAST_OBJECT(state,
                                                                        compileTimeFunctionsValue->value.object)));

    compileTimeVariable = get_array_entry_object(state,
                                                 ZR_CAST_OBJECT(state, compileTimeVariablesValue->value.object),
                                                 0);
    compileTimeFunction = get_array_entry_object(state,
                                                 ZR_CAST_OBJECT(state, compileTimeFunctionsValue->value.object),
                                                 0);
    TEST_ASSERT_NOT_NULL(compileTimeVariable);
    TEST_ASSERT_NOT_NULL(compileTimeFunction);

    variableNameValue = get_object_field_value(state, compileTimeVariable, "name");
    variableTypeValue = get_object_field_value(state, compileTimeVariable, "typeName");
    TEST_ASSERT_NOT_NULL(variableNameValue);
    TEST_ASSERT_NOT_NULL(variableTypeValue);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, variableNameValue->value.object), "MAX_SCALE"));
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, variableTypeValue->value.object), "int"));

    functionNameValue = get_object_field_value(state, compileTimeFunction, "name");
    functionReturnTypeValue = get_object_field_value(state, compileTimeFunction, "returnTypeName");
    functionParametersValue = get_object_field_value(state, compileTimeFunction, "parameters");
    TEST_ASSERT_NOT_NULL(functionNameValue);
    TEST_ASSERT_NOT_NULL(functionReturnTypeValue);
    TEST_ASSERT_NOT_NULL(functionParametersValue);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, functionNameValue->value.object), "buildBias"));
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, functionReturnTypeValue->value.object), "int"));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, functionParametersValue->type);

    functionParameters = ZR_CAST_OBJECT(state, functionParametersValue->value.object);
    TEST_ASSERT_NOT_NULL(functionParameters);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(functionParameters));
    firstParameter = get_array_entry_object(state, functionParameters, 0);
    TEST_ASSERT_NOT_NULL(firstParameter);
    parameterNameValue = get_object_field_value(state, firstParameter, "name");
    parameterTypeValue = get_object_field_value(state, firstParameter, "typeName");
    TEST_ASSERT_NOT_NULL(parameterNameValue);
    TEST_ASSERT_NOT_NULL(parameterTypeValue);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterNameValue->value.object), "seed"));
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, parameterTypeValue->value.object), "int"));

    ZrCore_Function_Free(state, entryFunction);
    ZrCore_Function_Free(state, moduleFunction);
    state->global->sourceLoader = ZR_NULL;
    g_module_fixture_sources = previousFixtures;
    g_module_fixture_source_count = previousFixtureCount;
    free(intermediateText);
    free(binaryBytes);
    free(importSource);
    free(moduleSource);
    remove(intermediatePath);
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 主函数 ====================

int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Module System Tests\n");
    TEST_MODULE_DIVIDER();

    // 1. 脚本级变量作为 __entry 局部变量
    RUN_TEST(test_script_level_variables_as_entry_locals);

    // 2. pub/pri/pro 可见性修饰符解析
    RUN_TEST(test_visibility_modifiers_parsing);

    // 3. 模块导出收集
    RUN_TEST(test_module_export_collection);

    // 4. 模块缓存机制
    RUN_TEST(test_module_cache_operations);

    // 5. 内部模块导入 helper / zr.import 隐藏
    RUN_TEST(test_module_import_helper_and_hidden_zr_import);

    // 6. 缓存命中/未命中场景
    RUN_TEST(test_module_cache_hit_miss);

    // 7. 模块可见性访问控制
    RUN_TEST(test_module_visibility_access_control);

    // 8. zr 标识符的全局访问和作用域覆盖
    RUN_TEST(test_zr_identifier_global_access_and_scope_override);

    // 9. 路径哈希计算
    RUN_TEST(test_path_hash_calculation);

    // 10. 完整模块加载流程
    RUN_TEST(test_complete_module_loading_flow);

    // 11. prototypeData 中 using 字段元数据恢复
    RUN_TEST(test_module_restores_owned_field_prototype_metadata);

    // 12. prototypeData 中高级 OOP runtime descriptor 元数据恢复
    RUN_TEST(test_module_restores_advanced_oop_runtime_descriptor_metadata);

    // 13. 复杂 source module 导出函数图不应出现 null call target
    RUN_TEST(test_source_module_exports_complex_function_graph_without_null_call_targets);

    // 13.1 preinstalled callable 不应在 native import 期间丢失 imported module captures
    RUN_TEST(test_source_module_preinstalled_callable_preserves_imported_module_captures_after_native_imports);

    // 14. 带参导出函数别名在跨模块调用时保留调用签名
    RUN_TEST(test_imported_function_alias_with_parameters_preserves_call_signature);

    // 14.1 module init summary cache 扩容后当前 source summary 仍可安全 finalize
    RUN_TEST(test_module_init_summary_cache_growth_keeps_current_source_summary_stable);

    // 14. 循环 source module 允许 declaration-ready 顶层函数引用
    RUN_TEST(test_cyclic_source_modules_allow_declaration_ready_function_references);

    // 15. 循环 source module 允许安全 imported call 在 entry 中执行
    RUN_TEST(test_cyclic_source_modules_allow_safe_imported_call_during_entry);

    // 16. 循环 source module 动态 entry 读取命中 runtime cycle-init guard
    RUN_TEST(test_cyclic_source_modules_dynamic_entry_read_raises_cycle_init_error);

    // 17. binary roundtrip 保留模块初始化 callable metadata
    RUN_TEST(test_binary_roundtrip_preserves_module_init_callable_metadata);

    // 18. zr.system.vm.callModuleExport 可执行嵌套 native 导出
    RUN_TEST(test_system_vm_call_module_export_executes_nested_native_export);

    // 19. native Vector3 构造在 runtime 绑定全部数值参数
    RUN_TEST(test_native_vector3_constructor_binds_all_numeric_arguments_at_runtime);

    // 19. zr.system 聚合根导出叶子模块
    RUN_TEST(test_system_root_aggregates_leaf_modules_and_reuses_cached_instances);

    // 20. zr.system 根模块仅导出子模块
    RUN_TEST(test_system_root_exports_only_submodules);

    // 18. 叶子模块导出新的 system API
    RUN_TEST(test_system_leaf_modules_expose_new_api_and_owned_types);

    // 19. system 原生类型字段元信息完整
    RUN_TEST(test_system_native_types_register_complete_struct_fields);

    // 20. system.gc 安全控制面返回运行时统计
    RUN_TEST(test_system_gc_safe_controls_report_runtime_stats);

    // 21. 根模块原生元信息包含 modules 数组
    RUN_TEST(test_system_root_native_module_info_exposes_module_links);

    // 22. native enum/interface descriptor 元信息完整暴露
    RUN_TEST(test_native_module_info_exposes_enum_and_interface_descriptors);

    // 23. native callable / generic descriptor 元信息完整暴露
    RUN_TEST(test_native_module_info_exposes_callable_parameters_and_generic_constraints);

    // 24. zr.ffi wrapper lowering 元信息同时暴露到 module info 和 runtime prototype
    RUN_TEST(test_native_module_info_exposes_ffi_wrapper_lowering_metadata);

    // 25. source wrapper metadata 同时暴露到 %type metadata 和 runtime prototype hidden fields
    RUN_TEST(test_source_module_wrapper_metadata_exposes_ffi_wrapper_fields_and_runtime_hidden_metadata);

    // 26. native runtime 注册 enum 静态成员和 interface 继承链
    RUN_TEST(test_native_module_runtime_registers_enum_members_and_interface_inheritance);

    // 27. native binding helper 在 GC retry 中仍保活 fresh object/value
    RUN_TEST(test_native_binding_helpers_root_fresh_values_across_gc_retry);

    // 28. native enum 构造在 runtime 返回正确实例
    RUN_TEST(test_native_enum_construction_returns_runtime_enum_instance);

    // 29. zr.container 导出泛型接口/类型以及约束元数据
    RUN_TEST(test_container_module_exports_generic_interfaces_and_constraints);

    // 26.1 zr.builtin 导出协议、根类型与 primitive wrappers
    RUN_TEST(test_builtin_module_exports_protocols_roots_and_wrappers);

    // 26.2 zr.builtin typeHints 覆盖全部正式 builtin 类型
    RUN_TEST(test_builtin_module_exports_complete_type_hints);

    // 26.3 builtin Object.box / TypeInfo.box 生成 wrapper 并保持值语义
    RUN_TEST(test_builtin_wrapper_box_helpers_preserve_runtime_semantics);

    // 27. zr.container Array runtime 支持 add 和 [] 访问
    RUN_TEST(test_container_array_runtime_supports_add_and_computed_index_access);

    // 28. 原生 fixed array 通过 foreach 迭代
    RUN_TEST(test_container_fixed_array_runtime_supports_foreach_iteration);

    // 29. zr.container Array 通过 foreach 迭代
    RUN_TEST(test_container_array_runtime_supports_foreach_iteration);

    // 30. zr.container Map runtime 支持键访问
    RUN_TEST(test_container_map_runtime_supports_computed_key_access);

    // 31. zr.container Set runtime 保持唯一性
    RUN_TEST(test_container_set_runtime_enforces_uniqueness);

    // 31. zr.container LinkedList runtime 维护首尾节点
    RUN_TEST(test_container_linked_list_runtime_updates_head_and_tail);

    // 30. %type 模块反射暴露预期字段
    RUN_TEST(test_percent_type_module_reflection_exposes_expected_fields);

    // 31. %type 实例反射使用运行时原型
    RUN_TEST(test_percent_type_instance_reflection_uses_runtime_prototype);

    // 32. %type 源模块反射按脚本顺序暴露声明和默认输出
    RUN_TEST(test_percent_type_source_module_reflection_uses_ordered_script_metadata);

    // 33. %type 源类型反射暴露参数、布局和代码块摘要
    RUN_TEST(test_percent_type_source_type_reflection_exposes_parameters_layout_and_codeblocks);

    // 34. %type 源函数反射暴露参数元数据和 IR 摘要
    RUN_TEST(test_percent_type_source_function_reflection_exposes_parameter_metadata_and_ir);

    // 35. 函数字面值类型在运行时物化为 callable reflection，并且 %type 保持同一形状
    RUN_TEST(test_function_type_literal_runtime_materializes_callable_reflection);

    // 36. %type(local function/lambda) 在 source/binary 两条路径都保留 callable shape
    RUN_TEST(test_percent_type_local_callable_reflection_preserves_callable_shape_in_source_and_binary);

    // 36. %type(%func(...)) 暴露 callable reflection shape
    RUN_TEST(test_percent_type_function_type_literal_reflection_exposes_callable_shape);

    // 35. %type 源模块反射暴露 compileTime / %test 元数据
    RUN_TEST(test_percent_type_source_module_reflection_exposes_compile_time_and_test_metadata);

    // 36. %type 源类型反射暴露 decorator metadata
    RUN_TEST(test_percent_type_source_type_reflection_exposes_decorator_metadata);

    // 37. %type 源类型反射暴露 runtime class decorator metadata
    RUN_TEST(test_percent_type_source_type_reflection_exposes_runtime_class_decorator_metadata);

    // 38. %type 源函数反射暴露 runtime function decorator metadata
    RUN_TEST(test_percent_type_source_function_reflection_exposes_runtime_function_decorator_metadata);

    // 39. source runtime decorated pub function 可直接从 imported module 调用
    RUN_TEST(test_source_runtime_decorated_pub_function_is_directly_callable_from_imported_module);

    // 40. source module runtime 注册 enum 静态成员并允许 imported access 正常执行
    RUN_TEST(test_source_module_runtime_registers_enum_members_and_imported_access);

    // 41. source module struct constructor 手工 VM 调用保持字段写入
    RUN_TEST(test_source_module_struct_constructor_manual_vm_call_preserves_fields);

    // 42. source module struct constructor runtime 调用保持字段写入
    RUN_TEST(test_source_module_struct_constructor_runtime_invocation_preserves_fields);

    // 43. source module struct method 调用保持 receiver 写回
    RUN_TEST(test_source_module_struct_method_call_writebacks_receiver_state);

    // 44. source module struct value construction 保持字段访问可用
    RUN_TEST(test_source_module_struct_value_construction_preserves_field_access);

    // 45. source module class boxed construction 保持字段访问可用
    RUN_TEST(test_source_module_class_boxed_construction_preserves_field_access);

    // 46. %type 源字段反射暴露 runtime field decorator metadata
    RUN_TEST(test_percent_type_source_field_reflection_exposes_runtime_field_decorator_metadata);

    // 47. %type 源方法反射暴露 runtime method decorator metadata
    RUN_TEST(test_percent_type_source_method_reflection_exposes_runtime_method_decorator_metadata);

    // 48. %type 源属性反射暴露 runtime property decorator metadata
    RUN_TEST(test_percent_type_source_property_reflection_exposes_runtime_property_decorator_metadata);

    // 49. %type 源反射暴露 advanced OOP modifier / override / slot 元数据
    RUN_TEST(test_percent_type_source_reflection_exposes_advanced_oop_metadata);

    // 50. %type 源模块在 full GC 后仍保留 compileTime 参数元数据
    RUN_TEST(test_percent_type_source_module_reflection_preserves_compile_time_metadata_across_full_gc);

    // 46. %type binary module 反射恢复 compileTime / %test 元数据
    RUN_TEST(test_percent_type_binary_module_reflection_restores_compile_time_and_test_metadata);

    // 47. %type binary type 反射暴露 runtime class decorator metadata
    RUN_TEST(test_percent_type_binary_type_reflection_exposes_runtime_class_decorator_metadata);

    // 48. %type binary function 反射暴露 runtime function decorator metadata
    RUN_TEST(test_percent_type_binary_function_reflection_exposes_runtime_function_decorator_metadata);

    // 49. binary runtime decorated pub function 可直接从 imported module 调用
    RUN_TEST(test_binary_runtime_decorated_pub_function_is_directly_callable_from_imported_module);

    // 50. %type binary 字段/方法/属性反射暴露 runtime member/property decorator metadata
    RUN_TEST(test_percent_type_binary_field_reflection_exposes_runtime_field_decorator_metadata);
    RUN_TEST(test_percent_type_binary_method_reflection_exposes_runtime_method_decorator_metadata);
    RUN_TEST(test_percent_type_binary_property_reflection_exposes_runtime_property_decorator_metadata);

    // 51. %type binary 反射恢复 advanced OOP modifier / override / slot 元数据
    RUN_TEST(test_percent_type_binary_reflection_restores_advanced_oop_metadata);

    // 35. native registry 拒绝未来 ABI 版本
    RUN_TEST(test_native_registry_rejects_future_runtime_abi);

    // 36. native registry 拒绝未支持 capability
    RUN_TEST(test_native_registry_rejects_unsupported_capabilities);

    // 37. reference protocols fixture uses native iterable contracts
    RUN_TEST(test_reference_protocols_native_iterable_fixture_uses_registered_contracts);

    // 38. reference import matrix 与 native member-chain fixture
    RUN_TEST(test_reference_import_manifest_and_native_member_chain_fixture);
    RUN_TEST(test_reference_modules_manifest_duplicate_import_and_hidden_api_cases);
    RUN_TEST(test_reference_hidden_internal_import_api_fixture_is_rejected);
    RUN_TEST(test_reference_source_and_binary_import_forms_share_logical_identity);
    RUN_TEST(test_reference_binary_module_metadata_roundtrip_fixture);

    return UNITY_END();
}
