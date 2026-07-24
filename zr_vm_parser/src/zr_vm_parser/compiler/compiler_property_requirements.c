#include "compiler_internal.h"

#include <string.h>

typedef struct SZrCompilerPropertyRequirementSearch {
    const SZrCompilerState *compilerState;
    const SZrTypeMemberInfo *implementation;
    SZrPropertyRequirementQuery *query;
    SZrArray visitedInterfaces;
} SZrCompilerPropertyRequirementSearch;

static const SZrTypePrototypeInfo *compiler_property_requirement_find_prototype(
        const SZrCompilerState *compilerState,
        SZrString *typeName) {
    if (compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < compilerState->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        (SZrArray *)&compilerState->typePrototypes,
                        index);
        if (prototype != ZR_NULL && prototype->name != ZR_NULL &&
            ZrCore_String_Equal(prototype->name, typeName)) {
            return prototype;
        }
    }
    return ZR_NULL;
}

static TZrUInt32 compiler_property_requirement_accessor_mask(
        const SZrTypeMemberInfo *member) {
    TZrUInt32 mask = ZR_PROPERTY_ACCESSOR_MASK_NONE;

    if (member == ZR_NULL) {
        return mask;
    }
    if (member->getterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) {
        mask |= ZR_PROPERTY_ACCESSOR_MASK_GET;
    }
    if (member->setterAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) {
        mask |= ZR_PROPERTY_ACCESSOR_MASK_SET;
    }
    if (member->initAccessorSymbolId != ZR_SEMANTIC_ID_INVALID) {
        mask |= ZR_PROPERTY_ACCESSOR_MASK_INIT;
    }
    return mask;
}

static TZrBool compiler_property_requirement_contract_matches(
        const SZrTypeMemberInfo *requirement,
        const SZrTypeMemberInfo *implementation) {
    return requirement != ZR_NULL && implementation != ZR_NULL &&
           requirement->memberType == ZR_AST_PROPERTY_DECLARATION &&
           implementation->memberType == ZR_AST_PROPERTY_DECLARATION &&
           requirement->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE &&
           implementation->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE &&
           requirement->name != ZR_NULL && implementation->name != ZR_NULL &&
           ZrCore_String_Equal(requirement->name, implementation->name) &&
           requirement->propertyValueTypeId == implementation->propertyValueTypeId &&
           requirement->isStatic == implementation->isStatic &&
           requirement->structuredReturnType.referenceAccess ==
                   implementation->structuredReturnType.referenceAccess &&
           requirement->exportsWritableRef == implementation->exportsWritableRef;
}

static TZrBool compiler_property_requirement_was_visited(
        SZrCompilerPropertyRequirementSearch *search,
        const SZrTypePrototypeInfo *prototype) {
    for (TZrSize index = 0U; index < search->visitedInterfaces.length; index++) {
        const SZrTypePrototypeInfo **visitedPtr =
                (const SZrTypePrototypeInfo **)ZrCore_Array_Get(
                        &search->visitedInterfaces,
                        index);
        if (visitedPtr != ZR_NULL && *visitedPtr == prototype) {
            return ZR_TRUE;
        }
    }
    ZrCore_Array_Push(
            search->compilerState->state,
            &search->visitedInterfaces,
            &prototype);
    return ZR_FALSE;
}

static void compiler_property_requirement_record_match(
        SZrCompilerPropertyRequirementSearch *search,
        const SZrTypeMemberInfo *requirement) {
    SZrPropertyRequirementQuery *query = search->query;

    query->matchingContractCount++;
    query->requiredAccessorMask |=
            compiler_property_requirement_accessor_mask(requirement);
    if (query->matchingContractCount != 1U) {
        query->interfacePropertySymbolId = ZR_SEMANTIC_ID_INVALID;
        query->interfaceGetterSymbolId = ZR_SEMANTIC_ID_INVALID;
        query->interfaceSetterSymbolId = ZR_SEMANTIC_ID_INVALID;
        query->interfaceInitializerSymbolId = ZR_SEMANTIC_ID_INVALID;
        return;
    }
    query->interfacePropertySymbolId = requirement->propertySymbolId;
    query->interfaceGetterSymbolId = requirement->getterAccessorSymbolId;
    query->interfaceSetterSymbolId = requirement->setterAccessorSymbolId;
    query->interfaceInitializerSymbolId = requirement->initAccessorSymbolId;
}

