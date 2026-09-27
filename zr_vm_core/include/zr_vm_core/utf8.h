//
// Core UTF-8 helpers backed by utf8proc.
//

#ifndef ZR_VM_CORE_UTF8_H
#define ZR_VM_CORE_UTF8_H

#include "zr_vm_core/conf.h"

/**
 * @brief 在指定字节范围内验证 UTF-8，供字符串转字节数组、控制台和 LSP 文档入口拒绝坏输入。
 * @note 不以 NUL 结尾符截断；(ZR_NULL, 0) 表示有效空串。
 */
ZR_CORE_API TZrBool ZrCore_Utf8_IsValid(TZrNativeString string, TZrSize length);

/**
 * @brief 从当前字节边界解出一个码点，供控制台输入与 LSP 位置换算逐码点前进。
 * @pre length 必须覆盖完整首码点；两个输出指针可为空，失败时不写输出。
 */
ZR_CORE_API TZrBool ZrCore_Utf8_DecodeCodePoint(TZrNativeString string,
                                                TZrSize length,
                                                TZrUInt32 *outCodePoint,
                                                TZrSize *outConsumedBytes);

/**
 * @brief 为字符串格式化与字符追加生成一个码点的 UTF-8 字节。
 * @pre buffer 至少容纳 ZR_STRING_UTF8_SIZE 字节，outLength 非空；调用者应提供有效 Unicode 标量值。
 * @note 成功时 outLength 是不含可选结尾 NUL 的字节数。
 */
ZR_CORE_API TZrBool ZrCore_Utf8_EncodeCodePoint(TZrUInt32 codePoint,
                                                TZrChar *buffer,
                                                TZrSize *outLength);

/**
 * @brief 按码点计数字符串长度，供 String 长度与切片语义区别于字节长度。
 * @note 遇到无效 UTF-8 返回失败且不发布部分计数；(ZR_NULL, 0) 返回零。
 */
ZR_CORE_API TZrBool ZrCore_Utf8_CountCodePoints(TZrNativeString string,
                                                TZrSize length,
                                                TZrSize *outCount);

/**
 * @brief 将码点计数换成字节截取边界，供 String 截断保留完整 UTF-8 序列。
 * @note 仅验证扫描到目标边界的前缀，不检查未经过的后缀；请求超出时返回末尾，已扫描部分解码失败时不改输出。
 */
ZR_CORE_API TZrBool ZrCore_Utf8_CodePointCountToByteOffset(TZrNativeString string,
                                                           TZrSize length,
                                                           TZrSize codePointCount,
                                                           TZrSize *outOffset);

#endif // ZR_VM_CORE_UTF8_H
