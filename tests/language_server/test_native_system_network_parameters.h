#ifndef TEST_NATIVE_SYSTEM_NETWORK_PARAMETERS_H
#define TEST_NATIVE_SYSTEM_NETWORK_PARAMETERS_H

#if defined(ZR_TEST_NATIVE_NETWORK)
#include "zr_vm_lib_network/tcp_registry.h"
#include "zr_vm_lib_network/udp_registry.h"
#endif
#include "zr_vm_lib_system/console_registry.h"
#include "zr_vm_lib_system/env_registry.h"
#include "zr_vm_lib_system/exception_registry.h"
#include "zr_vm_lib_system/process_registry.h"
#include "zr_vm_lib_system/vm_registry.h"

/* Check every finite argument-bearing row exposed by a system/network module. */
static void test_system_network_module_parameters(const ZrLibModuleDescriptor *module) {
    for (TZrSize index = 0; index < module->functionCount; index++) {
        const ZrLibFunctionDescriptor *function = &module->functions[index];
        if (function->maxArgumentCount != 0 && function->maxArgumentCount != 65535u) {
            check_parameters(module->moduleName, function->name, function->parameters,
                             function->parameterCount, function->maxArgumentCount);
        }
    }
    for (TZrSize index = 0; index < module->typeCount; index++) {
        const ZrLibTypeDescriptor *type = &module->types[index];
        for (TZrSize methodIndex = 0; methodIndex < type->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *method = &type->methods[methodIndex];
            if (method->maxArgumentCount != 0 && method->maxArgumentCount != 65535u) {
                check_parameters(type->name, method->name, method->parameters,
                                 method->parameterCount, method->maxArgumentCount);
            }
        }
        for (TZrSize metaIndex = 0; metaIndex < type->metaMethodCount; metaIndex++) {
            const ZrLibMetaMethodDescriptor *meta = &type->metaMethods[metaIndex];
            if (meta->maxArgumentCount != 0 && meta->maxArgumentCount != 65535u) {
                check_parameters(type->name, "meta", meta->parameters,
                                 meta->parameterCount, meta->maxArgumentCount);
            }
        }
    }
}

static void test_system_network_parameter_registration(SZrState *state) {
#if defined(ZR_TEST_NATIVE_NETWORK)
    const ZrLibModuleDescriptor *tcp = ZrNetwork_TcpRegistry_GetModule();
    const ZrLibModuleDescriptor *udp = ZrNetwork_UdpRegistry_GetModule();
#endif
    const ZrLibModuleDescriptor *console = ZrSystem_ConsoleRegistry_GetModule();
    const ZrLibModuleDescriptor *env = ZrSystem_EnvRegistry_GetModule();
    const ZrLibModuleDescriptor *exception = ZrSystem_ExceptionRegistry_GetModule();
    const ZrLibModuleDescriptor *process = ZrSystem_ProcessRegistry_GetModule();
    const ZrLibModuleDescriptor *vm = ZrSystem_VmRegistry_GetModule();
    const TZrChar *text;

    check(console != ZR_NULL && env != ZR_NULL && exception != ZR_NULL && process != ZR_NULL && vm != ZR_NULL
#if defined(ZR_TEST_NATIVE_NETWORK)
                  && tcp != ZR_NULL && udp != ZR_NULL
#endif
                  ,
          "system/network descriptor getters return modules");
    if (console == ZR_NULL || env == ZR_NULL || exception == ZR_NULL || process == ZR_NULL || vm == ZR_NULL
#if defined(ZR_TEST_NATIVE_NETWORK)
        || tcp == ZR_NULL || udp == ZR_NULL
#endif
    ) return;

#if defined(ZR_TEST_NATIVE_NETWORK)
    test_system_network_module_parameters(tcp);
    test_system_network_module_parameters(udp);
#endif
    test_system_network_module_parameters(console);
    test_system_network_module_parameters(env);
    test_system_network_module_parameters(exception);
    test_system_network_module_parameters(process);
    test_system_network_module_parameters(vm);

#if defined(ZR_TEST_NATIVE_NETWORK)
    text = render(state, tcp);
    check(strstr(text, "connect(host: string, port: int, timeoutMs: int /* optional */): TcpStream;") != ZR_NULL,
          "TCP connect renders an optional timeout with its name and type");
    check(strstr(text, "read(maxBytes: int, timeoutMs: int): string;") != ZR_NULL,
          "TCP read renders structured parameter names and types");
    check(strstr(text, "write(text: string): int;") != ZR_NULL,
          "TCP write renders structured parameter names and types");
    text = render(state, udp);
    check(strstr(text, "send(host: string, port: int, payload: string): int;") != ZR_NULL,
          "UDP send renders structured parameter names and types");
#endif
    text = render(state, console);
    check(strstr(text, "print(value: any): null;") != ZR_NULL,
          "console print renders its value parameter");
    text = render(state, env);
    check(strstr(text, "getVariable(name: string): string;") != ZR_NULL,
          "environment lookup renders its name parameter");
}

#endif
