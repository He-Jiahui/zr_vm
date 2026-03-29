//
// zr.system.vm descriptor registry.
//

#include "zr_vm_lib_system/vm_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_VmRegistry_GetModule(void) {
    static const ZrLibFieldDescriptor kVmStateFields[] = {
            {"loadedModuleCount", "int", "Number of currently loaded modules."},
            {"garbageCollectionMode", "int", "Current garbage-collection mode enum value."},
            {"garbageCollectionDebt", "int", "Outstanding garbage-collection debt."},
            {"garbageCollectionThreshold", "int", "Garbage-collection pause threshold."},
            {"stackDepth", "int", "Current VM stack depth."},
            {"frameDepth", "int", "Current VM call-frame depth."},
    };
    static const ZrLibFieldDescriptor kLoadedModuleInfoFields[] = {
            {"name", "string", "Resolved module name."},
            {"sourceKind", "string", "Module source kind such as native or source."},
            {"sourcePath", "string", "Module source path or native module identifier."},
            {"hasTypeHints", "bool", "Whether native type-hint metadata is available."},
    };
    static const ZrLibTypeDescriptor kTypes[] = {
            {"SystemVmState",
             ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
             kVmStateFields,
             ZR_ARRAY_COUNT(kVmStateFields),
             ZR_NULL,
             0,
             ZR_NULL,
             0,
             "Runtime VM state snapshot."},
            {"SystemLoadedModuleInfo",
             ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
             kLoadedModuleInfoFields,
             ZR_ARRAY_COUNT(kLoadedModuleInfoFields),
             ZR_NULL,
             0,
             ZR_NULL,
             0,
             "Loaded module metadata snapshot."},
    };
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"loadedModules", 0, 0, ZrSystem_Vm_LoadedModules, "SystemLoadedModuleInfo[]", "Return metadata for all loaded modules."},
            {"state", 0, 0, ZrSystem_Vm_State, "SystemVmState", "Return a snapshot of VM execution state."},
            {"callModuleExport", 3, 3, ZrSystem_Vm_CallModuleExport, "value", "Call an export from another module."},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"loadedModules", "function", "loadedModules(): SystemLoadedModuleInfo[]", "Return metadata for all loaded modules."},
            {"state", "function", "state(): SystemVmState", "Return a snapshot of VM execution state."},
            {"callModuleExport", "function", "callModuleExport(moduleName: string, exportName: string, args: array): any", "Call an export from another module."},
            {"SystemVmState", "type", "struct SystemVmState { loadedModuleCount, garbageCollectionMode, garbageCollectionDebt, garbageCollectionThreshold, stackDepth, frameDepth }", "Runtime VM state snapshot."},
            {"SystemLoadedModuleInfo", "type", "struct SystemLoadedModuleInfo { name, sourceKind, sourcePath, hasTypeHints }", "Loaded module metadata snapshot."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.vm\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.vm",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            kTypes,
            ZR_ARRAY_COUNT(kTypes),
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "VM inspection and module invocation helpers.",
            ZR_NULL,
            0,
    };

    return &kModule;
}
