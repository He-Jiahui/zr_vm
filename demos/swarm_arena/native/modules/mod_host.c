#include "modules.h"
#include "runtime.h"
#include "zr_vm_core/string.h"

static TZrBool now_micros(ZrLibCallContext *ctx, SZrTypeValue *result) {
    ZrLib_Value_SetInt(ctx->state, result, (TZrInt64)zrg_now_us());
    return ZR_TRUE;
}

static TZrBool read_zone_id(ZrLibCallContext *ctx, uint32_t *out) {
    TZrInt64 id;
    ZrLib_CallContext_ReadInt(ctx, 0, &id);
    if (id < 0 || id >= ZRG_ZONE_CAPACITY) return zrg_module_error(ctx, "zone id out of range");
    *out = (uint32_t)id;
    return ZR_TRUE;
}

static TZrBool zone_begin(ZrLibCallContext *ctx, SZrTypeValue *result) {
    uint32_t id;
    if (!read_zone_id(ctx, &id)) return ZR_FALSE;
    if (!zrg_zone_begin(id)) return zrg_module_error(ctx, "zone stack overflow");
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zone_end(ZrLibCallContext *ctx, SZrTypeValue *result) {
    uint32_t id;
    if (!read_zone_id(ctx, &id)) return ZR_FALSE;
    if (!zrg_zone_end(id)) return zrg_module_error(ctx, "zone nesting mismatch");
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zone_report(ZrLibCallContext *ctx, SZrTypeValue *result) {
    char report[ZRG_ZONE_REPORT_BYTES];
    ZrLibTempValueRoot root;
    if (!zrg_zone_report(report, sizeof(report))) return zrg_module_error(ctx, "zone report overflow");
    if (!ZrLib_CallContext_BeginTempValueRoot(ctx, &root)) return ZR_FALSE;
    ZrLib_Value_SetString(ctx->state, ZrLib_TempValueRoot_Value(&root), report);
    ZrCore_Value_Copy(ctx->state, result, ZrLib_TempValueRoot_Value(&root));
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}

static TZrBool open_sink(ZrLibCallContext *ctx, SZrTypeValue *result) {
    SZrString *path;
    ZrLib_CallContext_ReadString(ctx, 0, &path);
    const char *bytes = ZrCore_String_GetNativeString(path);
    size_t length = ZrCore_String_GetByteLength(path);
    /* Paths must not silently truncate at an embedded NUL. No VM allocations here. */
    for (size_t i = 0; i < length; ++i) {
        if (!bytes[i]) return zrg_module_error(ctx, "sink path contains NUL");
    }
    ZrLib_Value_SetInt(ctx->state, result, zrg_sink_open(bytes));
    return ZR_TRUE;
}

static TZrBool write_sink(ZrLibCallContext *ctx, SZrTypeValue *result) {
    TZrInt64 handle;
    SZrString *line;
    ZrLib_CallContext_ReadInt(ctx, 0, &handle);
    ZrLib_CallContext_ReadString(ctx, 1, &line);
    if (!zrg_sink_write(handle, ZrCore_String_GetNativeString(line), ZrCore_String_GetByteLength(line)))
        return zrg_module_error(ctx, "sink write failed");
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool close_sink(ZrLibCallContext *ctx, SZrTypeValue *result) {
    TZrInt64 handle;
    ZrLib_CallContext_ReadInt(ctx, 0, &handle);
    if (!zrg_sink_close(handle)) return zrg_module_error(ctx, "sink close failed");
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool rss_kb(ZrLibCallContext *ctx, SZrTypeValue *result) {
    ZrLib_Value_SetInt(ctx->state, result, zrg_rss_kb());
    return ZR_TRUE;
}

static TZrBool is_host_driven(ZrLibCallContext *ctx, SZrTypeValue *result) {
    ZrLib_Value_SetBool(ctx->state, result, zrg_is_host_driven());
    return ZR_TRUE;
}

/* W10 uses the same arithmetic as native/ffi/zrgame_ffi.c. */
static TZrBool add_i64(ZrLibCallContext *ctx, SZrTypeValue *result) {
    TZrInt64 a, b;
    ZrLib_CallContext_ReadInt(ctx, 0, &a);
    ZrLib_CallContext_ReadInt(ctx, 1, &b);
    /* Keep the callback defined at i64 bounds; W10 uses bounded operands. */
    ZrLib_Value_SetInt(ctx->state, result, (TZrInt64)((TZrUInt64)a + (TZrUInt64)b));
    return ZR_TRUE;
}

static TZrBool noise2(ZrLibCallContext *ctx, SZrTypeValue *result) {
    double x, y;
    ZrLib_CallContext_ReadFloat(ctx, 0, &x);
    ZrLib_CallContext_ReadFloat(ctx, 1, &y);
    ZrLib_Value_SetFloat(ctx->state, result, x * 0.25 + y * 0.75);
    return ZR_TRUE;
}

static TZrBool nop(ZrLibCallContext *ctx, SZrTypeValue *result) {
    (void)ctx;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static const ZrLibParameterDescriptor zone_args[] = {{.name = "id", .typeName = "int"}};
static const ZrLibParameterDescriptor path_args[] = {{.name = "path", .typeName = "string"}};
static const ZrLibParameterDescriptor handle_args[] = {{.name = "h", .typeName = "int"}};
static const ZrLibParameterDescriptor write_args[] = {
    {.name = "h", .typeName = "int"}, {.name = "line", .typeName = "string"},
};
static const ZrLibParameterDescriptor add_args[] = {
    {.name = "a", .typeName = "i64"}, {.name = "b", .typeName = "i64"},
};
static const ZrLibParameterDescriptor noise_args[] = {
    {.name = "x", .typeName = "f64"}, {.name = "y", .typeName = "f64"},
};
static const ZrLibFunctionDescriptor functions[] = {
    ZRG_FUNCTION("nowMicros", now_micros, "int", NULL, 0),
    ZRG_FUNCTION("zoneBegin", zone_begin, "void", zone_args, 1),
    ZRG_FUNCTION("zoneEnd", zone_end, "void", zone_args, 1),
    ZRG_FUNCTION("zoneReport", zone_report, "string", NULL, 0),
    ZRG_FUNCTION("openSink", open_sink, "int", path_args, 1),
    ZRG_FUNCTION("writeSink", write_sink, "void", write_args, 2),
    ZRG_FUNCTION("closeSink", close_sink, "void", handle_args, 1),
    ZRG_FUNCTION("rssKb", rss_kb, "int", NULL, 0),
    ZRG_FUNCTION("isHostDriven", is_host_driven, "bool", NULL, 0),
    ZRG_FUNCTION("addI64", add_i64, "i64", add_args, 2),
    ZRG_FUNCTION("noise2", noise2, "f64", noise_args, 2),
    ZRG_FUNCTION("nop", nop, "void", NULL, 0),
};

const ZrLibModuleDescriptor *ZrGame_HostModule(void) {
    static const ZrLibModuleDescriptor module = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION, .moduleName = "zrgame.host",
        .functions = functions, .functionCount = ZR_ARRAY_COUNT(functions),
        .documentation = "Swarm clock, inclusive nested zones and newline metrics sinks. Single owner thread.",
        .moduleVersion = "1.1.0", .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, .publicContractHash = "zrgame.host:v2",
    };
    return &module;
}
