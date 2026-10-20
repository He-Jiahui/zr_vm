#include "zr_vm_library/native_binding.h"
#include <string.h>

static TZrBool swarm_plugin_value(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetInt(context->state, result, 42);
    return ZR_TRUE;
}

static TZrBool sample_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    double x, y;
    TZrInt64 tag;
    ZrLibTempValueRoot root;
    SZrTypeValue value;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) ||
        !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadInt(context, 2, &tag) ||
        !ZrLib_CallContext_BeginTempValueRoot(context, &root)) return ZR_FALSE;
    SZrObject *object = ZrLib_Type_NewInstanceWithPrototype(
            context->state, ZrLib_CallContext_OwnerPrototype(context));
    if (object == ZR_NULL || !ZrLib_TempValueRoot_SetObject(&root, object, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }
    ZrLib_Value_SetFloat(context->state, &value, x);
    ZrLib_Object_TrySetFieldCString(context->state,
            (SZrObject *)ZrLib_TempValueRoot_Value(&root)->value.object, "x", &value);
    ZrLib_Value_SetFloat(context->state, &value, y);
    ZrLib_Object_TrySetFieldCString(context->state,
            (SZrObject *)ZrLib_TempValueRoot_Value(&root)->value.object, "y", &value);
    ZrLib_Value_SetInt(context->state, &value, tag);
    ZrLib_Object_TrySetFieldCString(context->state,
            (SZrObject *)ZrLib_TempValueRoot_Value(&root)->value.object, "tag", &value);
    ZrCore_Value_Copy(context->state, result, ZrLib_TempValueRoot_Value(&root));
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}

static TZrBool sample_sum(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLibInlineArgumentView view;
    double values[2];
    TZrInt64 tag;
    if (!ZrLib_CallContext_InlineArgumentView(context, 0, &view) ||
        !view.span.available || view.span.byteSize != sizeof(values) + sizeof(tag)) return ZR_FALSE;
    memcpy(values, view.span.address, sizeof(values));
    memcpy(&tag, (const char *)view.span.address + sizeof(values), sizeof(tag));
    ZrLib_Value_SetFloat(context->state, result, values[0] + values[1] + tag);
    return ZR_TRUE;
}

static const ZrLibFieldDescriptor fields[] = {
    ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("tag", "int", ZR_NULL),
};
static const ZrLibParameterDescriptor constructorParameters[] = {
    {.name = "x", .typeName = "float"}, {.name = "y", .typeName = "float"},
    {.name = "tag", .typeName = "int"},
};
static const ZrLibMetaMethodDescriptor metaMethods[] = {{
    .metaType = ZR_META_CONSTRUCTOR, .minArgumentCount = 3, .maxArgumentCount = 3,
    .callback = sample_construct, .returnTypeName = "Sample",
    .parameters = constructorParameters, .parameterCount = 3,
}};
static const ZrLibTypeDescriptor types[] = {{
    .name = "Sample", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
    .fields = fields, .fieldCount = 3, .allowValueConstruction = ZR_TRUE,
    .metaMethods = metaMethods, .metaMethodCount = 1,
    .constructorSignature = "Sample(x: float, y: float, tag: int)",
}};
static const ZrLibParameterDescriptor sumParameters[] = {{.name = "sample", .typeName = "Sample"}};

static const ZrLibFunctionDescriptor functions[] = {{
    .name = "value",
    .callback = swarm_plugin_value,
    .returnTypeName = "int",
    .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN,
}, {
    .name = "sum", .callback = sample_sum, .returnTypeName = "float",
    .minArgumentCount = 1, .maxArgumentCount = 1,
    .parameters = sumParameters, .parameterCount = 1,
    .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN,
}};

static const ZrLibModuleDescriptor module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "zr.swarmprobe",
    .functions = functions,
    .functionCount = 2,
    .types = types, .typeCount = 1,
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
    .publicContractHash = "zr.swarmprobe:v1",
};

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return &module; }
