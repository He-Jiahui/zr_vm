#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "harness/runtime_support.h"
#include "harness/module_fixture_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/property_reference.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/native_binding_call_binding.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

static TZrBool call_binding_probe(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, result, 7);
    return ZR_TRUE;
}

static TZrBool call_binding_make(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *object = ZrLib_Type_NewInstance(context->state, "NativeThing");
    SZrTypeValue stored;
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetInt(context->state, &stored, 19);
    ZrLib_Object_SetFieldCString(context->state, object, "stored", &stored);
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrBool call_binding_get_value(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *self = ZrLib_CallContext_Self(context);
    SZrObject *object;
    const SZrTypeValue *value;
    if (self == ZR_NULL || self->value.object == ZR_NULL) return ZR_FALSE;
    object = ZR_CAST_OBJECT(context->state, self->value.object);
    value = ZrLib_Object_GetFieldCString(context->state, object, "stored");
    if (value == ZR_NULL) return ZR_FALSE;
    ZrCore_Value_CopySlow(context->state, result, value);
    return ZR_TRUE;
}

static void close_fixture_reader(SZrState *state, TZrPtr data) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(data);
}

static const ZrLibFunctionDescriptor kFunctions[] = {
        {
                .name = "probe",
                .minArgumentCount = 0u,
                .maxArgumentCount = 0u,
                .callback = call_binding_probe,
                .returnTypeName = "int",
        },
};

static const ZrLibMethodDescriptor kMethods[] = {
        {
                .name = "read",
                .minArgumentCount = 0u,
                .maxArgumentCount = 0u,
                .callback = call_binding_probe,
                .returnTypeName = "int",
        },
        {
                .name = "__get_value",
                .minArgumentCount = 0u,
                .maxArgumentCount = 0u,
                .callback = call_binding_get_value,
                .returnTypeName = "int",
                .propertyName = "value",
                .propertyReferenceAccess = ZR_LIB_REFERENCE_ACCESS_READONLY,
        },
};

static const ZrLibFieldDescriptor kFields[] = {
        {.name = "stored", .typeName = "int"},
};

static const ZrLibMetaMethodDescriptor kMetaMethods[] = {
        {
                .metaType = ZR_META_CALL,
                .minArgumentCount = 0u,
                .maxArgumentCount = 0u,
                .callback = call_binding_probe,
                .returnTypeName = "int",
        },
};

static const ZrLibTypeDescriptor kTypes[] = {
        {
                .name = "NativeThing",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .fields = kFields,
                .fieldCount = ZR_ARRAY_COUNT(kFields),
                .methods = kMethods,
                .methodCount = ZR_ARRAY_COUNT(kMethods),
                .metaMethods = kMetaMethods,
                .metaMethodCount = ZR_ARRAY_COUNT(kMetaMethods),
        },
};

static const ZrLibFunctionDescriptor kTypeFunctions[] = {
        {.name = "make", .callback = call_binding_make, .returnTypeName = "NativeThing"},
};

static const ZrLibModuleDescriptor kModule = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "call_binding_native_registry",
        .functions = kFunctions,
        .functionCount = 1u,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

static const ZrLibModuleDescriptor kTypeModule = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "call_binding_native_types",
        .functions = kTypeFunctions,
        .functionCount = ZR_ARRAY_COUNT(kTypeFunctions),
        .types = kTypes,
        .typeCount = ZR_ARRAY_COUNT(kTypes),
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

