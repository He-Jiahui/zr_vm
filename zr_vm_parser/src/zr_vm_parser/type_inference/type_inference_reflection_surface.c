#include "type_inference_reflection_surface.h"

const ZrLibCanonicalTypeRoleDescriptor *
ZrParser_ReflectionCompileSurface_Find(
        SZrGlobalState *global,
        const TZrChar *canonicalName) {
    ZrLibRegisteredCanonicalTypeRole registeredRole;

    return ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(
                   global, canonicalName, &registeredRole)
                   ? registeredRole.typeRole
                   : ZR_NULL;
}

const ZrLibCanonicalTypeRoleDescriptor *
ZrParser_ReflectionCompileSurface_FindByRole(
        SZrGlobalState *global,
        EZrCanonicalTypeRole role) {
    ZrLibRegisteredCanonicalTypeRole registeredRole;

    return ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
                   global, role, &registeredRole)
                   ? registeredRole.typeRole
                   : ZR_NULL;
}

static TZrBool reflection_compile_surface_projection_for_category(
        EZrReflectionTypeCategory category,
        EZrCanonicalTypeProjectionKind *outProjection) {
    if (outProjection == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (category) {
        case ZR_REFLECTION_TYPE_CATEGORY_CLASS:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_CLASS;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_CONCRETE_CLASS;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_INSTANCE_CLASS;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_STRUCT:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_STRUCT;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_INTERFACE:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_INTERFACE;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_RESOURCE_CLASS;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_REF_STRUCT;
            return ZR_TRUE;
        case ZR_REFLECTION_TYPE_CATEGORY_ENUM:
            *outProjection = ZR_CANONICAL_TYPE_PROJECTION_ENUM;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

const TZrChar *ZrParser_ReflectionCompileSurface_DescriptorName(
        SZrGlobalState *global,
        EZrReflectionTypeCategory category) {
    ZrLibRegisteredCanonicalTypeRole registeredRole;
    const ZrLibCanonicalTypeRoleDescriptor *typeRole;
    EZrCanonicalTypeProjectionKind projectionKind;

    if (category == ZR_REFLECTION_TYPE_CATEGORY_ERASED) {
        typeRole = ZrParser_ReflectionCompileSurface_FindByRole(
                global, ZR_CANONICAL_TYPE_ROLE_REFLECTION_TYPE);
        return typeRole != ZR_NULL ? typeRole->canonicalName : ZR_NULL;
    }
    if (!reflection_compile_surface_projection_for_category(
                category, &projectionKind)) {
        return ZR_NULL;
    }
    if (!ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByProjection(
                global,
                ZR_PROVIDER_CONTRACT_ROLE_REFLECTION,
                projectionKind,
                &registeredRole)) {
        return ZR_NULL;
    }

    return registeredRole.typeRole->canonicalName;
}
