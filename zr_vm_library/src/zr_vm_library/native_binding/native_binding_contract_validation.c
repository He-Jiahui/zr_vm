#include "native_binding_internal.h"

/* 每个 canonical type role 只能由对应 provider 契约发布，防止跨模块抢占反射/内建类型身份。 */
static EZrProviderContractRole native_registry_provider_role_for_canonical_type_role(
        EZrCanonicalTypeRole role) {
    if (role >= ZR_CANONICAL_TYPE_ROLE_BUILTIN_METADATA_ROOT &&
        role <= ZR_CANONICAL_TYPE_ROLE_BUILTIN_METADATA_OBJECT) {
        return ZR_PROVIDER_CONTRACT_ROLE_BUILTIN_TYPE_SURFACE;
    }
    if (role >= ZR_CANONICAL_TYPE_ROLE_REFLECTION_TYPE &&
        role <= ZR_CANONICAL_TYPE_ROLE_REFLECTION_ENUM_TYPE_OF) {
        return ZR_PROVIDER_CONTRACT_ROLE_REFLECTION;
    }
    return ZR_PROVIDER_CONTRACT_ROLE_NONE;
}

/* parentRole 仅能引用同一 descriptor 内的角色，不能依赖另一个尚未加载的 provider。 */
static const ZrLibCanonicalTypeRoleDescriptor *native_registry_find_local_type_role(
        const ZrLibModuleDescriptor *descriptor,
        EZrCanonicalTypeRole role) {
    if (descriptor == ZR_NULL || role == ZR_CANONICAL_TYPE_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < descriptor->canonicalTypeRoleCount; index++) {
        if (descriptor->canonicalTypeRoles[index].role == role) {
            return &descriptor->canonicalTypeRoles[index];
        }
    }
    return ZR_NULL;
}

