//
// Built-in zr.system native module registration.
//

#include "zr_vm_lib_system/module.h"

#include "zr_vm_lib_system/console_registry.h"
#include "zr_vm_lib_system/env_registry.h"
#include "zr_vm_lib_system/exception_registry.h"
#include "zr_vm_lib_system/fs_registry.h"
#include "zr_vm_lib_system/gc_registry.h"
#include "zr_vm_lib_system/process_registry.h"
#include "zr_vm_lib_system/vm_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const TZrChar g_system_root_type_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.system\"\n"
        "}\n";

static const ZrLibModuleLinkDescriptor g_system_module_links[] = {
        {"console", "zr.system.console", "Console output helpers."},
        {"fs", "zr.system.fs", "Filesystem helpers."},
        {"env", "zr.system.env", "Environment helpers."},
        {"process", "zr.system.process", "Process helpers."},
        {"gc", "zr.system.gc", "Garbage-collection controls."},
        {"exception", "zr.system.exception", "Exception hierarchy and global unhandled hooks."},
        {"vm", "zr.system.vm", "VM inspection and module invocation helpers."},
};

static const ZrLibModuleDescriptor g_system_root_module_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.system",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        g_system_root_type_hints_json,
        "System native module root that aggregates leaf submodules.",
        g_system_module_links,
        ZR_ARRAY_COUNT(g_system_module_links),
};

const ZrLibModuleDescriptor *ZrVmLibSystem_GetModuleDescriptor(void) {
    return &g_system_root_module_descriptor;
}

TZrBool ZrVmLibSystem_Register(SZrGlobalState *global) {
    const ZrLibModuleDescriptor *leafModules[] = {
            ZrSystem_ConsoleRegistry_GetModule(),
            ZrSystem_FsRegistry_GetModule(),
            ZrSystem_EnvRegistry_GetModule(),
            ZrSystem_ProcessRegistry_GetModule(),
            ZrSystem_GcRegistry_GetModule(),
            ZrSystem_ExceptionRegistry_GetModule(),
            ZrSystem_VmRegistry_GetModule(),
    };
    TZrSize index;

    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < ZR_ARRAY_COUNT(leafModules); index++) {
        if (leafModules[index] == ZR_NULL ||
            !ZrLibrary_NativeRegistry_RegisterModule(global, leafModules[index])) {
            return ZR_FALSE;
        }
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_system_root_module_descriptor);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &g_system_root_module_descriptor;
}
#endif
