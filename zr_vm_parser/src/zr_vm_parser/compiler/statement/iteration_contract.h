#ifndef ZR_VM_PARSER_ITERATION_CONTRACT_H
#define ZR_VM_PARSER_ITERATION_CONTRACT_H

#include "zr_vm_parser/compiler.h"

/**
 * @brief 为 foreach 编译与赋值分析取得同一协议投影下的元素类型。
 * @pre compiler 与 source 有效；outElementType 是已初始化、尚未持有嵌套类型资源的独立结果对象。
 * @return 找到 Iterator<T> 或 Iterable<T> 的规范协议实参时返回真；未找到或参数无效时返回假。
 * @note 调用方在成功后负责释放结果。普通数组的元素参数不能代替迭代协议，
 *       因而缺少协议的来源类型会保留给调用方自己的回退路径。
 */
ZR_PARSER_API TZrBool ZrParser_EnumeratorBinding_ResolveElementType(
        SZrCompilerState *compiler,
        const SZrInferredType *source,
        SZrInferredType *outElementType);

#endif /* ZR_VM_PARSER_ITERATION_CONTRACT_H */