static void compiler_property_requirement_visit_interface(
        SZrCompilerPropertyRequirementSearch *search,
        const SZrTypePrototypeInfo *prototype,
        TZrUInt32 depth) {
    if (search == ZR_NULL || prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH ||
        compiler_property_requirement_was_visited(search, prototype)) {
        return;
    }

    for (TZrSize memberIndex = 0U;
         memberIndex < prototype->members.length;
         memberIndex++) {
        const SZrTypeMemberInfo *requirement =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members,
                        memberIndex);
        if (compiler_property_requirement_contract_matches(
                    requirement,
                    search->implementation)) {
            compiler_property_requirement_record_match(search, requirement);
        }
    }

    for (TZrSize inheritIndex = 0U;
         inheritIndex < prototype->inherits.length;
         inheritIndex++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(
                (SZrArray *)&prototype->inherits,
                inheritIndex);
        const SZrTypePrototypeInfo *inheritedPrototype =
                inheritNamePtr != ZR_NULL
                        ? compiler_property_requirement_find_prototype(
                                  search->compilerState,
                                  *inheritNamePtr)
                        : ZR_NULL;
        compiler_property_requirement_visit_interface(
                search,
                inheritedPrototype,
                depth + 1U);
    }
}

TZrBool ZrParser_Compiler_QueryPropertyRequirements(
        const SZrCompilerState *cs,
        const SZrTypePrototypeInfo *ownerPrototype,
        TZrSymbolId propertySymbolId,
        SZrPropertyRequirementQuery *outQuery) {
    SZrCompilerPropertyRequirementSearch search;

    if (outQuery != ZR_NULL) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || ownerPrototype == ZR_NULL ||
        propertySymbolId == ZR_SEMANTIC_ID_INVALID || outQuery == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&search, 0, sizeof(search));
    search.compilerState = cs;
    search.query = outQuery;
    for (TZrSize memberIndex = 0U;
         memberIndex < ownerPrototype->members.length;
         memberIndex++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&ownerPrototype->members,
                        memberIndex);
        if (member != ZR_NULL &&
            member->memberType == ZR_AST_PROPERTY_DECLARATION &&
            member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE &&
            member->propertySymbolId == propertySymbolId) {
            search.implementation = member;
            break;
        }
    }
    if (search.implementation == ZR_NULL) {
        return ZR_FALSE;
    }

    outQuery->presentAccessorMask =
            compiler_property_requirement_accessor_mask(search.implementation);
    ZrCore_Array_Init(
            cs->state,
            &search.visitedInterfaces,
            sizeof(const SZrTypePrototypeInfo *),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (TZrSize interfaceIndex = 0U;
         interfaceIndex < ownerPrototype->implements.length;
         interfaceIndex++) {
        SZrString **interfaceNamePtr = (SZrString **)ZrCore_Array_Get(
                (SZrArray *)&ownerPrototype->implements,
                interfaceIndex);
        const SZrTypePrototypeInfo *interfacePrototype =
                interfaceNamePtr != ZR_NULL
                        ? compiler_property_requirement_find_prototype(
                                  cs,
                                  *interfaceNamePtr)
                        : ZR_NULL;
        compiler_property_requirement_visit_interface(
                &search,
                interfacePrototype,
                0U);
    }
    ZrCore_Array_Free(cs->state, &search.visitedInterfaces);

    outQuery->missingAccessorMask =
            outQuery->requiredAccessorMask & ~outQuery->presentAccessorMask;
    return outQuery->matchingContractCount > 0U;
}
