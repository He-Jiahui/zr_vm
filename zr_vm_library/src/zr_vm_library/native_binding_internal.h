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
} ZrLibPluginHandleRecord;

typedef struct ZrLibrary_NativeRegistryState {
    SZrArray moduleRecords;
    SZrArray bindingEntries;
    SZrArray pluginHandles;
    EZrLibNativeRegistryErrorCode lastErrorCode;
    TZrChar lastErrorMessage[ZR_LIBRARY_NATIVE_REGISTRY_ERROR_BUFFER_LENGTH];
} ZrLibrary_NativeRegistryState;

#define kNativeEnumValueFieldName "__zr_enumValue"
#define kNativeEnumNameFieldName "__zr_enumName"
#define kNativeEnumValueTypeFieldName "__zr_enumValueTypeName"
#define kNativeAllowValueConstructionFieldName "__zr_allowValueConstruction"
#define kNativeAllowBoxedConstructionFieldName "__zr_allowBoxedConstruction"

typedef const ZrLibModuleDescriptor *(*FZrVmGetNativeModuleV1)(void);

TZrBool native_binding_trace_import_enabled(void);
void native_binding_init_call_context_layout(ZrLibCallContext *context,
                                                    TZrStackValuePointer functionBase,
                                                    TZrSize rawArgumentCount);
void native_binding_trace_import(const TZrChar *format, ...);
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
ZrLibrary_NativeRegistryState *native_registry_get(SZrGlobalState *global);
ZrLibBindingEntry *native_registry_find_binding(ZrLibrary_NativeRegistryState *registry,
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
TZrBool native_binding_make_callable_value(SZrState *state,
                                                  ZrLibrary_NativeRegistryState *registry,
                                                  EZrLibResolvedBindingKind bindingKind,
                                                  const ZrLibModuleDescriptor *moduleDescriptor,
                                                  const ZrLibTypeDescriptor *typeDescriptor,
                                                  SZrObjectPrototype *ownerPrototype,
                                                  const void *descriptor,
                                                  SZrTypeValue *value);
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
SZrObject *native_metadata_make_module_info(SZrState *state,
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
TZrInt64 native_binding_dispatcher(SZrState *state);
TZrStackValuePointer native_binding_temp_root_slot(ZrLibTempValueRoot *root);

#endif // ZR_VM_LIBRARY_NATIVE_BINDING_INTERNAL_H
