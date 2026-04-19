//
// Internal native binding interfaces shared across translation units.
//

#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_INTERNAL_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_INTERNAL_H

//
// Descriptor-driven native binding implementation.
//

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_library/native_binding.h"

#include "zr_vm_library/file.h"
#include "zr_vm_library/native_hints.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#include <direct.h>
#elif defined(ZR_PLATFORM_DARWIN)
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef enum EZrLibResolvedBindingKind {
    ZR_LIB_RESOLVED_BINDING_FUNCTION = 0,
    ZR_LIB_RESOLVED_BINDING_METHOD = 1,
    ZR_LIB_RESOLVED_BINDING_META_METHOD = 2
} EZrLibResolvedBindingKind;

typedef struct ZrLibBindingEntry {
    SZrClosureNative *closure;
    EZrLibResolvedBindingKind bindingKind;
    const ZrLibModuleDescriptor *moduleDescriptor;
    const ZrLibTypeDescriptor *typeDescriptor;
    SZrObjectPrototype *ownerPrototype;
    union {
        const ZrLibFunctionDescriptor *functionDescriptor;
        const ZrLibMethodDescriptor *methodDescriptor;
        const ZrLibMetaMethodDescriptor *metaMethodDescriptor;
    } descriptor;
} ZrLibBindingEntry;

typedef struct ZrLibStableValueCopy {
    SZrTypeValue value;
    TZrBool needsRelease;
} ZrLibStableValueCopy;

typedef struct ZrLibRegisteredModuleRecord {
    const ZrLibModuleDescriptor *descriptor;
    TZrChar *moduleName;
    TZrChar *sourcePath;
    EZrLibNativeModuleRegistrationKind registrationKind;
    TZrBool isDescriptorPlugin;
} ZrLibRegisteredModuleRecord;

typedef struct ZrLibPluginHandleRecord {
    void *handle;
    TZrChar *moduleName;
    TZrChar *sourcePath;
    TZrChar *loadedPath;
} ZrLibPluginHandleRecord;

#define ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_CAPACITY 2u
#define ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX ZR_MAX_SIZE

typedef struct ZrLibrary_NativeRegistryState {
    SZrArray moduleRecords;
    SZrArray bindingEntries;
    SZrArray pluginHandles;
    TZrSize bindingLookupHotIndices[ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_CAPACITY];
    EZrLibNativeRegistryErrorCode lastErrorCode;
    TZrChar lastErrorMessage[ZR_LIBRARY_NATIVE_REGISTRY_ERROR_BUFFER_LENGTH];
} ZrLibrary_NativeRegistryState;

TZrInt64 native_binding_dispatcher(SZrState *state);
TZrInt64 native_binding_dispatch_cached_stack_root_one_argument(SZrState *state);
TZrInt64 native_binding_dispatch_cached_stack_root_two_arguments(SZrState *state);

static ZR_FORCE_INLINE TZrUInt32 native_binding_descriptor_dispatch_flags(EZrLibResolvedBindingKind bindingKind,
                                                                          const void *descriptor) {
    switch (bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION: {
            const ZrLibFunctionDescriptor *functionDescriptor = (const ZrLibFunctionDescriptor *)descriptor;
            return functionDescriptor != ZR_NULL ? functionDescriptor->dispatchFlags : 0U;
        }
        case ZR_LIB_RESOLVED_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptor;
            return methodDescriptor != ZR_NULL ? methodDescriptor->dispatchFlags : 0U;
        }
        case ZR_LIB_RESOLVED_BINDING_META_METHOD: {
            const ZrLibMetaMethodDescriptor *metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)descriptor;
            return metaMethodDescriptor != ZR_NULL ? metaMethodDescriptor->dispatchFlags : 0U;
        }
        default:
            return 0U;
    }
}

