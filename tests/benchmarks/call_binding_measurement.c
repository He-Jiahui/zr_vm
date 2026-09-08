#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "harness/module_fixture_support.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"

#if defined(__linux__) && defined(__has_include)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#endif
#endif
#ifndef CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_START_INSTRUMENTATION ((void)0)
#define CALLGRIND_STOP_INSTRUMENTATION ((void)0)
#define CALLGRIND_DUMP_STATS ((void)0)
#endif

static TZrBool read_probe(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetInt(context->state, result, 7);
    return ZR_TRUE;
}

static TZrBool make_probe(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *object = ZrLib_Type_NewInstance(context->state, "BindingProbe");
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static const ZrLibMethodDescriptor methods[] = {
    {.name = "read", .callback = read_probe, .returnTypeName = "int"}
};
static const ZrLibTypeDescriptor types[] = {
    {.name = "BindingProbe", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
     .methods = methods, .methodCount = ZR_ARRAY_COUNT(methods)}
};
static const ZrLibFunctionDescriptor functions[] = {
    {.name = "make", .callback = make_probe, .returnTypeName = "BindingProbe"}
};
static const ZrLibModuleDescriptor nativeModule = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION, .moduleName = "binding_probe",
    .functions = functions, .functionCount = ZR_ARRAY_COUNT(functions),
    .types = types, .typeCount = ZR_ARRAY_COUNT(types), .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION
};

static TZrBool load_config(SZrState *state, TZrNativeString path, TZrNativeString hash, SZrIo *io) {
    static const ZrTestsFixtureSource config = ZR_TESTS_FIXTURE_SOURCE_TEXT(
            "bench_config", "pub fn scale(): int { return 1; }");
    return ZrTests_Fixture_SourceLoaderFromArray(state, path, hash, io, &config, 1u);
}

typedef struct Measurement {
    TZrBool cacheOnly;
    TZrUInt32 bindings;
    TZrUInt32 retainedImports;
    TZrUInt64 hits;
} Measurement;

static TZrBool prepare_measurement(SZrFunction *function, void *context) {
    Measurement *measurement = context;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        /* Import-call lowering has removed the terminal member load. Keep
         * those bindings in both modes so this remains the same program. */
        if (entry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL &&
            (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
             entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE)) {
            ++measurement->retainedImports;
            continue;
        }
        ++measurement->bindings;
        if (measurement->cacheOnly) {
            ZrCore_CallBinding_Invalidate(&entry->binding);
            memset(&entry->binding.contract, 0, sizeof(entry->binding.contract));
            if (function->callBindingInstructionMap != ZR_NULL &&
                entry->instructionIndex < function->callBindingInstructionMapLength)
                function->callBindingInstructionMap[entry->instructionIndex] = 0u;
        }
    }
    return ZR_TRUE;
}

static TZrBool count_hits(SZrFunction *function, void *context) {
    Measurement *measurement = context;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index)
        measurement->hits += function->callSiteCaches[index].runtimeHitCount;
    return ZR_TRUE;
}

int main(int argc, char **argv) {
    const char *source;
    TZrByte *fileSource = ZR_NULL;
    TZrSize fileLength = 0u;
    SZrState *state;
    SZrFunction *function;
    SZrTypeValue result;
    Measurement measurement = {0};
    if (argc != 3 || (strcmp(argv[2], "bound") != 0 && strcmp(argv[2], "cache-only") != 0)) {
        fprintf(stderr, "Usage: %s <call_chain_polymorphic|native_member|accessor> <bound|cache-only>\n", argv[0]);
        return 2;
    }
    measurement.cacheOnly = strcmp(argv[2], "cache-only") == 0;
    if (strcmp(argv[1], "call_chain_polymorphic") == 0) {
        fileSource = ZrTests_Fixture_ReadFileBytes(ZR_VM_TESTS_REPO_ROOT
                "/tests/benchmarks/cases/call_chain_polymorphic/zr/src/main.zr", &fileLength);
        if (fileSource == ZR_NULL) return 2;
        source = (const char *)fileSource;
    } else if (strcmp(argv[1], "native_member") == 0) {
        source = "var native = import(\"binding_probe\"); var value = native.make(); "
                "var sum: int = 0; var i: int = 0; while (i < 4000) { sum = sum + value.read(); i = i + 1; } return sum;";
    } else if (strcmp(argv[1], "accessor") == 0) {
        source = "class Box { pri var stored: int = 0; pub property value: int { "
                "get { return this.stored; } set { this.stored = value; } } } "
                "var box = new Box(); var sum: int = 0; var i: int = 0; "
                "while (i < 4000) { box.value = i; sum = sum + box.value; i = i + 1; } return sum;";
    } else return 2;
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) return 1;
    state->global->sourceLoader = load_config;
    state->global->compileSource = ZrParser_Source_Compile;
    if (!ZrLibrary_NativeRegistry_RegisterModule(state->global, &nativeModule)) return 1;
    function = ZrParser_Source_Compile(state, source, fileSource != ZR_NULL ? fileLength : strlen(source),
            ZrCore_String_CreateFromNative(state, "call_binding_measurement.zr"));
    free(fileSource);
    if (function == ZR_NULL || !ZrCore_CallBinding_VisitFunctions(function, prepare_measurement, &measurement)) return 1;
    CALLGRIND_START_INSTRUMENTATION;
    for (TZrUInt32 iteration = 0u; iteration < 5u; ++iteration) {
        if (!ZrTests_Runtime_Function_Execute(state, function, &result)) return 1;
    }
    CALLGRIND_DUMP_STATS;
    CALLGRIND_STOP_INSTRUMENTATION;
    ZrCore_CallBinding_VisitFunctions(function, count_hits, &measurement);
    printf("case=%s mode=%s bindings=%u retained_imports=%u hits=%llu\n", argv[1], argv[2],
            (unsigned)measurement.bindings, (unsigned)measurement.retainedImports,
            (unsigned long long)measurement.hits);
    if (result.type == ZR_VALUE_TYPE_STRING)
        printf("%s\n", ZrCore_String_GetNativeString((SZrString *)result.value.object));
    else if (ZR_VALUE_IS_TYPE_INT(result.type))
        printf("%lld\n", (long long)result.value.nativeObject.nativeInt64);
    else return 1;
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
