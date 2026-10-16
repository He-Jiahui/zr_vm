#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H

#include <stddef.h>

#include "zr_vm_core/aot_ir.h"

/**
 * @brief 将受限无参数 i64 函数写成独立 C11 源码。
 * @pre module 及其数组视图在调用期间有效；接受显式 NOARGS_I64 ABI。
 *      单常量分支支持单块 constant/return、入口 branch 到 constant/return 块，
 *      或真实源码单入口块 constant/plain nop/return 三种配置。前两者无 primitive
 *      行、frame slot、value slot 和 logical state map；源码配置仅接受一个
 *      返回类型一致的宿主 8 字节/8 对齐 primitive 行、一个匹配返回 SSA 的
 *      VALUE frame slot/value slot、8 字节 frame 和完整但内容为空的状态表。
 *      源码 source map 和调试身份可保留；其余运行时、恢复、能力语义不受支持。
 *      源码 NOP 保留与返回 ABI 一致的类型标记，且不承载操作数、效果或绑定。
 *      既有双常量算术及四常量条件配置由各自受限校验分支处理。
 * @pre output 指向 capacity 字节的可写存储，capacity 包含结尾 NUL；
 *      outLength 非空。输入视图和各输出存储不应互相覆盖。
 * @return 成功返回 OK，outLength 为不含 NUL 的文本字节数；输入校验失败、
 *         不支持的语义形状或容量不足返回相应状态。Core 校验错误原样传播；
 *         源码 primitive/frame 跨记录矛盾返回 INVALID_LAYOUT 及实际字段诊断。
 * @note 缓冲区由调用方所有；失败时能写入的首字节清零、能写入的长度归零。
 *       diagnostic 可为空；本接口不分配、不保存视图，也不编译或注册可加载产物。
 */
EZrAotIrStatus backend_aot_ir_c_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

/**
 * @brief 将同一受限无参数 i64 函数写成独立 LLVM IR 文本。
 * @pre module 和输出存储遵守 C 文本接口的有效期、互不覆盖及容量约束；
 *      接受显式 NOARGS_I64 ABI、单常量三种配置及既有受限算术/条件配置。
 * @return 成功返回 OK，outLength 不含结尾 NUL；失败返回输入校验、不支持
 *         或容量错误，并在可写时清空首字节和长度。
 * @note diagnostic 可为空；返回的只是文本，不编译或注册可加载 ZR 产物。
 */
EZrAotIrStatus backend_aot_ir_llvm_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H */