/* 注册前验证 provider 的 canonical 投影集合与父级图；后续注册/反射按此表建立稳定类型身份。 */
TZrBool native_registry_validate_canonical_type_roles(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor) {
    const TZrUInt32 knownSurfaceFlags =
            ZR_CANONICAL_TYPE_SURFACE_METADATA_MEMBERS |
            ZR_CANONICAL_TYPE_SURFACE_CALLABLE_MEMBERS |
            ZR_CANONICAL_TYPE_SURFACE_RUNTIME_TYPE_MEMBERS |
            ZR_CANONICAL_TYPE_SURFACE_REPRESENTED_TYPE_ID |
            ZR_CANONICAL_TYPE_SURFACE_CONSTRUCTIBLE;
    TZrBool seenProjection[ZR_CANONICAL_TYPE_PROJECTION_ENUM + 1u] = {ZR_FALSE};

    if ((descriptor->canonicalTypeRoles == ZR_NULL) !=
        (descriptor->canonicalTypeRoleCount == 0u)) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                "module '%s' has inconsistent canonical type role storage",
                descriptor->moduleName);
        return ZR_FALSE;
    }
    if (descriptor->isContractOnly &&
        (descriptor->constantCount != 0u || descriptor->functionCount != 0u ||
         descriptor->typeCount != 0u || descriptor->moduleLinkCount != 0u ||
         descriptor->onMaterialize != ZR_NULL)) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH,
                "contract-only module '%s' publishes a loadable runtime surface",
                descriptor->moduleName);
        return ZR_FALSE;
    }
    if (descriptor->providerContractRole == ZR_PROVIDER_CONTRACT_ROLE_NONE) {
        if (descriptor->canonicalTypeRoleCount == 0u && !descriptor->isContractOnly) {
            return ZR_TRUE;
        }
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH,
                "module '%s' publishes canonical type roles without a provider contract role",
                descriptor->moduleName);
        return ZR_FALSE;
    }
    if (descriptor->canonicalTypeRoleCount == 0u) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH,
                "module '%s' declares provider role %u without canonical type roles",
                descriptor->moduleName,
                (unsigned)descriptor->providerContractRole);
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < descriptor->canonicalTypeRoleCount; index++) {
        const ZrLibCanonicalTypeRoleDescriptor *typeRole =
                &descriptor->canonicalTypeRoles[index];

        if (typeRole->canonicalName == ZR_NULL || typeRole->canonicalName[0] == '\0' ||
            native_registry_provider_role_for_canonical_type_role(typeRole->role) !=
                    descriptor->providerContractRole ||
            (typeRole->surfaceFlags & ~knownSurfaceFlags) != 0u ||
            (TZrInt32)typeRole->projectionKind < 0 ||
            typeRole->projectionKind > ZR_CANONICAL_TYPE_PROJECTION_ENUM) {
            native_registry_set_error(
                    registry,
                    ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                    "module '%s' has invalid canonical type role at index %zu",
                    descriptor->moduleName,
                    index);
            return ZR_FALSE;
        }
        if (typeRole->projectionKind != ZR_CANONICAL_TYPE_PROJECTION_ERASED) {
            if (seenProjection[typeRole->projectionKind]) {
                native_registry_set_error(
                        registry,
                        ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                        "module '%s' repeats canonical projection kind %u",
                        descriptor->moduleName,
                        (unsigned)typeRole->projectionKind);
                return ZR_FALSE;
            }
            seenProjection[typeRole->projectionKind] = ZR_TRUE;
        }
        for (TZrSize otherIndex = 0u;
             otherIndex < descriptor->canonicalTypeRoleCount;
             otherIndex++) {
            const ZrLibCanonicalTypeRoleDescriptor *otherRole =
                    &descriptor->canonicalTypeRoles[otherIndex];

            if (otherIndex != index &&
                (otherRole->role == typeRole->role ||
                 (otherRole->canonicalName != ZR_NULL &&
                  strcmp(otherRole->canonicalName, typeRole->canonicalName) == 0))) {
                native_registry_set_error(
                        registry,
                        ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                        "module '%s' repeats canonical type role or name '%s'",
                        descriptor->moduleName,
                        typeRole->canonicalName);
                return ZR_FALSE;
            }
        }

        /* 父级链必须在本模块闭合且无环，否则投影与类型查询无法终止。 */
        {
            EZrCanonicalTypeRole parentRole = typeRole->parentRole;
            TZrSize parentDepth = 0u;

            while (parentRole != ZR_CANONICAL_TYPE_ROLE_NONE) {
                const ZrLibCanonicalTypeRoleDescriptor *parent =
                        native_registry_find_local_type_role(descriptor, parentRole);

                if (parent == ZR_NULL) {
                    native_registry_set_error(
                            registry,
                            ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                            "module '%s' canonical type '%s' references missing parent role %u",
                            descriptor->moduleName,
                            typeRole->canonicalName,
                            (unsigned)parentRole);
                    return ZR_FALSE;
                }
                if (parent->role == typeRole->role ||
                    ++parentDepth >= descriptor->canonicalTypeRoleCount) {
                    native_registry_set_error(
                            registry,
                            ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                            "module '%s' canonical type '%s' participates in a parent cycle",
                            descriptor->moduleName,
                            typeRole->canonicalName);
                    return ZR_FALSE;
                }
                parentRole = parent->parentRole;
            }
        }
    }
    /* 反射 provider 必须一次提供各投影，避免消费者只看到部分类型层级。 */
    if (descriptor->providerContractRole == ZR_PROVIDER_CONTRACT_ROLE_REFLECTION) {
        for (TZrUInt32 projection = ZR_CANONICAL_TYPE_PROJECTION_CLASS;
             projection <= ZR_CANONICAL_TYPE_PROJECTION_ENUM;
             projection++) {
            if (!seenProjection[projection]) {
                native_registry_set_error(
                        registry,
                        ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                        "module '%s' is missing canonical projection kind %u",
                        descriptor->moduleName,
                        (unsigned)projection);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}
