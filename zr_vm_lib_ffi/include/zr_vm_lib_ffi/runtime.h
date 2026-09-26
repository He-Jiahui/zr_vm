//
// zr.ffi runtime helpers and native callbacks.
//

#ifndef ZR_VM_LIB_FFI_RUNTIME_H
#define ZR_VM_LIB_FFI_RUNTIME_H

#include "zr_vm_lib_ffi/conf.h"
#include "zr_vm_common/zr_ffi_contract.h"
#include "zr_vm_core/native_call_contract.h"

const ZrLibModuleDescriptor *ZrVmLibFfiRuntime_GetModuleDescriptor(void);

/** @brief 按脚本路径建立托管动态库句柄，供动态查符号和 source extern 共用。
 * @note 打开失败会报告 LoadError；成功句柄由 close 或 GC finalizer 释放。
 */
TZrBool ZrFfi_LoadLibrary(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 把 ZR closure 包成可传给 C 的 callback trampoline。
 * @pre 签名须匹配目标 ABI，调用须符合创建线程和激活期策略。
 * @note close 后外部 C 不得再调用已保存的函数指针。
 */
TZrBool ZrFfi_CreateCallback(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 查询可解析 FFI 类型在目标 ABI 中的字节尺寸。 */
TZrBool ZrFfi_SizeOf(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 查询可解析 FFI 类型在目标 ABI 中的对齐值。 */
TZrBool ZrFfi_AlignOf(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 创建保留目标类型信息的空 pointer；结果不可解引用。 */
TZrBool ZrFfi_NullPointer(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 逻辑关闭库，禁止新查找和 symbol 调用。
 * @note 现有 SymbolHandle 持有库 owner，实际卸载可推迟到其 finalizer。
 */
TZrBool ZrFfi_Library_Close(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 查询逻辑关闭状态；true 不保证 OS 已卸载动态库。 */
TZrBool ZrFfi_Library_IsClosed(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 用调用方提供的 ABI 签名创建持库的 SymbolHandle。
 * @pre 签名必须与 C 导出一致，库尚未请求关闭。
 */
TZrBool ZrFfi_Library_GetSymbol(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 按活动调用帧的 retained native-import contract 索引解析符号。
 * @pre 参数是非负整数索引，所指 contract 须属于活动 caller 且库定位符匹配。
 * @note 此入口由 source extern 编译器生成调用；参数不是 contract 对象。
 */
TZrBool ZrFfi_Library_GetContractSymbol(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 调用可选的零参版本导出，导出不存在时返回 null。
 * @pre 指定符号必须具有 const char *(*)(void) ABI，返回有效 NUL 结尾字符串。
 * @note 此便捷入口不执行签名编译，调用方负责保证导出类型。
 */
TZrBool ZrFfi_Library_GetVersion(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 验证 retained contract 能否在当前 ABI 形成可调用签名。
 * @pre errorBufferSize 非零时，errorBuffer 指向相应可写空间；调用方需提供容量来接收错误文本。
 * @note 错误写入调用方自有的 errorBuffer；不检查动态库导出，也不代替 parser 的源码诊断。
 */
ZR_VM_LIB_FFI_API TZrBool ZrVmLibFfi_ValidateNativeImportContract(
        const SZrNativeImportContract *contract,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
/** @brief 交由核心 native-call 规划器预检 contract 与 callbackId。
 * @pre plan 指向可写的调用计划空间；调用方负责管理后续 native pin lease 的生命周期。
 * @note plan 和 diagnostic 由调用方持有；此接口不直接执行 native symbol。
 */
ZR_VM_LIB_FFI_API TZrBool ZrVmLibFfi_PrepareNativeCallPlan(
        const SZrNativeImportContract *contract,
        TZrUInt64 callbackId,
        SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic);

/** @brief 以数组实参执行已编译签名的 native symbol。
 * @pre 数组项须满足 ABI 类型和参数个数；所属库仍处于逻辑打开状态。
 */
TZrBool ZrFfi_Symbol_Call(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 让 SymbolHandle 支持 symbol(a, b) 位置参数语法，共用数组调用路径。 */
TZrBool ZrFfi_Symbol_MetaCall(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 停用 callback；closure 物理内存由 finalizer 最终释放。 */
TZrBool ZrFfi_Callback_Close(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 在同一地址和 owner 上建立新类型视图，不复制或转换内存。
 * @pre 目标类型须与实际 ABI 布局兼容；新视图应独立 close。
 */
TZrBool ZrFfi_Pointer_As(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 按指定 FFI 类型从 native 地址读取值。
 * @pre 地址有效、对齐正确且至少容纳目标类型大小；此操作可能直接读取原生内存。
 */
TZrBool ZrFfi_Pointer_Read(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 关闭当前 pointer 视图并释放它持有的一次 buffer pin。 */
TZrBool ZrFfi_Pointer_Close(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将显式 pinned 地址借出为 zr.container Span；Span 依赖原 pointer 的有效期。 */
TZrBool ZrFfi_Pointer_Span(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 读取 pinned 范围内的一个字节；下标单位为 byte。 */
TZrBool ZrFfi_Pointer_GetItem(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 向 pinned 范围写一个字节，值必须位于 0..255。 */
TZrBool ZrFfi_Pointer_SetItem(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 建立零初始化的 owned native byte buffer。 */
TZrBool ZrFfi_Buffer_Allocate(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 关闭 buffer；已有 pointer pin 释放后才回收底层字节。 */
TZrBool ZrFfi_Buffer_Close(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 借出持有 BufferHandle owner 的 pointer view；每个 view 独立持有一次 pin。 */
TZrBool ZrFfi_Buffer_Pin(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 把指定字节范围复制为 ZR 数组；结果不借用原 buffer。 */
TZrBool ZrFfi_Buffer_Read(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 把脚本字节数组写入 owned native buffer。
 * @pre offset 和长度在范围内，元素应是 0..255 的整数。
 */
TZrBool ZrFfi_Buffer_Write(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 复制指定范围为独立 owned BufferHandle，不延长原 buffer 的 pin。 */
TZrBool ZrFfi_Buffer_Slice(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_FFI_RUNTIME_H
