#include "reflection_property_internal.h"

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/string.h"

#include <stdio.h>
#include <string.h>

/* 编译成员表中的无属性身份哨兵；规范 carrier 与关联 accessor 均不得使用它。 */
#define ZR_REFLECTION_PROPERTY_IDENTITY_NONE ((TZrUInt32)0xffffffffu)

/* 编译元数据的 accessorRole 固定为 1=getter、2=setter、3=initializer；下方映射反射名和字段。 */
static const TZrChar *reflection_property_accessor_name(TZrUInt32 role) {
    switch (role) {
        case 1u:
            return "get";
        case 2u:
            return "set";
        case 3u:
            return "init";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *reflection_property_accessor_field(TZrUInt32 role) {
    switch (role) {
        case 1u:
            return "getter";
        case 2u:
            return "setter";
        case 3u:
            return "initializer";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *reflection_property_accessor_access_field(
        TZrUInt32 role) {
    switch (role) {
        case 1u:
            return "getterAccess";
        case 2u:
            return "setterAccess";
        case 3u:
            return "initializerAccess";
        default:
            return ZR_NULL;
    }
}

TZrBool ZrCore_ReflectionProperty_IsCanonicalCarrier(
        const SZrCompiledMemberInfo *member) {
    return (TZrBool)(
            member != ZR_NULL &&
            member->memberType == ZR_AST_CONSTANT_PROPERTY_DECLARATION &&
            member->accessorRole == 0u &&
            member->propertyIdentity != ZR_REFLECTION_PROPERTY_IDENTITY_NONE);
}
/* 只有成员表含规范承载项时，专用属性路径才接管该身份的访问器；无承载项记录留给通用路径。 */
static TZrBool reflection_property_has_carrier(
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        TZrUInt32 propertyIdentity) {
    if (members == ZR_NULL ||
        propertyIdentity == ZR_REFLECTION_PROPERTY_IDENTITY_NONE) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < memberCount; index++) {
        if (ZrCore_ReflectionProperty_IsCanonicalCarrier(&members[index]) &&
            members[index].propertyIdentity == propertyIdentity) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrCore_ReflectionProperty_ShouldSkipCanonicalMember(
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        const SZrCompiledMemberInfo *member) {
    if (ZrCore_ReflectionProperty_IsCanonicalCarrier(member)) {
        return ZR_TRUE;
    }
    return (TZrBool)(
            member != ZR_NULL && member->accessorRole != 0u &&
            reflection_property_has_carrier(
                    members, memberCount, member->propertyIdentity));
}
/* 访问器自身提供函数和权限数据，carrier 提供属性级类型与引用能力。 */
static SZrObject *reflection_property_build_accessor(
        SZrState *state,
        SZrObject *typeReflection,
        SZrObject *moduleReflection,
        const TZrChar *qualifiedTypeName,
        const TZrChar *propertyName,
        SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *carrier,
        const SZrCompiledMemberInfo *member,
        TZrUInt32 memberIndex,
        TZrUInt64 ownerHash,
        const SZrReflectionPropertyHost *host) {
    const TZrChar *roleName =
            reflection_property_accessor_name(member->accessorRole);
    const TZrChar *returnTypeName;
    TZrChar qualifiedName[256];
    SZrObject *accessorReflection;
    SZrFunction *function;

    if (roleName == ZR_NULL) {
        return ZR_NULL;
    }
    snprintf(qualifiedName,
             sizeof(qualifiedName),
             "%s.%s.%s",
             qualifiedTypeName,
             propertyName,
             roleName);
    accessorReflection = host->buildMemberInfo(
            state,
            roleName,
            qualifiedName,
            "method",
            ownerHash ^
                    ((TZrUInt64)memberIndex +
                     ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE + 0x2000u));
    if (accessorReflection == ZR_NULL) {
        return ZR_NULL;
    }
    host->assignOwnerLinks(
            state,
            accessorReflection,
            typeReflection,
            moduleReflection);
    host->populateCompiledMetadata(
            state,
            accessorReflection,
            entryFunction,
            member);
    /*
     * BUG: accessorReflection 接入所属 property 前只由 C 局部变量持有；此回调先分配字符串、后 pin 接收对象。
     *      分配失败引发完整 GC 时对象可被回收，随后字段写入继续使用失效指针。
     */
    host->setFieldString(
            state,
            accessorReflection,
            "baseDefinitionName",
            propertyName);
    host->setFieldBool(
            state,
            accessorReflection,
            "isStatic",
            member->isStatic ? ZR_TRUE : ZR_FALSE);
    host->setFieldBool(
            state,
            accessorReflection,
            "isConst",
            member->isConst ? ZR_TRUE : ZR_FALSE);
    host->setFieldInt(
            state,
            accessorReflection,
            "parameterCount",
            member->parameterCount);
    host->setFieldInt(
            state,
            accessorReflection,
            "access",
            member->accessModifier);
    host->setFieldInt(
            state,
            accessorReflection,
            "propertyTypeId",
            ZR_COMPILED_PROPERTY_VALUE_TYPE_ID(carrier));
    host->setFieldInt(
            state,
            accessorReflection,
            "receiverEffect",
            member->isStatic
                    ? ZR_MEMBER_RECEIVER_EFFECT_NONE
                    : (member->isConst
                               ? ZR_MEMBER_RECEIVER_EFFECT_READONLY
                               : ZR_MEMBER_RECEIVER_EFFECT_MUTABLE));
    host->setFieldInt(
            state,
            accessorReflection,
            "referenceAccess",
            member->accessorRole == 1u
                    ? ZR_COMPILED_PROPERTY_REFERENCE_ACCESS(carrier)
                    : ZR_MEMBER_REFERENCE_ACCESS_NONE);
    host->setFieldBool(
            state,
            accessorReflection,
            "exportsWritableRef",
            member->accessorRole == 1u &&
                    ZR_COMPILED_PROPERTY_EXPORTS_WRITABLE_REF(carrier)
                    ? ZR_TRUE
                    : ZR_FALSE);
    returnTypeName = host->stringFromConstant(
            state,
            entryFunction,
            member->returnTypeNameStringIndex,
            member->accessorRole == 1u ? "any" : "void");
    host->setFieldString(
            state,
            accessorReflection,
            "returnTypeName",
            returnTypeName);
    function = host->extractFunction(
            state,
            entryFunction,
            member->functionConstantIndex);
    if (function != ZR_NULL) {
        host->populateParameters(
                state,
                accessorReflection,
                function,
                member->parameterCount);
        host->populateFunctionMetadata(
                state,
                accessorReflection,
                function);
    }
    host->populateDecoratorMetadata(
            state,
            accessorReflection,
            entryFunction,
            member);
    return accessorReflection;
}

void ZrCore_ReflectionProperty_PopulateCurrent(
        SZrState *state,
        SZrObject *membersObject,
        SZrObject *typeReflection,
        SZrObject *moduleReflection,
        SZrObjectPrototype *prototype,
        const TZrChar *qualifiedTypeName,
        SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        const SZrReflectionPropertyHost *host) {
    if (state == ZR_NULL || membersObject == ZR_NULL ||
        typeReflection == ZR_NULL || prototype == ZR_NULL ||
        qualifiedTypeName == ZR_NULL || entryFunction == ZR_NULL ||
        members == ZR_NULL || host == ZR_NULL) {
        return;
    }

    for (TZrUInt32 propertyIndex = 0u;
         propertyIndex < memberCount;
         propertyIndex++) {
        const SZrCompiledMemberInfo *carrier = &members[propertyIndex];
        const TZrChar *propertyName;
        const TZrChar *propertyTypeName;
        TZrChar qualifiedName[256];
        SZrObject *propertyReflection;

        if (!ZrCore_ReflectionProperty_IsCanonicalCarrier(carrier)) {
            continue;
        }
        propertyName = host->stringFromConstant(
                state,
                entryFunction,
                carrier->nameStringIndex,
                ZR_NULL);
        if (propertyName == ZR_NULL || propertyName[0] == '\0') {
            continue;
        }
        snprintf(qualifiedName,
                 sizeof(qualifiedName),
                 "%s.%s",
                 qualifiedTypeName,
                 propertyName);
        propertyReflection = host->buildMemberInfo(
                state,
                propertyName,
                qualifiedName,
                "property",
                prototype->super.super.hash ^
                        ((TZrUInt64)propertyIndex +
                         ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE + 0x1000u));
        if (propertyReflection == ZR_NULL) {
            continue;
        }
        host->assignOwnerLinks(
                state,
                propertyReflection,
                typeReflection,
                moduleReflection);
        host->populateCompiledMetadata(
                state,
                propertyReflection,
                entryFunction,
                carrier);
        propertyTypeName = host->stringFromConstant(
                state,
                entryFunction,
                carrier->fieldTypeNameStringIndex,
                "any");
        /*
         * BUG: propertyReflection 加入 members 前只由 C 局部变量持有；此回调先分配字符串、后 pin 接收对象。
         *      分配失败引发完整 GC 时对象可被回收，随后字段写入继续使用失效指针。
         */
        host->setFieldString(
                state,
                propertyReflection,
                "typeName",
                propertyTypeName);
        host->setFieldBool(
                state,
                propertyReflection,
                "isStatic",
                carrier->isStatic ? ZR_TRUE : ZR_FALSE);
        host->setFieldBool(
                state,
                propertyReflection,
                "isConst",
                ZR_COMPILED_PROPERTY_REFERENCE_ACCESS(carrier) ==
                        ZR_MEMBER_REFERENCE_ACCESS_READONLY);
        host->setFieldInt(
                state,
                propertyReflection,
                "access",
                carrier->accessModifier);
        host->setFieldInt(
                state,
                propertyReflection,
                "parameterCount",
                0);
        host->setFieldInt(
                state,
                propertyReflection,
                "propertyTypeId",
                ZR_COMPILED_PROPERTY_VALUE_TYPE_ID(carrier));
        host->setFieldInt(
                state,
                propertyReflection,
                "receiverEffect",
                carrier->isStatic
                        ? ZR_MEMBER_RECEIVER_EFFECT_NONE
                        : (carrier->isConst
                                   ? ZR_MEMBER_RECEIVER_EFFECT_READONLY
                                   : ZR_MEMBER_RECEIVER_EFFECT_MUTABLE));
        host->setFieldInt(
                state,
                propertyReflection,
                "referenceAccess",
                ZR_COMPILED_PROPERTY_REFERENCE_ACCESS(carrier));
        host->setFieldBool(
                state,
                propertyReflection,
                "exportsWritableRef",
                ZR_COMPILED_PROPERTY_EXPORTS_WRITABLE_REF(carrier)
                        ? ZR_TRUE
                        : ZR_FALSE);
        host->setFieldInt(
                state,
                propertyReflection,
                "getterAccess",
                ZR_MEMBER_ACCESS_MODIFIER_UNAVAILABLE);
        host->setFieldInt(
                state,
                propertyReflection,
                "setterAccess",
                ZR_MEMBER_ACCESS_MODIFIER_UNAVAILABLE);
        host->setFieldInt(
                state,
                propertyReflection,
                "initializerAccess",
                ZR_MEMBER_ACCESS_MODIFIER_UNAVAILABLE);
        /* 缺失访问器保持 unavailable；只把同一 propertyIdentity 的可识别角色挂到规范属性。 */
        for (TZrUInt32 memberIndex = 0u;
             memberIndex < memberCount;
             memberIndex++) {
            const SZrCompiledMemberInfo *accessor = &members[memberIndex];
            const TZrChar *fieldName;
            const TZrChar *accessFieldName;
            SZrObject *accessorReflection;

            if (accessor->propertyIdentity != carrier->propertyIdentity ||
                accessor->accessorRole == 0u) {
                continue;
            }
            fieldName = reflection_property_accessor_field(
                    accessor->accessorRole);
            accessFieldName = reflection_property_accessor_access_field(
                    accessor->accessorRole);
            if (fieldName == ZR_NULL || accessFieldName == ZR_NULL) {
                continue;
            }
            accessorReflection = reflection_property_build_accessor(
                    state,
                    typeReflection,
                    moduleReflection,
                    qualifiedTypeName,
                    propertyName,
                    entryFunction,
                    carrier,
                    accessor,
                    memberIndex,
                    prototype->super.super.hash,
                    host);
            if (accessorReflection == ZR_NULL) {
                continue;
            }
            host->setFieldObject(
                    state,
                    propertyReflection,
                    fieldName,
                    accessorReflection,
                    ZR_VALUE_TYPE_OBJECT);
            host->setFieldInt(
                    state,
                    propertyReflection,
                    accessFieldName,
                    accessor->accessModifier);
        }
        /* 装饰器属于规范属性记录；所有访问器链接完成后再把属性发布为一个 named entry。 */
        host->populateDecoratorMetadata(
                state,
                propertyReflection,
                entryFunction,
                carrier);
        host->addNamedEntry(
                state,
                membersObject,
                propertyName,
                propertyReflection);
    }
}
