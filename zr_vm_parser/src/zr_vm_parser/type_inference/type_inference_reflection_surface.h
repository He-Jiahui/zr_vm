#ifndef ZR_VM_PARSER_TYPE_INFERENCE_REFLECTION_SURFACE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_REFLECTION_SURFACE_H

#include "zr_vm_core/global.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_library/native_registry.h"

const ZrLibCanonicalTypeRoleDescriptor *
ZrParser_ReflectionCompileSurface_Find(
        SZrGlobalState *global,
        const TZrChar *canonicalName);

const ZrLibCanonicalTypeRoleDescriptor *
ZrParser_ReflectionCompileSurface_FindByRole(
        SZrGlobalState *global,
        EZrCanonicalTypeRole role);

const TZrChar *ZrParser_ReflectionCompileSurface_DescriptorName(
        SZrGlobalState *global,
        EZrReflectionTypeCategory category);

#endif
