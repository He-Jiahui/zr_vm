// 归档 AOT 后端和动态加载器约定的模块边界；描述符布局必须与加载器和生成物同时演进。

#ifndef ZR_VM_COMMON_ZR_AOT_ABI_H
#define ZR_VM_COMMON_ZR_AOT_ABI_H

#include "zr_vm_common/zr_common_conf.h"

struct SZrState;

// BUG: 若将此归档头与归档 aot_runtime.c 一起重新接入构建，描述符缺少
// codeRegistration、methodInfos 等加载器在 aot_runtime_prepare_record 中访问的字段，
// 编译会直接失败；需先统一生成器、描述符版本和加载器布局。
#define ZR_VM_AOT_ABI_VERSION 2u

/** @brief 标识生成物所用后端，加载器据此选择库名并核对入口类型。 */
typedef enum EZrAotBackendKind {
    ZR_AOT_BACKEND_KIND_NONE = 0,
    ZR_AOT_BACKEND_KIND_C = 1,
    ZR_AOT_BACKEND_KIND_LLVM = 2
} EZrAotBackendKind;

/** @brief 告知加载器用于核对哈希的原始输入种类。 */
typedef enum EZrAotInputKind {
    ZR_AOT_INPUT_KIND_NONE = 0,
    ZR_AOT_INPUT_KIND_SOURCE = 1,
    ZR_AOT_INPUT_KIND_BINARY = 2
} EZrAotInputKind;

/** @brief 生成代码的入口签名；返回值与 VM 调用状态一同表示执行结果。 */
typedef TZrInt64 (*FZrAotEntryThunk)(struct SZrState *state);

/**
 * @brief 动态库导出的不可变模块描述符，供加载器绑定元数据与本机入口。
 * @note 名称、哈希、契约、内嵌 blob 和 thunk 表由动态库持有，库句柄卸载前必须保持有效。
 *       functionThunks 的索引应与加载器展开的函数图索引一致；entryThunk 是模块入口。
 */
typedef struct ZrAotCompiledModule {
    TZrUInt32 abiVersion;
    TZrUInt32 backendKind;
    const TZrChar *moduleName;
    TZrUInt32 inputKind;
    const TZrChar *inputHash;
    const TZrChar *const *runtimeContracts;
    const TZrByte *embeddedModuleBlob;
    TZrSize embeddedModuleBlobLength;
    const FZrAotEntryThunk *functionThunks;
    TZrUInt32 functionThunkCount;
    FZrAotEntryThunk entryThunk;
} ZrAotCompiledModule;

/** @brief 动态库导出符号的签名；返回库生命周期内稳定的描述符地址。 */
typedef const ZrAotCompiledModule *(*FZrVmGetAotCompiledModule)(void);

// 仅导出描述符入口，运行时 helper 的可见性由其各自的库 API 控制。
#if defined(ZR_PLATFORM_WIN)
#define ZR_VM_AOT_EXPORT __declspec(dllexport)
#else
#define ZR_VM_AOT_EXPORT __attribute__((visibility("default")))
#endif

#endif
