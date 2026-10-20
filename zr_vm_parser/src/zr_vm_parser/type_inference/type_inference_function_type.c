#include "type_inference_function_type.h"
#include "type_inference_internal.h"

#include "zr_vm_common/zr_parser_conf.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

#include <string.h>

static const TZrChar *function_type_passing_mode_prefix(EZrParameterPassingMode passingMode) {
    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN: return "in ";
        case ZR_PARAMETER_PASSING_MODE_OUT: return "out ";
        case ZR_PARAMETER_PASSING_MODE_REF: return "ref ";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return "";
    }
}

static TZrBool function_type_name_append(TZrChar *buffer,
                                         TZrSize bufferSize,
                                         TZrSize *writeIndex,
                                         const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || writeIndex == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (*writeIndex + length >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *writeIndex, text, length);
    *writeIndex += length;
    buffer[*writeIndex] = '\0';
    return ZR_TRUE;
}

SZrString *type_inference_build_callable_type_name_from_signature(SZrState *state,
                                                          const SZrInferredType *returnType,
                                                          const SZrArray *paramTypes,
                                                          const SZrArray *parameterPassingModes) {
    TZrChar buffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];
    TZrSize writeIndex = 0;
    TZrChar nestedBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];

    if (state == ZR_NULL || returnType == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!function_type_name_append(buffer, sizeof(buffer), &writeIndex, "fn(")) {
        return ZR_NULL;
    }

    if (paramTypes != ZR_NULL) {
        for (TZrSize index = 0; index < paramTypes->length; index++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)paramTypes, index);
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
            EZrParameterPassingMode *modePtr = parameterPassingModes != ZR_NULL
                                                       ? (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes,
                                                                                                     index)
                                                       : ZR_NULL;
            const TZrChar *paramText;

            if (index > 0 && !function_type_name_append(buffer, sizeof(buffer), &writeIndex, ", ")) {
                return ZR_NULL;
            }

            if (modePtr != ZR_NULL) {
                passingMode = *modePtr;
            }

            if (!function_type_name_append(buffer,
                                           sizeof(buffer),
                                           &writeIndex,
                                           function_type_passing_mode_prefix(passingMode))) {
                return ZR_NULL;
            }

            paramText = ZrParser_TypeNameString_Get(state,
                                                    paramType,
                                                    nestedBuffer,
                                                    sizeof(nestedBuffer));
            if (!function_type_name_append(buffer,
                                           sizeof(buffer),
                                           &writeIndex,
                                           paramText != ZR_NULL ? paramText : "unknown")) {
                return ZR_NULL;
            }
        }
    }

    if (!function_type_name_append(buffer, sizeof(buffer), &writeIndex, ") -> ")) {
        return ZR_NULL;
    }

    {
        const TZrChar *returnText =
                ZrParser_TypeNameString_Get(state, returnType, nestedBuffer, sizeof(nestedBuffer));
        if (!function_type_name_append(buffer,
                                       sizeof(buffer),
                                       &writeIndex,
                                       returnText != ZR_NULL ? returnText : "unknown")) {
            return ZR_NULL;
        }
    }

    return ZrCore_String_Create(state, buffer, writeIndex);
}

static void function_type_signature_release(SZrCompilerState *cs,
                                            SZrArray *paramTypes,
                                            SZrArray *parameterPassingModes,
                                            SZrInferredType *returnType) {
    free_inferred_type_array(cs->state, paramTypes);
    if (parameterPassingModes->isValid) {
        ZrCore_Array_Free(cs->state, parameterPassingModes);
    }
    ZrParser_InferredType_Free(cs->state, returnType);
    ZrCore_Array_Construct(paramTypes);
    ZrCore_Array_Construct(parameterPassingModes);
    ZrParser_InferredType_Init(cs->state, returnType, ZR_VALUE_TYPE_OBJECT);
}

TZrBool type_inference_infer_function_type_signature(SZrCompilerState *cs,
                                                  const SZrFunctionType *funcType,
                                                  SZrArray *paramTypes,
                                                  SZrArray *parameterPassingModes,
                                                  SZrInferredType *returnType) {
    if (cs == ZR_NULL || funcType == ZR_NULL || paramTypes == ZR_NULL ||
        parameterPassingModes == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(paramTypes);
    ZrCore_Array_Construct(parameterPassingModes);
    ZrParser_InferredType_Init(cs->state, returnType, ZR_VALUE_TYPE_OBJECT);

    ZrCore_Array_Init(cs->state,
                      paramTypes,
                      sizeof(SZrInferredType),
                      funcType->params != ZR_NULL ? funcType->params->count : 0);
    ZrCore_Array_Init(cs->state,
                      parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      funcType->params != ZR_NULL ? funcType->params->count : 0);

    if (funcType->params != ZR_NULL) {
        for (TZrSize index = 0; index < funcType->params->count; index++) {
            SZrAstNode *paramNode = funcType->params->nodes[index];
            SZrInferredType paramType;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                passingMode = param->passingMode;
                if (param->typeInfo != ZR_NULL &&
                    !ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                    ZrParser_InferredType_Free(cs->state, &paramType);
                    function_type_signature_release(cs, paramTypes,
                                                    parameterPassingModes, returnType);
                    return ZR_FALSE;
                }
            }

            ZrCore_Array_Push(cs->state, paramTypes, &paramType);
            ZrCore_Array_Push(cs->state, parameterPassingModes, &passingMode);
        }
    }

    if (funcType->returnType != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(cs, funcType->returnType, returnType)) {
        function_type_signature_release(cs, paramTypes, parameterPassingModes, returnType);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
