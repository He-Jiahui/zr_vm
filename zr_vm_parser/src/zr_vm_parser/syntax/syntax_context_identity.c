#include "syntax/syntax_context_internal.h"

#include <string.h>

static const TZrUInt64 identity_hash_seed = UINT64_C(1469598103934665603);
static const TZrUInt64 identity_hash_prime = UINT64_C(1099511628211);

static TZrUInt32 identity_context_representative(SZrSyntaxContext context) {
    TZrUInt32 representative;

    if (context.markId == 0u) {
        return 0u;
    }
    if (context.table == ZR_NULL ||
        context.markId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        (TZrSize)context.markId > context.table->markCount) {
        return ZR_SYNTAX_CONTEXT_INVALID_MARK_ID;
    }
    representative = context.table->marks[
            (TZrSize)context.markId - 1u].representativeMarkId;
    if (representative == 0u || representative > context.markId) {
        return ZR_SYNTAX_CONTEXT_INVALID_MARK_ID;
    }
    return representative;
}

static TZrBool identity_context_equal(
        SZrSyntaxContext left,
        SZrSyntaxContext right) {
    TZrUInt32 leftRepresentative;
    TZrUInt32 rightRepresentative;

    if (left.markId == 0u || right.markId == 0u) {
        return left.markId == right.markId;
    }
    if (left.table != right.table) {
        return ZR_FALSE;
    }
    leftRepresentative = identity_context_representative(left);
    rightRepresentative = identity_context_representative(right);
    return leftRepresentative != ZR_SYNTAX_CONTEXT_INVALID_MARK_ID &&
           leftRepresentative == rightRepresentative;
}

TZrBool ZrParser_SyntaxContext_ExpansionInfoEqual(
        const SZrSyntaxExpansionInfo *left,
        const SZrSyntaxExpansionInfo *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->stableIdentity != right->stableIdentity ||
        left->parentExpansionId != right->parentExpansionId ||
        left->definitionModuleLength != right->definitionModuleLength ||
        left->callSiteSourceLength != right->callSiteSourceLength ||
        left->definitionSiteSourceLength != right->definitionSiteSourceLength ||
        memcmp(&left->callSiteSpan, &right->callSiteSpan,
               sizeof(left->callSiteSpan)) != 0 ||
        memcmp(&left->definitionSiteSpan, &right->definitionSiteSpan,
               sizeof(left->definitionSiteSpan)) != 0 ||
        (left->definitionModuleLength != 0u &&
         memcmp(left->definitionModule, right->definitionModule,
                left->definitionModuleLength) != 0) ||
        (left->callSiteSourceLength != 0u &&
         memcmp(left->callSiteSource, right->callSiteSource,
                left->callSiteSourceLength) != 0) ||
        (left->definitionSiteSourceLength != 0u &&
         memcmp(left->definitionSiteSource, right->definitionSiteSource,
                left->definitionSiteSourceLength) != 0) ||
        !identity_context_equal(left->callSiteContext, right->callSiteContext) ||
        !identity_context_equal(
                left->definitionSiteContext, right->definitionSiteContext)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 identity_hash_bytes(
        TZrUInt64 hash, const TZrChar *bytes, TZrSize length) {
    if (bytes == ZR_NULL) {
        hash ^= UINT64_C(0xFF);
        hash *= identity_hash_prime;
        return hash;
    }
    for (TZrSize index = 0u; index < length; index++) {
        hash ^= (TZrUInt64)(TZrByte)bytes[index];
        hash *= identity_hash_prime;
    }
    hash ^= (TZrUInt64)length;
    hash *= identity_hash_prime;
    return hash;
}

static TZrUInt64 identity_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    for (TZrSize index = 0u; index < sizeof(value); index++) {
        hash ^= (value >> (index * 8u)) & UINT64_C(0xFF);
        hash *= identity_hash_prime;
    }
    return hash;
}

