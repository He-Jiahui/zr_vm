#ifndef ZR_VM_PARSER_COMPILER_METADATA_SIGNATURE_H
#define ZR_VM_PARSER_COMPILER_METADATA_SIGNATURE_H

#include "compiler_internal.h"

/**
 * @brief 在稳定 v1 域内哈希完整 metadata 签名字节。
 * @pre 输入指向本次签名的完整非空序列化范围。
 * @return 稳定哈希；缺失或空输入返回 0。
 * @note token、module record、type reference、TypeDef 与 TypeSpec 共用此身份规则。
 */
TZrUInt64 metadata_signature_hash_v1(const TZrByte *signatureBlob, TZrSize signatureBlobLength);

/**
 * @brief 返回字符串对象可见原生文本的长度。
 * @return 空对象或无法取得文本时返回 0；按 C 文本在首个 NUL 处结束。
 */
TZrSize metadata_token_string_length(SZrString *stringValue);

/**
 * @brief 将本地声明的 union 类型名拆为基类型名和泛型实参。
 * @pre 输出指针非空，cs 指向有效脚本编译上下文。
 * @return 成功时把实参数组交给调用方并由 cs->state 释放；失败时不得读取输出。
 * @note 查找递归穿过 extern block，但不会解析外部模块声明。
 */
TZrBool metadata_token_try_resolve_union_signature_type(SZrCompilerState *cs,
                                                        SZrString *typeName,
                                                        SZrString **outBaseName,
                                                        SZrArray *outArgumentTypeNames);

/**
 * @brief 在本次写出所用的字符串堆快照中查找字符串键。
 * @pre entries 与本次签名写出前完成的字符串收集快照相同。
 * @return 零表示缺席；写出前须先收集所有引用字符串。
 */
TZrUInt32 metadata_token_string_heap_index(const SZrMetadataStringHeapEntry *entries,
                                           TZrUInt32 entryCount,
                                           SZrString *value);

/**
 * @brief 向共享 metadata 输出缓冲区追加单字节字段。
 * @pre buffer 和 offset 有效，且调用方已保证当前位置有一个可写字节。
 */
void metadata_token_write_u8(TZrByte *buffer, TZrSize *offset, TZrUInt8 value);

/**
 * @brief 向共享 metadata 输出缓冲区追加小端序 u32 字段。
 * @pre buffer 和 offset 有效，且调用方已保证当前位置至少剩余四字节。
 */
void metadata_token_write_u32(TZrByte *buffer, TZrSize *offset, TZrUInt32 value);

/**
 * @brief 计算一个类型树的 metadata 编码长度，供分配前规划使用。
 * @pre cs 和 typeRef 须与随后传给 writer 的输入一致。
 * @note 调用方须在分配前检查总堆上限。TODO: 多层 TZrSize 累加未检查溢出，依赖调用方约束。
 */
TZrSize metadata_token_type_ref_signature_size(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef);

/**
 * @brief 按共享 metadata signature 格式写出一个类型树。
 * @pre 目标容量覆盖配对 size 结果，字符串堆与规划快照一致；输入在同步写出期间保持有效。
 */
void metadata_token_write_type_ref_signature(TZrByte *buffer,
                                             TZrSize *offset,
                                             SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *typeRef,
                                             const SZrMetadataStringHeapEntry *stringHeapEntries,
                                             TZrUInt32 stringHeapEntryCount);

/**
 * @brief 按导出符号种类计算方法或字段签名长度。
 * @pre 计长与后续写出使用同一符号及其类型数组快照。
 * @return 空符号返回 0；否则返回对应签名 frame 和类型树的编码长度。
 */
TZrSize metadata_token_symbol_signature_size(SZrCompilerState *cs, const SZrFunctionTypedExportSymbol *symbol);

/**
 * @brief 按计长阶段相同的符号种类写出方法或字段签名。
 * @pre 输出容量覆盖配对 size 结果；符号及字符串堆快照在同步写出期间保持有效。
 */
void metadata_token_write_symbol_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedExportSymbol *symbol,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount);

/**
 * @brief 计算导出方法及导入 effect 共用的方法 frame 长度。
 * @pre 计长与写出阶段使用相同的返回类型、泛型数、参数数量和参数数组；空参数数组按 object 占位计长。
 * @note 当前 frame 为 arity 固定预留一个字节；该参数不改变总长度。
 * TODO: 参数长度累加未检查溢出；调用方须保证 metadata heap 长度可表示。
 */
TZrSize metadata_token_method_signature_size(SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *returnType,
                                             TZrUInt32 genericParameterCount,
                                             TZrUInt32 parameterCount,
                                             const SZrFunctionTypedTypeRef *parameterTypes);

/**
 * @brief 写出方法 frame、返回类型及按顺序排列的参数类型树。
 * @pre 目标容量覆盖配对 size 结果；参数与堆快照在同步写出期间有效。
 * BUG: zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c:1144/1148 将 declaration->generic 传给 builder，zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c:1039/1059/1060 再复制到导出符号；zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c:146/147/151-155 检查非空声明、收集泛型参数并转交 infos；zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c:61/62/69 校验实参数组非空并写入 genericParameterCount。仅 genericParameterCount > 0 时错位可观察：zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c:1616 经 zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c:623 进入 writer，arity 写入 reader 的 flags u8 槽、后续 u32 写 0；core reader zr_vm_core/src/zr_vm_core/metadata_runtime.c:887/892/896 分开读取节点、flags 和元数，故 flags 被污染且 reader 元数归零；零元数时无可观察影响。
 * TODO: arity 超过 255 时饱和为 0xff；核实上游限制或定义扩展编码。
 */
void metadata_token_write_method_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedTypeRef *returnType,
                                           TZrUInt32 genericParameterCount,
                                           TZrUInt32 parameterCount,
                                           const SZrFunctionTypedTypeRef *parameterTypes,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount);

/**
 * @brief 计算字段或属性签名 frame 与值类型树的编码长度。
 * @pre valueType 与随后字段 writer 使用同一类型引用及编译上下文。
 */
TZrSize metadata_token_field_signature_size(SZrCompilerState *cs,
                                            const SZrFunctionTypedTypeRef *valueType);

/**
 * @brief 写出 metadata reader 使用的字段 frame 和值类型树。
 * @pre 输出容量覆盖配对 size 结果，且值类型和字符串堆快照与规划阶段一致。
 */
void metadata_token_write_field_signature(TZrByte *buffer,
                                          TZrSize *offset,
                                          SZrCompilerState *cs,
                                          const SZrFunctionTypedTypeRef *valueType,
                                          const SZrMetadataStringHeapEntry *stringHeapEntries,
                                          TZrUInt32 stringHeapEntryCount);

/** @brief 将固定宽度字符串堆键写入共享 metadata 输出。
 * @pre 非空文本值须已收集到本次写出使用的同一堆快照；空值或缺席可不入堆并编码为 0。
 * @note 缺失值编码为零，表示字符串引用缺席。
 */
void metadata_token_write_string_ref(TZrByte *buffer,
                                     TZrSize *offset,
                                     SZrString *value,
                                     const SZrMetadataStringHeapEntry *stringHeapEntries,
                                     TZrUInt32 stringHeapEntryCount);

#endif
