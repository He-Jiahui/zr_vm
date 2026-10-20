#include "modules.h"
#include "runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"

#include <float.h>
#include <math.h>
#include <string.h>

static bool valid_float(double value) { return isfinite(value) && value >= -FLT_MAX && value <= FLT_MAX; }

static TZrBool make_sprite(ZrLibCallContext *ctx, const double values[4], TZrInt64 color, ZrgSprite *out) {
    for (unsigned i = 0; i < 4; ++i) {
        if (!valid_float(values[i])) return zrg_module_error(ctx, "sprite requires finite float32 values");
    }
    if (values[2] < 0 || color < 0 || (uint64_t)color > UINT32_MAX)
        return zrg_module_error(ctx, "sprite size or rgba out of range");
    *out = (ZrgSprite){(float)values[0], (float)values[1], (float)values[2], (float)values[3], (uint32_t)color};
    return ZR_TRUE;
}

static TZrBool append_sprite(ZrLibCallContext *ctx, ZrgSprite value) {
    if (!zrg_sprite(value)) return zrg_module_error(ctx, "sprite batch overflow or no active frame");
    return ZR_TRUE;
}

static TZrBool begin_frame(ZrLibCallContext *ctx, SZrTypeValue *result) {
    TZrInt64 frame;
    ZrLib_CallContext_ReadInt(ctx, 0, &frame);
    zrg_begin_frame(frame);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool camera(ZrLibCallContext *ctx, SZrTypeValue *result) {
    double x, y;
    ZrLib_CallContext_ReadFloat(ctx, 0, &x);
    ZrLib_CallContext_ReadFloat(ctx, 1, &y);
    if (!valid_float(x) || !valid_float(y) || !zrg_camera((float)x, (float)y))
        return zrg_module_error(ctx, "camera requires an active frame and finite float32 values");
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool sprite(ZrLibCallContext *ctx, SZrTypeValue *result) {
    double values[4];
    TZrInt64 color;
    for (unsigned i = 0; i < 4; ++i) ZrLib_CallContext_ReadFloat(ctx, i, &values[i]);
    ZrLib_CallContext_ReadInt(ctx, 4, &color);
    ZrgSprite value;
    if (!make_sprite(ctx, values, color, &value) || !append_sprite(ctx, value)) return ZR_FALSE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool construct_sprite(ZrLibCallContext *ctx, SZrTypeValue *result) {
    static const char *names[] = {"x", "y", "size", "rot", "rgba"};
    double values[4];
    TZrInt64 color;
    ZrLibTempValueRoot root;
    for (unsigned i = 0; i < 4; ++i) ZrLib_CallContext_ReadFloat(ctx, i, &values[i]);
    ZrLib_CallContext_ReadInt(ctx, 4, &color);
    ZrgSprite checked;
    if (!make_sprite(ctx, values, color, &checked)) return ZR_FALSE;
    if (!ZrLib_CallContext_BeginTempValueRoot(ctx, &root)) return ZR_FALSE;
    SZrObject *object = ZrLib_Type_NewInstanceWithPrototype(ctx->state, ZrLib_CallContext_GetConstructTargetPrototype(ctx));
    if (!object || !ZrLib_TempValueRoot_SetObject(&root, object, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }
    SZrTypeValue field;
    ZrCore_Value_ResetAsNull(&field);
    for (unsigned i = 0; i < 5; ++i) {
        if (i < 4) ZrLib_Value_SetFloat(ctx->state, &field, values[i]);
        else ZrLib_Value_SetInt(ctx->state, &field, color);
        object = (SZrObject *)ZrLib_TempValueRoot_Value(&root)->value.object;
        if (!ZrLib_Object_TrySetFieldCString(ctx->state, object, names[i], &field)) {
            ZrLib_TempValueRoot_End(&root);
            return ZR_FALSE;
        }
    }
    ZrCore_Value_Copy(ctx->state, result, ZrLib_TempValueRoot_Value(&root));
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}

static TZrBool sprite_inst(ZrLibCallContext *ctx, SZrTypeValue *result) {
    ZrLibInlineArgumentView view;
    double values[4];
    TZrInt64 color;
    if (!ZrLib_CallContext_InlineArgumentView(ctx, 0, &view) || !view.span.available ||
        !view.typeLayout || view.typeLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        view.span.byteSize != sizeof(values) + sizeof(color) || view.typeLayout->fieldCount != 5)
        return zrg_module_error(ctx, "SpriteInstance inline layout mismatch");
    for (unsigned i = 0; i < 5; ++i) {
        if (view.typeLayout->fields[i].byteOffset != i * 8u || view.typeLayout->fields[i].byteSize != 8u)
            return zrg_module_error(ctx, "SpriteInstance field layout mismatch");
    }
    /* Borrowed inline bytes are consumed synchronously, with no allocation or safepoint. */
    memcpy(values, view.span.address, sizeof(values));
    memcpy(&color, (const char *)view.span.address + sizeof(values), sizeof(color));
    ZrgSprite value;
    if (!make_sprite(ctx, values, color, &value) || !append_sprite(ctx, value)) return ZR_FALSE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static bool array_float(const SZrTypeValue *value, double *out) {
    if (!value || (value->type != ZR_VALUE_TYPE_FLOAT && value->type != ZR_VALUE_TYPE_DOUBLE)) return false;
    *out = value->value.nativeObject.nativeDouble;
    return valid_float(*out);
}

static bool array_color(const SZrTypeValue *value, uint32_t *out) {
    if (!value) return false;
    switch (value->type) {
        case ZR_VALUE_TYPE_INT8: case ZR_VALUE_TYPE_INT16: case ZR_VALUE_TYPE_INT32: case ZR_VALUE_TYPE_INT64:
            if (value->value.nativeObject.nativeInt64 < 0 || (uint64_t)value->value.nativeObject.nativeInt64 > UINT32_MAX) return false;
            *out = (uint32_t)value->value.nativeObject.nativeInt64;
            return true;
        default: return false;
    }
}

static bool soa_sprite(ZrLibCallContext *ctx, ZrLibTempValueRoot roots[4], size_t index, ZrgSprite *out) {
    double values[3];
    for (unsigned i = 0; i < 3; ++i) {
        SZrObject *array = (SZrObject *)ZrLib_TempValueRoot_Value(&roots[i])->value.object;
        if (!array_float(ZrLib_Array_Get(ctx->state, array, index), &values[i])) return false;
    }
    SZrObject *colors = (SZrObject *)ZrLib_TempValueRoot_Value(&roots[3])->value.object;
    uint32_t color;
    if (values[2] < 0 || !array_color(ZrLib_Array_Get(ctx->state, colors, index), &color)) return false;
    *out = (ZrgSprite){(float)values[0], (float)values[1], (float)values[2], 0.0f, color};
    return true;
}

static TZrBool submit_soa(ZrLibCallContext *ctx, SZrTypeValue *result) {
    TZrInt64 first, count;
    ZrLib_CallContext_ReadInt(ctx, 4, &first);
    ZrLib_CallContext_ReadInt(ctx, 5, &count);
    if (first < 0 || count < 0 || count > ZRG_SPRITE_CAPACITY)
        return zrg_module_error(ctx, "submitSoA range out of bounds");
    if (!zrg_batch_can_append((uint32_t)count)) return zrg_module_error(ctx, "sprite batch overflow or no active frame");
    /* Root each backing array before resolving another wrapper's field (which may allocate). */
    ZrLibTempValueRoot roots[4];
    unsigned rooted = 0;
    const char *error = NULL;
    for (unsigned i = 0; i < 4; ++i) {
        if (!ZrLib_CallContext_BeginTempValueRoot(ctx, &roots[i])) break;
        ++rooted;
        SZrObject *wrapper;
        ZrLib_CallContext_ReadObject(ctx, i, &wrapper);
        const SZrTypeValue *items = ZrLib_Object_GetFieldCString(ctx->state, wrapper, "__zr_items");
        if (!items || items->type != ZR_VALUE_TYPE_ARRAY) { error = "submitSoA requires zr.container.Array columns"; break; }
        if (!ZrLib_TempValueRoot_SetValue(&roots[i], items)) { error = "submitSoA array root failed"; break; }
        /* Array_Get can materialize the engine's compact integer backing. Do that
         * while rooted here, so the scalar validation/submission loops cannot allocate. */
        if (!ZrCore_Object_SuperArrayMaterializeGeneric(ctx->state,
                (SZrObject *)ZrLib_TempValueRoot_Value(&roots[i])->value.object)) {
            error = "submitSoA array preparation failed";
            break;
        }
        size_t length = ZrLib_Array_Length((SZrObject *)ZrLib_TempValueRoot_Value(&roots[i])->value.object);
        if ((uint64_t)first > length || (uint64_t)count > length - (size_t)first) {
            error = "submitSoA range out of bounds";
            break;
        }
    }
    bool success = rooted == 4 && !error;
    /* Validate the whole slice before appending: invalid input leaves the batch intact.
     * Scalar Array_Get reads in these loops allocate no VM objects and have no safepoint. */
    for (size_t i = 0; success && i < (size_t)count; ++i) {
        ZrgSprite value;
        if (!soa_sprite(ctx, roots, (size_t)first + i, &value)) {
            error = "submitSoA requires finite float columns, nonnegative sizes and u32 rgba";
            success = false;
        }
    }
    for (size_t i = 0; success && i < (size_t)count; ++i) {
        ZrgSprite value;
        success = soa_sprite(ctx, roots, (size_t)first + i, &value) && zrg_sprite(value);
    }
    while (rooted) ZrLib_TempValueRoot_End(&roots[--rooted]);
    if (error) return zrg_module_error(ctx, error);
    if (!success) return ZR_FALSE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool text(ZrLibCallContext *ctx, SZrTypeValue *result) {
    double x, y;
    SZrString *message;
    ZrLib_CallContext_ReadFloat(ctx, 0, &x);
    ZrLib_CallContext_ReadFloat(ctx, 1, &y);
    ZrLib_CallContext_ReadString(ctx, 2, &message);
    if (!valid_float(x) || !valid_float(y) || !zrg_text((float)x, (float)y,
            ZrCore_String_GetNativeString(message), ZrCore_String_GetByteLength(message)))
        return zrg_module_error(ctx, "text batch overflow, invalid coordinates or no active frame");
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool end_frame(ZrLibCallContext *ctx, SZrTypeValue *result) {
    int count = zrg_end_frame();
    if (count < 0) return zrg_module_error(ctx, "endFrame requires an active frame");
    ZrLib_Value_SetInt(ctx->state, result, count);
    return ZR_TRUE;
}

static TZrBool snapshot(ZrLibCallContext *ctx, SZrTypeValue *result) {
    SZrString *path;
    ZrLib_CallContext_ReadString(ctx, 0, &path);
    ZrLib_Value_SetInt(ctx->state, result, zrg_snapshot(ZrCore_String_GetNativeString(path)));
    return ZR_TRUE;
}

static const ZrLibParameterDescriptor frame_args[] = {{.name = "frame", .typeName = "int"}};
static const ZrLibParameterDescriptor camera_args[] = {{.name = "x", .typeName = "float"}, {.name = "y", .typeName = "float"}};
static const ZrLibParameterDescriptor sprite_args[] = {
    {.name = "x", .typeName = "float"}, {.name = "y", .typeName = "float"},
    {.name = "size", .typeName = "float"}, {.name = "rot", .typeName = "float"}, {.name = "rgba", .typeName = "int"},
};
static const ZrLibParameterDescriptor inst_args[] = {{.name = "inst", .typeName = "SpriteInstance"}};
static const ZrLibParameterDescriptor soa_args[] = {
    {.name = "xs", .typeName = "Array<float>"}, {.name = "ys", .typeName = "Array<float>"},
    {.name = "sizes", .typeName = "Array<float>"}, {.name = "colors", .typeName = "Array<int>"},
    {.name = "first", .typeName = "int"}, {.name = "count", .typeName = "int"},
};
static const ZrLibParameterDescriptor text_args[] = {
    {.name = "x", .typeName = "float"}, {.name = "y", .typeName = "float"}, {.name = "msg", .typeName = "string"},
};
static const ZrLibParameterDescriptor path_args[] = {{.name = "path", .typeName = "string"}};
static const ZrLibFieldDescriptor fields[] = {
    ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL), ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("size", "float", ZR_NULL), ZR_LIB_FIELD_DESCRIPTOR_INIT("rot", "float", ZR_NULL),
    ZR_LIB_FIELD_DESCRIPTOR_INIT("rgba", "int", ZR_NULL),
};
static const ZrLibMetaMethodDescriptor meta[] = {{
    .metaType = ZR_META_CONSTRUCTOR, .minArgumentCount = 5, .maxArgumentCount = 5,
    .callback = construct_sprite, .returnTypeName = "SpriteInstance",
    .parameters = sprite_args, .parameterCount = 5,
    .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN,
}};
static const ZrLibTypeDescriptor types[] = {{
    .name = "SpriteInstance", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
    .fields = fields, .fieldCount = ZR_ARRAY_COUNT(fields), .allowValueConstruction = ZR_TRUE,
    .metaMethods = meta, .metaMethodCount = ZR_ARRAY_COUNT(meta),
    .constructorSignature = "SpriteInstance(x: float, y: float, size: float, rot: float, rgba: int)",
}};
static const ZrLibFunctionDescriptor functions[] = {
    ZRG_FUNCTION("beginFrame", begin_frame, "void", frame_args, 1),
    ZRG_FUNCTION("camera", camera, "void", camera_args, 2),
    ZRG_FUNCTION("sprite", sprite, "void", sprite_args, 5),
    ZRG_FUNCTION("spriteInst", sprite_inst, "void", inst_args, 1),
    ZRG_FUNCTION("submitSoA", submit_soa, "void", soa_args, 6),
    ZRG_FUNCTION("text", text, "void", text_args, 3),
    ZRG_FUNCTION("endFrame", end_frame, "int", NULL, 0),
    ZRG_FUNCTION("snapshot", snapshot, "int", path_args, 1),
};
static const ZrLibModuleLinkDescriptor links[] = {{.name = "container", .moduleName = "zr.container"}};

const ZrLibModuleDescriptor *ZrGame_GfxModule(void) {
    static const ZrLibModuleDescriptor module = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION, .moduleName = "zrgame.gfx",
        .functions = functions, .functionCount = ZR_ARRAY_COUNT(functions),
        .types = types, .typeCount = ZR_ARRAY_COUNT(types),
        .moduleLinks = links, .moduleLinkCount = ZR_ARRAY_COUNT(links),
        .documentation = "Fixed-capacity sprite/text submission, modes A/B/C, null backend.",
        .moduleVersion = "1.0.0", .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS | ZR_LIB_MODULE_CAPABILITY_TYPE_METADATA,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, .publicContractHash = "zrgame.gfx:v1",
    };
    return &module;
}
