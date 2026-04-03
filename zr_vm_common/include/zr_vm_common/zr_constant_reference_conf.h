//
// Shared constant-reference path step tags used by parser and runtime.
//

#ifndef ZR_CONSTANT_REFERENCE_CONF_H
#define ZR_CONSTANT_REFERENCE_CONF_H

#include "zr_vm_common/zr_common_conf.h"

typedef enum EZrConstantReferenceStepType {
    ZR_CONSTANT_REF_STEP_PARENT = -1,
    ZR_CONSTANT_REF_STEP_CHILD = 0,
    ZR_CONSTANT_REF_STEP_CONSTANT_POOL = -2,
    ZR_CONSTANT_REF_STEP_MODULE = -3,
    ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX = -4,
    ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX = -5,
} EZrConstantReferenceStepType;

#define ZR_CONSTANT_REF_STEP_TO_UINT32(step) ((TZrUInt32)(TZrInt32)(step))
#define ZR_CONSTANT_REF_STEP_FROM_UINT32(step) ((TZrInt32)(TZrUInt32)(step))

#endif // ZR_CONSTANT_REFERENCE_CONF_H
