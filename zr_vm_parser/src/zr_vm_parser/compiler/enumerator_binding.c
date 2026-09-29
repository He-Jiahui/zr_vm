#include "zr_vm_parser/iteration_contract.h"

#include "type_inference_internal.h"

TZrBool ZrParser_EnumeratorBinding_ResolveElementType(
        SZrCompilerState *compiler,
        const SZrInferredType *source,
        SZrInferredType *outElementType) {
    /* foreach 编译和流敏感赋值分析共用此顺序，避免两处从不同协议推断同一绑定。 */
    static const EZrProtocolId kEnumeratorProtocols[] = {
            ZR_PROTOCOL_ID_ITERATOR,
            ZR_PROTOCOL_ID_ITERABLE,
    };

    /* TODO: 核对同时声明 Iterator<T> 与 Iterable<U> 的类型是否由语义层保证 T == U；
     * 当前按 Iterator 优先返回，协议注册和 foreach 测试尚未覆盖冲突实参。 */

    if (compiler == ZR_NULL || source == ZR_NULL || outElementType == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kEnumeratorProtocols); ++index) {
        if (ZrParser_TypeInference_BindProtocolElementType(
                    compiler, source, kEnumeratorProtocols[index], outElementType)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}