static ZR_FORCE_INLINE TZrBool native_binding_descriptor_fixed_argument_count(EZrLibResolvedBindingKind bindingKind,
                                                                              const void *descriptor,
                                                                              TZrSize *outCount) {
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;

    if (descriptor == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION: {
            const ZrLibFunctionDescriptor *functionDescriptor = (const ZrLibFunctionDescriptor *)descriptor;
            if (functionDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = functionDescriptor->minArgumentCount;
            maxArgumentCount = functionDescriptor->maxArgumentCount;
            break;
        }
        case ZR_LIB_RESOLVED_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptor;
            if (methodDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = methodDescriptor->minArgumentCount;
            maxArgumentCount = methodDescriptor->maxArgumentCount;
            break;
        }
        case ZR_LIB_RESOLVED_BINDING_META_METHOD: {
            const ZrLibMetaMethodDescriptor *metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)descriptor;
            if (metaMethodDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = metaMethodDescriptor->minArgumentCount;
            maxArgumentCount = metaMethodDescriptor->maxArgumentCount;
            break;
        }
        default:
            return ZR_FALSE;
    }

    if (minArgumentCount != maxArgumentCount) {
        return ZR_FALSE;
    }

    *outCount = (TZrSize)minArgumentCount;
    return ZR_TRUE;
}
static ZR_FORCE_INLINE TZrBool native_binding_descriptor_uses_receiver(EZrLibResolvedBindingKind bindingKind,
                                                                       const void *descriptor) {
    switch (bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            return ZR_FALSE;
        case ZR_LIB_RESOLVED_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptor;
            return methodDescriptor != ZR_NULL && !methodDescriptor->isStatic;
        }
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static ZR_FORCE_INLINE FZrLibBoundCallback native_binding_descriptor_callback(EZrLibResolvedBindingKind bindingKind,
                                                                              const void *descriptor) {
    switch (bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION: {
            const ZrLibFunctionDescriptor *functionDescriptor = (const ZrLibFunctionDescriptor *)descriptor;
            return functionDescriptor != ZR_NULL ? functionDescriptor->callback : ZR_NULL;
        }
        case ZR_LIB_RESOLVED_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptor;
            return methodDescriptor != ZR_NULL ? methodDescriptor->callback : ZR_NULL;
        }
        case ZR_LIB_RESOLVED_BINDING_META_METHOD: {
            const ZrLibMetaMethodDescriptor *metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)descriptor;
            return metaMethodDescriptor != ZR_NULL ? metaMethodDescriptor->callback : ZR_NULL;
        }
        default:
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE FZrNativeFunction native_binding_closure_dispatcher_for_cached_binding(
        const SZrClosureNative *closure) {
    EZrLibResolvedBindingKind bindingKind;
    const void *descriptor;
    TZrUInt32 dispatchFlags;
    TZrSize argumentCount;

    if (closure == ZR_NULL || closure->nativeBindingDescriptor == ZR_NULL) {
        return native_binding_dispatcher;
    }

    bindingKind = (EZrLibResolvedBindingKind)closure->nativeBindingKind;
    descriptor = (const void *)closure->nativeBindingDescriptor;
    dispatchFlags = native_binding_descriptor_dispatch_flags(bindingKind, descriptor);
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT) != 0U &&
        native_binding_descriptor_fixed_argument_count(bindingKind, descriptor, &argumentCount)) {
        if (argumentCount == 1u) {
            return native_binding_dispatch_cached_stack_root_one_argument;
        }
        if (argumentCount == 2u) {
            return native_binding_dispatch_cached_stack_root_two_arguments;
        }
    }

    return native_binding_dispatcher;
}

