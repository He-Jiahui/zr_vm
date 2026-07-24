#include "zr_vm_parser/iteration_contract.h"

#include "type_inference_internal.h"

TZrBool ZrParser_EnumeratorBinding_ResolveElementType(
        SZrCompilerState *compiler,
        const SZrInferredType *source,
        SZrInferredType *outElementType) {
    static const EZrProtocolId kEnumeratorProtocols[] = {
            ZR_PROTOCOL_ID_ITERATOR,
            ZR_PROTOCOL_ID_ITERABLE,
    };

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
