#ifndef ZR_VM_RUST_BINDING_INTERNAL_H
#define ZR_VM_RUST_BINDING_INTERNAL_H

#include "zr_vm_rust_binding.h"

#include <stdarg.h>

#include "project/project.h"
#include "compiler/compiler.h"
#include "runtime/runtime.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_binding.h"

typedef enum EZrRustBindingValueStorageKind {
    ZR_RUST_BINDING_VALUE_STORAGE_OWNED = 0,
    ZR_RUST_BINDING_VALUE_STORAGE_VM = 1
} ZrRustBindingValueStorageKind;

typedef struct ZrRustBindingRuntimeNativeRegistry ZrRustBindingRuntimeNativeRegistry;

/* session、checkpoint 与 live Value 共享此计数；最后一个引用在 ownsGlobal 时先销毁 global，再释放 native descriptor。
 * 计数非原子，所有 owner 操作需由同一执行线程串行完成。 */
typedef struct ZrRustBindingExecutionOwner {
    SZrGlobalState *global;
    TZrBool ownsGlobal;
    ZrRustBindingNativeModule **nativeModules;
    TZrSize nativeModuleCount;
    TZrSize refCount;
    /* Owner references held by checkpoints, excluded from live Value-root checks. */
    TZrSize checkpointRefCount;
    TZrBool activeCall;
} ZrRustBindingExecutionOwner;

typedef struct ZrRustBindingOwnedArray {
    struct ZrRustBindingValue **items;
    TZrSize count;
    TZrSize capacity;
} ZrRustBindingOwnedArray;

typedef struct ZrRustBindingOwnedObjectField {
    TZrChar *name;
    struct ZrRustBindingValue *value;
} ZrRustBindingOwnedObjectField;

typedef struct ZrRustBindingOwnedObject {
    ZrRustBindingOwnedObjectField *fields;
    TZrSize count;
    TZrSize capacity;
} ZrRustBindingOwnedObject;

struct ZrRustBindingRuntime {
    ZrRustBindingRuntimeOptions options;
    ZrRustBindingRuntimeNativeRegistry *nativeRegistry;
    TZrBool standardProfile;
};

struct ZrRustBindingProjectWorkspace {
    SZrCliProjectContext context;
};

struct ZrRustBindingProjectSession {
    ZrRustBindingExecutionOwner *owner;
};

struct ZrRustBindingProjectSessionCheckpoint {
    struct SZrSessionCheckpoint *checkpoint;
    ZrRustBindingExecutionOwner *owner;
};

struct ZrRustBindingCompileResult {
    TZrSize compiledCount;
    TZrSize skippedCount;
    TZrSize removedCount;
};

struct ZrRustBindingManifestSnapshot {
    SZrCliIncrementalManifest manifest;
};

/* OWNED 保存 host 深拷贝，VM 保存 owner 加临时 GC root；两种存储决定跨 global 传参及释放方式。 */
struct ZrRustBindingValue {
    ZrRustBindingValueKind kind;
    ZrRustBindingOwnershipKind ownershipKind;
    ZrRustBindingValueStorageKind storageKind;
    union {
        TZrBool boolValue;
        TZrInt64 intValue;
        TZrFloat64 floatValue;
        TZrChar *stringValue;
        ZrRustBindingOwnedArray arrayValue;
        ZrRustBindingOwnedObject objectValue;
        struct {
            ZrRustBindingExecutionOwner *owner;
            ZrLibTempValueRoot root;
        } vmValue;
    } storage;
};

void zr_rust_binding_clear_error(void);
ZrRustBindingStatus zr_rust_binding_call_module_export_with_owner(
        ZrRustBindingExecutionOwner *owner,
        const TZrChar *moduleName,
        const TZrChar *exportName,
        ZrRustBindingValue *const *arguments,
        TZrSize argumentCount,
        ZrRustBindingValue **outResult,
        SZrExecutionBudget *budget);
ZrRustBindingStatus zr_rust_binding_set_error(ZrRustBindingStatus status, const TZrChar *format, ...);
TZrBool zr_rust_binding_copy_string_to_buffer(const TZrChar *source, TZrChar *buffer, TZrSize bufferSize);
TZrChar *zr_rust_binding_strdup(const TZrChar *value);

ZrRustBindingValueKind zr_rust_binding_map_value_kind(EZrValueType valueType);
ZrRustBindingOwnershipKind zr_rust_binding_map_ownership_kind(EZrOwnershipValueKind ownershipKind);

ZrRustBindingExecutionOwner *zr_rust_binding_execution_owner_new(SZrGlobalState *global,
                                                                 const struct ZrRustBindingRuntime *runtime);
void zr_rust_binding_execution_owner_retain(ZrRustBindingExecutionOwner *owner);
void zr_rust_binding_execution_owner_release(ZrRustBindingExecutionOwner *owner);

ZrRustBindingValue *zr_rust_binding_value_alloc(void);
void zr_rust_binding_value_free_impl(ZrRustBindingValue *value);
ZrRustBindingValue *zr_rust_binding_value_new_live(ZrRustBindingExecutionOwner *owner, const SZrTypeValue *value);
TZrBool zr_rust_binding_materialize_value(SZrState *state, const ZrRustBindingValue *value, SZrTypeValue *outValue);

TZrBool zr_rust_binding_runtime_register_native_modules(ZrRustBindingRuntime *runtime, SZrGlobalState *global);
TZrBool zr_rust_binding_runtime_capture_native_modules(const ZrRustBindingRuntime *runtime,
                                                       ZrRustBindingNativeModule ***outModules,
                                                       TZrSize *outCount);
void zr_rust_binding_runtime_native_registry_free(ZrRustBindingRuntime *runtime);
void zr_rust_binding_native_module_retain(ZrRustBindingNativeModule *module);
void zr_rust_binding_native_module_release(ZrRustBindingNativeModule *module);

#endif