static ZR_FORCE_INLINE void native_binding_closure_refresh_direct_dispatch_cache(SZrClosureNative *closure) {
    EZrLibResolvedBindingKind bindingKind;
    const void *descriptor;
    TZrUInt32 dispatchFlags;
    TZrSize argumentCount;
    FZrLibBoundCallback callback;

    if (closure == ZR_NULL) {
        return;
    }

    memset(&closure->nativeBindingDirectDispatch, 0, sizeof(closure->nativeBindingDirectDispatch));
    if (closure->nativeBindingDescriptor == ZR_NULL || closure->nativeBindingUsesReceiver == 0u) {
        return;
    }

    bindingKind = (EZrLibResolvedBindingKind)closure->nativeBindingKind;
    descriptor = (const void *)closure->nativeBindingDescriptor;
    dispatchFlags = native_binding_descriptor_dispatch_flags(bindingKind, descriptor);
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT) == 0u ||
        !native_binding_descriptor_fixed_argument_count(bindingKind, descriptor, &argumentCount)) {
        return;
    }

    callback = native_binding_descriptor_callback(bindingKind, descriptor);
    if (callback == ZR_NULL) {
        return;
    }

    closure->nativeBindingDirectDispatch.callback = callback;
    closure->nativeBindingDirectDispatch.moduleDescriptor = closure->nativeBindingModuleDescriptor;
    closure->nativeBindingDirectDispatch.typeDescriptor = closure->nativeBindingTypeDescriptor;
    closure->nativeBindingDirectDispatch.ownerPrototype = (SZrObjectPrototype *)closure->nativeBindingOwnerPrototype;
    closure->nativeBindingDirectDispatch.rawArgumentCount = (TZrUInt32)(argumentCount + 1u);
    closure->nativeBindingDirectDispatch.usesReceiver = ZR_TRUE;
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL);
    }
    switch (bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            closure->nativeBindingDirectDispatch.functionDescriptor = descriptor;
            break;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            closure->nativeBindingDirectDispatch.methodDescriptor = descriptor;
            break;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            closure->nativeBindingDirectDispatch.metaMethodDescriptor = descriptor;
            closure->nativeBindingDirectDispatch.readonlyInlineGetFastCallback =
                    ((const ZrLibMetaMethodDescriptor *)descriptor)->readonlyInlineGetFastCallback;
            closure->nativeBindingDirectDispatch.readonlyInlineSetNoResultFastCallback =
                    ((const ZrLibMetaMethodDescriptor *)descriptor)->readonlyInlineSetNoResultFastCallback;
            break;
        default:
            memset(&closure->nativeBindingDirectDispatch, 0, sizeof(closure->nativeBindingDirectDispatch));
            break;
    }
}

static ZR_FORCE_INLINE void native_binding_closure_store_cached_binding(SZrClosureNative *closure,
                                                                        TZrSize index,
                                                                        EZrLibResolvedBindingKind bindingKind,
                                                                        const ZrLibModuleDescriptor *moduleDescriptor,
                                                                        const ZrLibTypeDescriptor *typeDescriptor,
                                                                        SZrObjectPrototype *ownerPrototype,
                                                                        const void *descriptor) {
    if (closure == ZR_NULL) {
        return;
    }

    closure->nativeBindingLookupIndex = index;
    closure->nativeBindingDescriptor = (TZrPtr)descriptor;
    closure->nativeBindingModuleDescriptor = (TZrPtr)moduleDescriptor;
    closure->nativeBindingTypeDescriptor = (TZrPtr)typeDescriptor;
    closure->nativeBindingOwnerPrototype = (TZrPtr)ownerPrototype;
    closure->nativeBindingKind = (TZrUInt32)bindingKind;
    closure->nativeBindingUsesReceiver = native_binding_descriptor_uses_receiver(bindingKind, descriptor);
    native_binding_closure_refresh_direct_dispatch_cache(closure);
    closure->nativeFunction = native_binding_closure_dispatcher_for_cached_binding(closure);
}

static ZR_FORCE_INLINE void native_binding_closure_invalidate_cached_lookup(SZrClosureNative *closure) {
    if (closure == ZR_NULL) {
        return;
    }

    closure->nativeBindingLookupIndex = ZR_MAX_SIZE;
}

