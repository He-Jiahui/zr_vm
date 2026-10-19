#include "zr_vm_library/native_binding.h"
#include <string.h>

#if !defined(SWARM_INLINE_PROBE)
static TZrBool value(ZrLibCallContext *ctx, SZrTypeValue *result) {
    ZrLib_Value_SetInt(ctx->state, result, 37);
    return ZR_TRUE;
}
static const ZrLibFunctionDescriptor functions[] = {
    {.name = "value", .callback = value, .returnTypeName = "int",
     .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN},
};
#else
static TZrBool construct_sprite(ZrLibCallContext *ctx, SZrTypeValue *result) {
    static const char *names[] = {"x", "y", "size", "rot", "rgba"};
    double values[4];
    TZrInt64 rgba;
    ZrLibTempValueRoot root;
    SZrObject *object;
    SZrTypeValue field;
    for (unsigned i = 0; i < 4; ++i) {
        if (!ZrLib_CallContext_ReadFloat(ctx, i, &values[i])) { return ZR_FALSE; }
    }
    if (!ZrLib_CallContext_ReadInt(ctx, 4, &rgba) || !ZrLib_CallContext_BeginTempValueRoot(ctx, &root)) { return ZR_FALSE; }
    object = ZrLib_Type_NewInstanceWithPrototype(ctx->state, ZrLib_CallContext_OwnerPrototype(ctx));
    if (!object || !ZrLib_TempValueRoot_SetObject(&root, object, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&root); return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(&field);
    for (unsigned i = 0; i < 5; ++i) {
        if (i < 4) { ZrLib_Value_SetFloat(ctx->state, &field, values[i]); }
        else { ZrLib_Value_SetInt(ctx->state, &field, rgba); }
        object = (SZrObject *)ZrLib_TempValueRoot_Value(&root)->value.object;
        if (!ZrLib_Object_TrySetFieldCString(ctx->state, object, names[i], &field)) {
            ZrLib_TempValueRoot_End(&root); return ZR_FALSE;
        }
    }
    ZrCore_Value_Copy(ctx->state, result, ZrLib_TempValueRoot_Value(&root));
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}
static TZrBool sprite_sum(ZrLibCallContext *ctx, SZrTypeValue *result) {
    ZrLibInlineArgumentView view;
    double values[4];
    TZrInt64 color;
    if (!ZrLib_CallContext_InlineArgumentView(ctx, 0, &view) ||
        !view.span.available || view.span.byteSize != sizeof(values) + sizeof(color)) {
        return ZR_FALSE;
    }
    memcpy(values, view.span.address, sizeof(values));
    memcpy(&color, (const char *)view.span.address + sizeof(values), sizeof(color));
    ZrLib_Value_SetFloat(ctx->state, result, values[0] + values[1] + values[2] + values[3] + color);
    return ZR_TRUE;
}

static const ZrLibFieldDescriptor fields[] = {
    ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("size", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("rot", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("rgba", "int", ZR_NULL),
};
static const ZrLibParameterDescriptor constructor_args[] = {
    {.name = "x", .typeName = "float"}, {.name = "y", .typeName = "float"},
    {.name = "size", .typeName = "float"}, {.name = "rot", .typeName = "float"},
    {.name = "rgba", .typeName = "int"},
};
static const ZrLibMetaMethodDescriptor meta[] = {{
    .metaType = ZR_META_CONSTRUCTOR, .minArgumentCount = 5, .maxArgumentCount = 5,
    .callback = construct_sprite, .returnTypeName = "SpriteInstance",
    .parameters = constructor_args, .parameterCount = 5,
}};
static const ZrLibTypeDescriptor types[] = {{
    .name = "SpriteInstance", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
    .fields = fields, .fieldCount = 5, .allowValueConstruction = ZR_TRUE,
    .metaMethods = meta, .metaMethodCount = 1,
    .constructorSignature = "SpriteInstance(x: float, y: float, size: float, rot: float, rgba: int)",
}};
static const ZrLibParameterDescriptor sprite_args[] = {{.name = "sprite", .typeName = "SpriteInstance"}};
static const ZrLibFunctionDescriptor functions[] = {
    {.name = "spriteSum", .callback = sprite_sum, .returnTypeName = "float",
     .minArgumentCount = 1, .maxArgumentCount = 1, .parameters = sprite_args, .parameterCount = 1,
     .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN},
};
#endif
static const ZrLibModuleDescriptor module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
#if defined(SWARM_INLINE_PROBE)
    .moduleName = "zrgame.inlineprobe", .types = types, .typeCount = 1,
#else
    .moduleName = "zrgame.probe",
#endif
    .functions = functions, .functionCount = 1,
    .moduleVersion = "1.0.0", .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, .publicContractHash = "zrgame.probe:v1",
};
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return &module; }
