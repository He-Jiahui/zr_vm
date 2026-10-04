#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H

#include <stddef.h>

#include "zr_vm_core/aot_ir.h"

/**
 * @brief 将一个无参数、返回 i64 常量的函数写成独立 C11 源码。
 * @pre module 及其数组视图在调用期间有效；仅接受显式 NOARGS_I64 ABI，
 *      以及单块 constant/return 或入口 branch 到 constant/return 块的形状。
 * @pre output 指向 capacity 字节的可写存储，capacity 包含结尾 NUL；
 *      outLength 非空。输入视图和各输出存储不应互相覆盖。
 * @return 成功返回 OK，outLength 为不含 NUL 的文本字节数；输入校验失败、
 *         不支持的语义形状或容量不足返回相应状态。
 * @note 缓冲区由调用方所有；失败时能写入的首字节清零、能写入的长度归零。
 *       diagnostic 可为空；本接口不分配、不保存视图，也不编译或注册可加载产物。
 */
EZrAotIrStatus backend_aot_ir_c_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

/**
 * @brief 将同一受限 i64 常量函数写成独立 LLVM IR 文本。
 * @pre module 和输出存储遵守 C 文本接口的有效期、互不覆盖及容量约束；
 *      仅接受显式 NOARGS_I64 ABI 和该接口支持的两种 CFG 形状。
 * @return 成功返回 OK，outLength 不含结尾 NUL；失败返回输入校验、不支持
 *         或容量错误，并在可写时清空首字节和长度。
 * @note diagnostic 可为空；返回的只是文本，不编译或注册可加载 ZR 产物。
 */
EZrAotIrStatus backend_aot_ir_llvm_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H */