static ZR_FORCE_INLINE TZrBool native_binding_closure_try_build_cached_entry(SZrClosureNative *closure,
                                                                             ZrLibBindingEntry *entry) {
    if (closure == ZR_NULL || entry == ZR_NULL || closure->nativeBindingDescriptor == ZR_NULL ||
        closure->nativeBindingModuleDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    entry->closure = closure;
    entry->bindingKind = (EZrLibResolvedBindingKind)closure->nativeBindingKind;
    entry->moduleDescriptor = (const ZrLibModuleDescriptor *)closure->nativeBindingModuleDescriptor;
    entry->typeDescriptor = (const ZrLibTypeDescriptor *)closure->nativeBindingTypeDescriptor;
    entry->ownerPrototype = (SZrObjectPrototype *)closure->nativeBindingOwnerPrototype;

    switch (entry->bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            entry->descriptor.functionDescriptor = (const ZrLibFunctionDescriptor *)closure->nativeBindingDescriptor;
            return ZR_TRUE;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            entry->descriptor.methodDescriptor = (const ZrLibMethodDescriptor *)closure->nativeBindingDescriptor;
            return ZR_TRUE;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            entry->descriptor.metaMethodDescriptor =
                    (const ZrLibMetaMethodDescriptor *)closure->nativeBindingDescriptor;
            return ZR_TRUE;
        default:
            memset(entry, 0, sizeof(*entry));
            return ZR_FALSE;
    }
}

static ZR_FORCE_INLINE TZrBool native_binding_closure_try_get_cached_callback(const SZrClosureNative *closure,
                                                                              FZrLibBoundCallback *outCallback) {
    if (outCallback != ZR_NULL) {
        *outCallback = ZR_NULL;
    }

    if (closure == ZR_NULL || outCallback == ZR_NULL || closure->nativeBindingDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    *outCallback = native_binding_descriptor_callback((EZrLibResolvedBindingKind)closure->nativeBindingKind,
                                                      (const void *)closure->nativeBindingDescriptor);
    return *outCallback != ZR_NULL;
}

static ZR_FORCE_INLINE void native_binding_init_call_context_layout_cached(ZrLibCallContext *context,
                                                                           TZrStackValuePointer functionBase,
                                                                           TZrSize rawArgumentCount,
                                                                           TZrBool usesReceiver) {
    if (context == ZR_NULL) {
        return;
    }

    context->rawArgumentCount = rawArgumentCount;
    context->stackLayoutUsesReceiver = usesReceiver;
    context->stackLayoutAnchored = ZR_FALSE;
    context->stackBasePointer = ZR_NULL;

    if (!usesReceiver) {
        context->argumentBase = functionBase + 1;
        context->argumentValues = ZR_NULL;
        context->argumentValuePointers = ZR_NULL;
        context->argumentCount = rawArgumentCount;
        context->selfValue = ZR_NULL;
        return;
    }

    context->selfValue = rawArgumentCount > 0 ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;
    context->argumentBase = rawArgumentCount > 0 ? functionBase + 2 : functionBase + 1;
    context->argumentValues = ZR_NULL;
    context->argumentValuePointers = ZR_NULL;
    context->argumentCount = rawArgumentCount > 0 ? rawArgumentCount - 1 : 0;
}

static ZR_FORCE_INLINE void native_binding_init_cached_stack_root_context(ZrLibCallContext *context,
                                                                          SZrState *state,
                                                                          const ZrLibBindingEntry *entry,
                                                                          TZrStackValuePointer functionBase,
                                                                          TZrSize rawArgumentCount,
                                                                          TZrBool usesReceiver) {
    if (context == ZR_NULL || entry == ZR_NULL) {
        return;
    }

    context->state = state;
    context->moduleDescriptor = entry->moduleDescriptor;
    context->typeDescriptor = entry->typeDescriptor;
    context->functionDescriptor =
            entry->bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION ? entry->descriptor.functionDescriptor : ZR_NULL;
    context->methodDescriptor =
            entry->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD ? entry->descriptor.methodDescriptor : ZR_NULL;
    context->metaMethodDescriptor =
            entry->bindingKind == ZR_LIB_RESOLVED_BINDING_META_METHOD ? entry->descriptor.metaMethodDescriptor
                                                                      : ZR_NULL;
    context->ownerPrototype = entry->ownerPrototype;
    context->constructTargetPrototype = entry->ownerPrototype;
    context->functionBase = functionBase;
    native_binding_init_call_context_layout_cached(context, functionBase, rawArgumentCount, usesReceiver);
}

static ZR_FORCE_INLINE void native_binding_init_cached_stack_root_context_from_closure(
        ZrLibCallContext *context,
        SZrState *state,
        const SZrClosureNative *closure,
        TZrStackValuePointer functionBase,
        TZrSize rawArgumentCount,
        TZrBool usesReceiver) {
    EZrLibResolvedBindingKind bindingKind;

    if (context == ZR_NULL || closure == ZR_NULL) {
        return;
    }

    bindingKind = (EZrLibResolvedBindingKind)closure->nativeBindingKind;
    context->state = state;
    context->moduleDescriptor = (const ZrLibModuleDescriptor *)closure->nativeBindingModuleDescriptor;
    context->typeDescriptor = (const ZrLibTypeDescriptor *)closure->nativeBindingTypeDescriptor;
    context->ownerPrototype = (SZrObjectPrototype *)closure->nativeBindingOwnerPrototype;
    context->constructTargetPrototype = context->ownerPrototype;
    context->functionDescriptor = bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION
                                          ? (const ZrLibFunctionDescriptor *)closure->nativeBindingDescriptor
                                          : ZR_NULL;
    context->methodDescriptor = bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD
                                        ? (const ZrLibMethodDescriptor *)closure->nativeBindingDescriptor
                                        : ZR_NULL;
    context->metaMethodDescriptor = bindingKind == ZR_LIB_RESOLVED_BINDING_META_METHOD
                                            ? (const ZrLibMetaMethodDescriptor *)closure->nativeBindingDescriptor
                                            : ZR_NULL;
    context->functionBase = functionBase;
    native_binding_init_call_context_layout_cached(context, functionBase, rawArgumentCount, usesReceiver);
}

#define kNativeEnumValueFieldName "__zr_enumValue"
#define kNativeEnumNameFieldName "__zr_enumName"
#define kNativeEnumValueTypeFieldName "__zr_enumValueTypeName"
#define kNativeAllowValueConstructionFieldName "__zr_allowValueConstruction"
#define kNativeAllowBoxedConstructionFieldName "__zr_allowBoxedConstruction"
#define kNativeFfiLoweringKindFieldName "__zr_ffiLoweringKind"
#define kNativeFfiViewTypeFieldName "__zr_ffiViewTypeName"
#define kNativeFfiUnderlyingTypeFieldName "__zr_ffiUnderlyingTypeName"
#define kNativeFfiOwnerModeFieldName "__zr_ffiOwnerMode"
#define kNativeFfiReleaseHookFieldName "__zr_ffiReleaseHook"

typedef const ZrLibModuleDescriptor *(*FZrVmGetNativeModuleV1)(void);

const ZrLibModuleDescriptor *ZrLibrary_BuiltinModule_GetDescriptor(void);
TZrBool native_binding_trace_import_enabled(void);
void native_binding_init_call_context_layout(ZrLibCallContext *context,
                                                    TZrStackValuePointer functionBase,
                                                    TZrSize rawArgumentCount);
void native_binding_trace_import(SZrState *state, const TZrChar *format, ...);
void native_registry_clear_error(ZrLibrary_NativeRegistryState *registry);
void native_registry_set_error(ZrLibrary_NativeRegistryState *registry,
                                      EZrLibNativeRegistryErrorCode errorCode,
                                      const TZrChar *format,
                                      ...);
TZrChar *native_registry_duplicate_string(SZrGlobalState *global, const TZrChar *text);
const TZrChar *native_binding_value_type_name(SZrState *state, const SZrTypeValue *value);
const TZrChar *native_binding_call_name(const ZrLibCallContext *context);
SZrString *native_binding_create_string(SZrState *state, const TZrChar *text);
SZrObjectModule *native_binding_import_module(SZrState *state, const TZrChar *moduleName);
void native_binding_register_prototype_in_global_scope(SZrState *state,
                                                              SZrString *typeName,
                                                              const SZrTypeValue *prototypeValue);
ZR_LIBRARY_API ZrLibrary_NativeRegistryState *native_registry_get(SZrGlobalState *global);
ZR_LIBRARY_API ZrLibBindingEntry *native_registry_find_binding(ZrLibrary_NativeRegistryState *registry,
                                                               SZrClosureNative *closure);
const ZrLibRegisteredModuleRecord *native_registry_find_record(ZrLibrary_NativeRegistryState *registry,
                                                                      const TZrChar *moduleName);
const ZrLibRegisteredModuleRecord *native_registry_find_record_by_descriptor(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor);
TZrBool native_registry_validate_descriptor_compatibility(ZrLibrary_NativeRegistryState *registry,
                                                                 const ZrLibModuleDescriptor *descriptor);
TZrBool native_registry_register_module_record(SZrGlobalState *global,
                                                      const ZrLibModuleDescriptor *descriptor,
                                                      EZrLibNativeModuleRegistrationKind registrationKind,
                                                      const TZrChar *sourcePath,
                                                      TZrBool isDescriptorPlugin);
void *native_registry_open_library(const TZrChar *path);
void native_registry_close_library(void *handle);
TZrPtr native_registry_find_symbol(void *handle, const TZrChar *symbolName);
const TZrChar *native_registry_dynamic_library_extension(void);
void native_registry_release_plugin_handle_record(SZrGlobalState *global, ZrLibPluginHandleRecord *handleRecord);
TZrBool native_registry_get_executable_directory(TZrChar *buffer, TZrSize bufferSize);
void native_registry_sanitize_module_name(const TZrChar *moduleName,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize);
const ZrLibModuleDescriptor *native_registry_load_plugin_descriptor(SZrState *state,
                                                                           ZrLibrary_NativeRegistryState *registry,
                                                                           const TZrChar *candidatePath,
                                                                           const TZrChar *requestedModuleName);
const ZrLibModuleDescriptor *native_registry_try_plugin_directory(SZrState *state,
                                                                         ZrLibrary_NativeRegistryState *registry,
                                                                         const TZrChar *directory,
                                                                         const TZrChar *moduleName);
const ZrLibModuleDescriptor *native_registry_try_plugin_paths(SZrState *state,
                                                                     ZrLibrary_NativeRegistryState *registry,
                                                                     const TZrChar *moduleName);
const ZrLibModuleDescriptor *native_registry_find_descriptor_or_plugin(SZrState *state,
                                                                              ZrLibrary_NativeRegistryState *registry,
                                                                              const TZrChar *moduleName);
SZrObjectModule *native_registry_resolve_loaded_module(SZrState *state,
                                                              ZrLibrary_NativeRegistryState *registry,
                                                              const TZrChar *moduleName);
ZR_LIBRARY_API TZrBool native_binding_make_callable_value(SZrState *state,
                                                          ZrLibrary_NativeRegistryState *registry,
                                                          EZrLibResolvedBindingKind bindingKind,
                                                          const ZrLibModuleDescriptor *moduleDescriptor,
                                                          const ZrLibTypeDescriptor *typeDescriptor,
                                                          SZrObjectPrototype *ownerPrototype,
                                                          const void *descriptor,
                                                          SZrTypeValue *value);
ZR_LIBRARY_API TZrBool native_binding_prepare_stable_value(SZrState *state,
                                                           ZrLibStableValueCopy *copy,
                                                           const SZrTypeValue *source);
ZR_LIBRARY_API void native_binding_release_stable_value(SZrState *state, ZrLibStableValueCopy *copy);
TZrStackValuePointer native_binding_resolve_call_scratch_base(TZrStackValuePointer stackTop,
                                                                      const SZrCallInfo *callInfo);
SZrObject *native_binding_new_instance_with_prototype(SZrState *state, SZrObjectPrototype *prototype);
SZrObjectModule *native_registry_materialize_module(SZrState *state,
                                                           ZrLibrary_NativeRegistryState *registry,
                                                           const ZrLibModuleDescriptor *descriptor);
struct SZrObjectModule *native_registry_loader(SZrState *state, SZrString *moduleName, TZrPtr userData);
void native_metadata_set_value_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            const SZrTypeValue *value);
void native_metadata_set_string_field(SZrState *state,
                                             SZrObject *object,
                                             const TZrChar *fieldName,
                                             const TZrChar *value);
void native_metadata_set_int_field(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          TZrInt64 value);
void native_metadata_set_float_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            TZrFloat64 value);
void native_metadata_set_bool_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrBool value);
const TZrChar *native_metadata_constant_type_name(const ZrLibConstantDescriptor *descriptor);
TZrBool native_descriptor_allows_value_construction(const ZrLibTypeDescriptor *descriptor);
TZrBool native_descriptor_allows_boxed_construction(const ZrLibTypeDescriptor *descriptor);
void native_metadata_set_constant_value_fields(SZrState *state,
                                                      SZrObject *object,
                                                      EZrLibConstantKind kind,
                                                      TZrInt64 intValue,
                                                      TZrFloat64 floatValue,
                                                      const TZrChar *stringValue,
                                                      TZrBool boolValue);
