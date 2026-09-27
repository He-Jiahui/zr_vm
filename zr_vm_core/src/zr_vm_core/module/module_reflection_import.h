#ifndef ZR_VM_CORE_MODULE_REFLECTION_IMPORT_H
#define ZR_VM_CORE_MODULE_REFLECTION_IMPORT_H

#include "zr_vm_core/conf.h"

struct SZrFunction;
struct SZrObjectModule;
struct SZrState;
struct SZrString;

/* 命中反射 provider 路径返回 true；outModule 可仍为空，表示该调用图没有可用运行时。 */
TZrBool zr_module_reflection_import_try_resolve(
        struct SZrState *state,
        struct SZrString *path,
        struct SZrFunction *callerFunction,
        struct SZrObjectModule **outModule);

#endif