static TZrUInt64 identity_hash_mark(
        const SZrSyntaxContextTable *table,
        TZrUInt64 hash,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency) {
    const SZrSyntaxExpansionInfo *expansion =
            ZrParser_SyntaxContextTable_Expansion(table, markerId);

    hash = identity_hash_u64(hash, (TZrUInt64)transparency + 1u);
    if (expansion == ZR_NULL) {
        hash = identity_hash_u64(hash, UINT64_C(0xBAD0));
        return identity_hash_u64(hash, markerId);
    }
    hash = identity_hash_u64(hash, expansion->stableIdentity);
    if (expansion->parentExpansionId != 0u) {
        const SZrSyntaxExpansionInfo *parent =
                ZrParser_SyntaxContextTable_Expansion(
                        table, expansion->parentExpansionId);

        hash = identity_hash_u64(
                hash, parent == ZR_NULL ? UINT64_C(0xBAD1)
                                        : parent->stableIdentity);
    } else {
        hash = identity_hash_u64(hash, UINT64_C(0));
    }
    if (expansion->stableIdentity == 0u) {
        hash = identity_hash_bytes(
                hash, expansion->definitionModule,
                expansion->definitionModuleLength);
        hash = identity_hash_bytes(
                hash, expansion->callSiteSource,
                expansion->callSiteSourceLength);
        hash = identity_hash_bytes(
                hash, expansion->definitionSiteSource,
                expansion->definitionSiteSourceLength);
    }
    return hash;
}

void ZrParser_SyntaxContext_InitializeMark(
        SZrSyntaxContextTable *table,
        SZrSyntaxMarkRecord *record,
        TZrUInt32 parentMarkId,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency) {
    SZrSyntaxContext parent = {table, parentMarkId};
    TZrUInt32 parentRepresentative = identity_context_representative(parent);
    const SZrSyntaxExpansionInfo *expansion =
            ZrParser_SyntaxContextTable_Expansion(table, markerId);
    TZrUInt64 hash = parentMarkId == 0u ? identity_hash_seed
            : table->marks[(TZrSize)parentMarkId - 1u].rollingHashPrefix;

    record->parentMarkId = parentMarkId;
    record->markerId = markerId;
    record->transparency = transparency;
    record->representativeMarkId = (TZrUInt32)(table->markCount + 1u);
    record->rollingHashPrefix = identity_hash_mark(
            table, hash, markerId, transparency);
    /* Registered site contexts already have exact representatives.  Scanning
     * only prior marks avoids recursive comparison and forward rollback links. */
    for (TZrSize index = 0u; index < table->markCount; index++) {
        const SZrSyntaxMarkRecord *candidate = &table->marks[index];
        SZrSyntaxContext candidateParent = {table, candidate->parentMarkId};

        if (candidate->transparency == transparency &&
            identity_context_representative(candidateParent) ==
                    parentRepresentative &&
            (candidate->markerId == markerId ||
             ZrParser_SyntaxContext_ExpansionInfoEqual(
                     ZrParser_SyntaxContextTable_Expansion(
                             table, candidate->markerId),
                     expansion))) {
            record->representativeMarkId = candidate->representativeMarkId;
            break;
        }
    }
}

TZrUInt64 ZrParser_SyntaxContext_StableHash(SZrSyntaxContext context) {
    TZrUInt64 hash;

    if (!ZrParser_SyntaxContext_IsValid(context)) {
        return 0u;
    }
    if (context.markId == 0u) {
        return identity_hash_seed;
    }
    hash = context.table->marks[
            (TZrSize)context.markId - 1u].rollingHashPrefix;
    hash ^= identity_hash_prime;
    hash *= identity_hash_prime;
    return hash;
}

TZrBool ZrParser_SyntaxContext_IsValid(SZrSyntaxContext context) {
    if (context.table == ZR_NULL) {
        return context.markId == 0u;
    }
    if (context.markId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID) {
        return ZR_FALSE;
    }
    return ZrParser_SyntaxContext_ValidateMarkChain(
            context.table, context.markId);
}

TZrBool ZrParser_SyntaxContext_IsSameTable(
        SZrSyntaxContext left,
        SZrSyntaxContext right) {
    return left.table == right.table;
}

TZrBool ZrParser_SyntaxContext_Equal(
        SZrSyntaxContext left,
        SZrSyntaxContext right) {
    if (!ZrParser_SyntaxContext_IsValid(left) ||
        !ZrParser_SyntaxContext_IsValid(right)) {
        return ZR_FALSE;
    }
    return identity_context_equal(left, right);
}