TZrBool native_metadata_push_string_value(SZrState *state, SZrObject *array, const TZrChar *value);
SZrObject *native_metadata_make_string_array(SZrState *state,
                                                    const TZrChar *const *values,
                                                    TZrSize valueCount);
SZrObject *native_metadata_make_field_entry(SZrState *state, const ZrLibFieldDescriptor *descriptor);
SZrObject *native_metadata_make_method_entry(SZrState *state, const ZrLibMethodDescriptor *descriptor);
SZrObject *native_metadata_make_meta_method_entry(SZrState *state,
                                                         const ZrLibMetaMethodDescriptor *descriptor);
SZrObject *native_metadata_make_function_entry(SZrState *state, const ZrLibFunctionDescriptor *descriptor);
SZrObject *native_metadata_make_constant_entry(SZrState *state, const ZrLibConstantDescriptor *descriptor);
SZrObject *native_metadata_make_enum_member_entry(SZrState *state, const ZrLibEnumMemberDescriptor *descriptor);
SZrObject *native_metadata_make_module_link_entry(SZrState *state, const ZrLibModuleLinkDescriptor *descriptor);
SZrObject *native_metadata_make_type_entry(SZrState *state, const ZrLibTypeDescriptor *descriptor);
ZR_LIBRARY_API SZrObject *native_metadata_make_module_info(SZrState *state,
                                                           const ZrLibModuleDescriptor *descriptor,
                                                           const ZrLibRegisteredModuleRecord *record);
