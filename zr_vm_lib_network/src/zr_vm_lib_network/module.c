#include "zr_vm_lib_network/module.h"

#include "zr_vm_lib_network/tcp_registry.h"
#include "zr_vm_lib_network/udp_registry.h"
#include "zr_vm_library/native_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const TZrChar g_network_root_type_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.network\"\n"
        "}\n";

static const ZrLibModuleLinkDescriptor g_network_module_links[] = {
        {"tcp", "zr.network.tcp", "TCP client and server primitives."},
        {"udp", "zr.network.udp", "UDP datagram primitives."},
};

static const ZrLibModuleDescriptor g_network_root_module_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.network",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        g_network_root_type_hints_json,
        "Network native module root that aggregates TCP and UDP leaf modules.",
        g_network_module_links,
        ZR_ARRAY_COUNT(g_network_module_links),
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        0,
};

const ZrLibModuleDescriptor *ZrVmLibNetwork_GetModuleDescriptor(void) {
    return &g_network_root_module_descriptor;
}

TZrBool ZrVmLibNetwork_Register(SZrGlobalState *global) {
    const ZrLibModuleDescriptor *leafModules[] = {
            ZrNetwork_TcpRegistry_GetModule(),
            ZrNetwork_UdpRegistry_GetModule(),
    };
    TZrSize index;

    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    for (index = 0; index < ZR_ARRAY_COUNT(leafModules); index++) {
        if (leafModules[index] == ZR_NULL ||
            !ZrLibrary_NativeRegistry_RegisterModule(global, leafModules[index])) {
            return ZR_FALSE;
        }
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_network_root_module_descriptor);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &g_network_root_module_descriptor;
}
#endif