int main(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *moduleName;
    SZrObjectModule *module;
    const SZrTypeValue *exportValue;
    SZrClosureNative *closure;
    SZrCallBindingContract contract;
    SZrCallBindingTarget target;
    SZrCallBindingDiagnostic diagnostic;
    EZrCallBindingStatus status;

    assert(state != ZR_NULL);
    assert(ZrLibrary_NativeRegistry_RegisterModule(state->global, &kModule));
    assert(ZrLibrary_NativeRegistry_RegisterModule(state->global, &kTypeModule));
    {
        SZrString *typeModuleName = ZrCore_String_CreateFromNative(state, (TZrNativeString)kTypeModule.moduleName);
        SZrObjectModule *typeModule = ZrCore_Module_ImportByPath(state, typeModuleName);
        assert(typeModule != ZR_NULL);
        {
            SZrObjectPrototype *prototype = ZrLib_Type_FindPrototype(state, "NativeThing");
            SZrObject *instance = ZrLib_Type_NewInstanceWithPrototype(state, prototype);
            SZrString *methodName = ZrCore_String_CreateFromNative(state, (TZrNativeString)"read");
            SZrTypeValue key;
            SZrTypeValue receiver;
            SZrTypeValue result;
            const SZrTypeValue *callable;
            assert(prototype != ZR_NULL && instance != ZR_NULL && methodName != ZR_NULL);
            ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(methodName));
            key.type = ZR_VALUE_TYPE_STRING;
            callable = ZrCore_Object_GetValue(state, instance, &key);
            assert(callable != ZR_NULL);
            ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
            receiver.type = ZR_VALUE_TYPE_OBJECT;
            assert(ZrLib_CallValue(state, callable, &receiver, ZR_NULL, 0u, &result));
            assert(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) && result.value.nativeObject.nativeInt64 == 7);
            {
                SZrMeta *meta = ZrCore_Object_GetMetaRecursively(state->global, instance, ZR_META_CALL);
                SZrTypeValue metaCallable;
                assert(meta != ZR_NULL && meta->function != ZR_NULL);
                ZrCore_Value_InitAsRawObject(state, &metaCallable,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                assert(ZrLib_CallValue(state, &metaCallable, &receiver, ZR_NULL, 0u, &result));
                assert(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) && result.value.nativeObject.nativeInt64 == 7);
            }
        }
    }
    {
        const char *source =
                "var nativeModule = import(\"call_binding_native_types\");\n"
                "var thing = nativeModule.make();\n"
                "return thing.read();\n";
        SZrFunction *compiled = ZrParser_Source_Compile(state, source, strlen(source),
                ZrCore_String_CreateFromNative(state, "native_call_binding_pipeline.zr"));
        TZrInt64 result = 0;
        TZrBool sawProviderBinding = ZR_FALSE;
        assert(compiled != ZR_NULL);
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < compiled->callSiteCacheLength; ++cacheIndex) {
            const SZrCallBindingContract *fact = &compiled->callSiteCaches[cacheIndex].binding.contract;
            if (fact->moduleSignatureHash == ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(&kTypeModule)) {
                sawProviderBinding = ZR_TRUE;
                assert(compiled->callSiteCaches[cacheIndex].bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE);
            }
        }
        assert(sawProviderBinding);
        assert(ZrTests_Runtime_Function_ExecuteExpectInt64(state, compiled, &result));
        assert(result == 7);
        {
            const TZrChar *path = "native_call_binding_pipeline.zro";
            TZrByte *bytes;
            TZrSize byteLength = 0u;
            ZrTestsFixtureReader fixture = {0};
            SZrIo *io;
            SZrIoSource *artifact;
            SZrFunction *loaded;
            TZrInt64 loadedResult = 0;
            assert(ZrParser_Writer_WriteBinaryFile(state, compiled, path));
            bytes = ZrTests_Fixture_ReadFileBytes(path, &byteLength);
            assert(bytes != ZR_NULL && byteLength != 0u);
            fixture.bytes = bytes;
            fixture.length = byteLength;
            io = ZrCore_Io_New(state->global);
            assert(io != ZR_NULL);
            ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead,
                    close_fixture_reader, &fixture);
            io->isBinary = ZR_TRUE;
            artifact = ZrCore_Io_ReadSourceNew(io);
            assert(artifact != ZR_NULL);
            loaded = ZrCore_Io_LoadEntryFunctionToRuntime(state, artifact);
            assert(loaded != ZR_NULL);
            assert(ZrTests_Runtime_Function_ExecuteExpectInt64(state, loaded, &loadedResult));
            assert(loadedResult == 7);
            ZrCore_Io_ReadSourceFree(state->global, artifact);
            ZrCore_Io_Free(state->global, io);
            free(bytes);
            remove(path);
        }
    }
    moduleName = ZrCore_String_CreateFromNative(state, (TZrNativeString)kModule.moduleName);
    assert(moduleName != ZR_NULL);
    module = ZrCore_Module_ImportByPath(state, moduleName);
    assert(module != ZR_NULL);
    exportValue = ZrCore_Module_GetPubExport(state, module, ZrCore_String_CreateFromNative(state, "probe"));
    assert(exportValue != ZR_NULL && exportValue->value.object != ZR_NULL);
    closure = (SZrClosureNative *)exportValue->value.object;
    assert(ZrLibrary_NativeRegistry_GetCallBindingIdentity(state->global, closure, &contract));
    assert(contract.targetMetadataToken != 0u && contract.signatureToken != 0u);
    {
        SZrCallBindingContract methodContract;
        SZrCallBindingContract propertyContract;
        SZrCallBindingContract metaContract;
        assert(ZrLibrary_NativeCallBinding_GetDescriptorContract(
                &kTypeModule, &kTypes[0], ZR_NATIVE_CALL_BINDING_METHOD, &kMethods[0], &methodContract));
        assert(ZrLibrary_NativeCallBinding_GetDescriptorContract(
                &kTypeModule, &kTypes[0], ZR_NATIVE_CALL_BINDING_METHOD, &kMethods[1], &propertyContract));
        assert(ZrLibrary_NativeCallBinding_GetDescriptorContract(
                &kTypeModule, &kTypes[0], ZR_NATIVE_CALL_BINDING_META_METHOD, &kMetaMethods[0], &metaContract));
        assert(methodContract.operation == ZR_CALL_BINDING_OPERATION_CALL);
        assert(propertyContract.operation == ZR_CALL_BINDING_OPERATION_GET);
        assert(metaContract.operation == ZR_CALL_BINDING_OPERATION_META);
        assert(methodContract.ownerTypeToken != 0u && methodContract.layoutHash != 0u);
        assert(methodContract.moduleSignatureHash == ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(&kTypeModule));
    }

    memset(&diagnostic, 0, sizeof(diagnostic));
    status = ZrLibrary_NativeRegistry_ResolveCallBinding(state, &contract, &target, &diagnostic);
    assert(status == ZR_CALL_BINDING_OK);
    assert(target.targetKind == ZR_CALL_BINDING_TARGET_NATIVE);
    assert(target.callableObject == (SZrRawObject *)closure);
    assert(target.native.function == closure->nativeFunction);

    contract.signatureHash ^= 1u;
    status = ZrLibrary_NativeRegistry_ResolveCallBinding(state, &contract, &target, &diagnostic);
    assert(status == ZR_CALL_BINDING_SIGNATURE_MISMATCH);
    assert(diagnostic.status == ZR_CALL_BINDING_SIGNATURE_MISMATCH);
    {
        const char *source = "var provider = import(\"call_binding_native_registry\"); return provider.probe();";
        SZrFunction *compiled = ZrParser_Source_Compile(state, source, strlen(source),
                ZrCore_String_CreateFromNative(state, "native_call_binding_reload.zr"));
        static ZrLibModuleDescriptor replacement;
        TZrInt64 result = 0;
        assert(compiled != ZR_NULL);
        assert(ZrTests_Runtime_Function_ExecuteExpectInt64(state, compiled, &result));
        assert(result == 7);
        replacement = kModule;
        assert(ZrLibrary_NativeRegistry_RegisterModule(state->global, &replacement));
        assert(!ZrTests_Runtime_Function_ExecuteExpectInt64(state, compiled, &result));
        assert(state->lastCallBindingError.status == ZR_CALL_BINDING_STALE_GENERATION);
    }
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