TZrBool native_registry_add_constant(SZrState *state,
                                            SZrObjectModule *module,
                                            const ZrLibConstantDescriptor *descriptor);
TZrBool native_registry_add_function(SZrState *state,
                                            ZrLibrary_NativeRegistryState *registry,
                                            SZrObjectModule *module,
                                            const ZrLibModuleDescriptor *moduleDescriptor,
                                            const ZrLibFunctionDescriptor *functionDescriptor);
TZrBool native_registry_add_module_link(SZrState *state,
                                               ZrLibrary_NativeRegistryState *registry,
                                               SZrObjectModule *module,
                                               const ZrLibModuleLinkDescriptor *descriptor);
TZrBool native_registry_add_methods(SZrState *state,
                                           ZrLibrary_NativeRegistryState *registry,
                                           const ZrLibModuleDescriptor *moduleDescriptor,
                                           const ZrLibTypeDescriptor *typeDescriptor,
                                           SZrObjectPrototype *prototype);
void native_registry_set_hidden_string_metadata(SZrState *state,
                                                       SZrObject *object,
                                                       const TZrChar *fieldName,
                                                       const TZrChar *value);
void native_registry_set_hidden_bool_metadata(SZrState *state,
                                                     SZrObject *object,
                                                     const TZrChar *fieldName,
                                                     TZrBool value);
TZrBool native_registry_init_enum_member_scalar(SZrState *state,
                                                       const ZrLibEnumMemberDescriptor *descriptor,
                                                       SZrTypeValue *value);
SZrObject *native_registry_make_enum_instance(SZrState *state,
                                                     SZrObjectPrototype *prototype,
                                                     const SZrTypeValue *underlyingValue,
                                                     const TZrChar *memberName);
void native_registry_attach_type_runtime_metadata(SZrState *state,
                                                         const ZrLibTypeDescriptor *typeDescriptor,
                                                         SZrObjectPrototype *prototype);
TZrBool native_registry_add_enum_members(SZrState *state,
                                                SZrObjectPrototype *prototype,
                                                const ZrLibTypeDescriptor *typeDescriptor);
TZrBool native_registry_add_type(SZrState *state,
                                        ZrLibrary_NativeRegistryState *registry,
                                        SZrObjectModule *module,
                                        const ZrLibModuleDescriptor *moduleDescriptor,
                                        const ZrLibTypeDescriptor *typeDescriptor);
SZrObjectPrototype *native_registry_get_module_prototype(SZrState *state,
                                                                SZrObjectModule *module,
                                                                const TZrChar *typeName);
void native_registry_resolve_type_relationships(SZrState *state,
                                                       SZrObjectModule *module,
                                                       const ZrLibModuleDescriptor *descriptor);
TZrBool native_binding_auto_check_arity(const ZrLibCallContext *context);
TZrStackValuePointer native_binding_temp_root_slot(ZrLibTempValueRoot *root);

#endif // ZR_VM_LIBRARY_NATIVE_BINDING_INTERNAL_H
