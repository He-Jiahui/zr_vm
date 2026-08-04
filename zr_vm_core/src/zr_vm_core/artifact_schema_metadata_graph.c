#include "artifact_schema_internal.h"

static const SZrArtifactSectionInput *artifact_metadata_find_input(
        const SZrArtifactDocument *document,
        EZrArtifactSectionKind kind) {
    TZrUInt32 index;

    for (index = 0u; index < document->sectionCount; ++index) {
        if (document->sections[index].kind == kind) {
            return &document->sections[index];
        }
    }
    return ZR_NULL;
}

static TZrBool artifact_metadata_input_find_type_def(
        const SZrArtifactDocument *document,
        TZrMetadataToken token,
        SZrArtifactTypeDefRow *outRow) {
    const SZrArtifactSectionInput *section = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE);
    TZrUInt32 index;

    for (index = 0u; section != ZR_NULL && index < section->elementCount; ++index) {
        const SZrArtifactTypeDefRow *row =
                &((const SZrArtifactTypeDefRow *)section->data)[index];
        if (row->token == token) {
            if (outRow != ZR_NULL) *outRow = *row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_input_has_type_token(
        const SZrArtifactDocument *document,
        TZrMetadataToken token) {
    const SZrArtifactSectionInput *section;
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    TZrUInt32 index;

    if (table == ZR_METADATA_TABLE_TYPE_DEF) {
        return artifact_metadata_input_find_type_def(document, token, ZR_NULL);
    }
    if (table != ZR_METADATA_TABLE_TYPE_REF &&
        table != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }
    section = artifact_metadata_find_input(
            document,
            table == ZR_METADATA_TABLE_TYPE_REF
                    ? ZR_ARTIFACT_SECTION_TYPE_REF_TABLE
                    : ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE);
    for (index = 0u; section != ZR_NULL && index < section->elementCount; ++index) {
        if (((const SZrArtifactTypeIdentityRow *)section->data)[index].token == token) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_input_find_member_owner(
        const SZrArtifactDocument *document,
        TZrMetadataToken memberToken,
        TZrMetadataToken *outOwnerToken) {
    const SZrArtifactSectionInput *members = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE);
    const SZrArtifactSectionInput *properties = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE);
    TZrUInt32 index;

    for (index = 0u; members != ZR_NULL && index < members->elementCount; ++index) {
        const SZrArtifactMemberDefRow *row =
                &((const SZrArtifactMemberDefRow *)members->data)[index];
        if (row->token == memberToken) {
            if (outOwnerToken != ZR_NULL) *outOwnerToken = row->ownerTypeToken;
            return ZR_TRUE;
        }
    }
    for (index = 0u; properties != ZR_NULL && index < properties->elementCount; ++index) {
        const SZrArtifactPropertyDefRow *row =
                &((const SZrArtifactPropertyDefRow *)properties->data)[index];
        if (row->token == memberToken) {
            if (outOwnerToken != ZR_NULL) *outOwnerToken = row->ownerTypeToken;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_input_member_has_owner(
        const SZrArtifactDocument *document,
        TZrMetadataToken memberToken,
        TZrMetadataToken expectedOwnerToken) {
    TZrMetadataToken actualOwnerToken = 0u;

    return (TZrBool)(artifact_metadata_input_find_member_owner(
                             document, memberToken, &actualOwnerToken) &&
                     actualOwnerToken == expectedOwnerToken);
}

static TZrBool artifact_metadata_input_has_contract_hash(
        const SZrArtifactDocument *document,
        TZrUInt64 contractHash) {
    const SZrArtifactSectionInput *contracts = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_CONTRACT_TABLE);
    TZrUInt32 index;

    for (index = 0u; contracts != ZR_NULL && index < contracts->elementCount; ++index) {
        if (((const SZrArtifactContractRow *)contracts->data)[index].contractHash ==
            contractHash) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_input_find_layout(
        const SZrArtifactDocument *document,
        TZrMetadataToken typeToken,
        SZrArtifactLayoutRow *outRow) {
    const SZrArtifactSectionInput *layouts = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_LAYOUT_TABLE);
    TZrUInt32 index;

    for (index = 0u; layouts != ZR_NULL && index < layouts->elementCount; ++index) {
        const SZrArtifactLayoutRow *row =
                &((const SZrArtifactLayoutRow *)layouts->data)[index];
        if (row->typeToken == typeToken) {
            if (outRow != ZR_NULL) *outRow = *row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrUInt32 artifact_metadata_input_count_members(
        const SZrArtifactDocument *document,
        TZrMetadataToken typeToken) {
    const SZrArtifactSectionInput *members = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE);
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    for (index = 0u; members != ZR_NULL && index < members->elementCount; ++index) {
        if (((const SZrArtifactMemberDefRow *)members->data)[index].ownerTypeToken ==
            typeToken) {
            ++count;
        }
    }
    return count;
}

static TZrUInt32 artifact_metadata_input_count_properties(
        const SZrArtifactDocument *document,
        TZrMetadataToken typeToken) {
    const SZrArtifactSectionInput *properties = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE);
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    for (index = 0u; properties != ZR_NULL && index < properties->elementCount; ++index) {
        if (((const SZrArtifactPropertyDefRow *)properties->data)[index].ownerTypeToken ==
            typeToken) {
            ++count;
        }
    }
    return count;
}

static TZrBool artifact_metadata_category_matches_type(
        EZrArtifactReflectionCategory category,
        const SZrArtifactTypeDefRow *typeDef) {
    TZrUInt32 flags = typeDef->flags;

    switch (category) {
        case ZR_ARTIFACT_REFLECTION_CATEGORY_ERASED:
            return ZR_TRUE;
        case ZR_ARTIFACT_REFLECTION_CATEGORY_CLASS:
            return (TZrBool)((flags & ZR_ARTIFACT_TYPE_FLAG_GC) != 0u &&
                             (flags & (ZR_ARTIFACT_TYPE_FLAG_RESOURCE |
                                       ZR_ARTIFACT_TYPE_FLAG_INTERFACE)) == 0u);
        case ZR_ARTIFACT_REFLECTION_CATEGORY_CONCRETE_CLASS:
            return (TZrBool)((flags & ZR_ARTIFACT_TYPE_FLAG_GC) != 0u &&
                             (flags & (ZR_ARTIFACT_TYPE_FLAG_RESOURCE |
                                       ZR_ARTIFACT_TYPE_FLAG_INTERFACE |
                                       ZR_ARTIFACT_TYPE_FLAG_ABSTRACT)) == 0u);
        case ZR_ARTIFACT_REFLECTION_CATEGORY_INSTANCE_CLASS:
            return (TZrBool)((flags & (ZR_ARTIFACT_TYPE_FLAG_GC |
                                      ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE)) ==
                                     (ZR_ARTIFACT_TYPE_FLAG_GC |
                                      ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE) &&
                             (flags & (ZR_ARTIFACT_TYPE_FLAG_RESOURCE |
                                       ZR_ARTIFACT_TYPE_FLAG_INTERFACE |
                                       ZR_ARTIFACT_TYPE_FLAG_ABSTRACT)) == 0u);
        case ZR_ARTIFACT_REFLECTION_CATEGORY_STRUCT:
            return (TZrBool)((flags & ZR_ARTIFACT_TYPE_FLAG_VALUE) != 0u &&
                             (flags & (ZR_ARTIFACT_TYPE_FLAG_REF_LIKE |
                                       ZR_ARTIFACT_TYPE_FLAG_ENUM)) == 0u);
        case ZR_ARTIFACT_REFLECTION_CATEGORY_INTERFACE:
            return (TZrBool)((flags & ZR_ARTIFACT_TYPE_FLAG_INTERFACE) != 0u);
        case ZR_ARTIFACT_REFLECTION_CATEGORY_RESOURCE_CLASS:
            return (TZrBool)((flags & ZR_ARTIFACT_TYPE_FLAG_RESOURCE) != 0u);
        case ZR_ARTIFACT_REFLECTION_CATEGORY_REF_STRUCT:
            return (TZrBool)((flags & (ZR_ARTIFACT_TYPE_FLAG_VALUE |
                                      ZR_ARTIFACT_TYPE_FLAG_REF_LIKE)) ==
                                     (ZR_ARTIFACT_TYPE_FLAG_VALUE |
                                      ZR_ARTIFACT_TYPE_FLAG_REF_LIKE));
        case ZR_ARTIFACT_REFLECTION_CATEGORY_ENUM:
            return (TZrBool)((flags & (ZR_ARTIFACT_TYPE_FLAG_VALUE |
                                      ZR_ARTIFACT_TYPE_FLAG_ENUM)) ==
                                     (ZR_ARTIFACT_TYPE_FLAG_VALUE |
                                      ZR_ARTIFACT_TYPE_FLAG_ENUM));
        default:
            return ZR_FALSE;
    }
}

static EZrArtifactStatus artifact_metadata_validate_layout_map(
        const TZrByte *heap,
        TZrUInt32 heapLength,
        const SZrArtifactLayoutRow *layout,
        TZrUInt32 rowIndex,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *map;
    TZrUInt64 mapEnd;
    TZrUInt64 expectedLength;
    TZrUInt32 counts[3];
    TZrUInt32 segment;
    TZrUInt32 cursor;

    if (layout->ownershipMapLength == 0u) {
        if (layout->ownershipMapOffset != 0u ||
            layout->gcScanKind == ZR_ARTIFACT_GC_SCAN_MAPPED) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                    rowIndex,
                    layout->ownershipMapOffset);
        }
        return ZR_ARTIFACT_STATUS_OK;
    }
    mapEnd = (TZrUInt64)layout->ownershipMapOffset + layout->ownershipMapLength;
    if (heap == ZR_NULL || mapEnd > heapLength ||
        layout->ownershipMapLength < ZR_ARTIFACT_LAYOUT_MAP_HEADER_ENCODED_SIZE) {
        return zr_artifact_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                rowIndex,
                layout->ownershipMapOffset);
    }
    map = heap + layout->ownershipMapOffset;
    if (zr_artifact_read_u32(map) != ZR_ARTIFACT_LAYOUT_MAP_VERSION) {
        return zr_artifact_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                rowIndex,
                layout->ownershipMapOffset);
    }
    counts[0] = zr_artifact_read_u32(map + 4u);
    counts[1] = zr_artifact_read_u32(map + 8u);
    counts[2] = zr_artifact_read_u32(map + 12u);
    expectedLength = ZR_ARTIFACT_LAYOUT_MAP_HEADER_ENCODED_SIZE +
                     (TZrUInt64)(counts[0] + counts[1] + counts[2]) * 4u;
    if (counts[0] > ZR_ARTIFACT_MAX_ROW_COUNT ||
        counts[1] > ZR_ARTIFACT_MAX_ROW_COUNT ||
        counts[2] > ZR_ARTIFACT_MAX_ROW_COUNT ||
        expectedLength != layout->ownershipMapLength ||
        (layout->gcScanKind == ZR_ARTIFACT_GC_SCAN_MAPPED && counts[0] == 0u) ||
        (layout->gcScanKind == ZR_ARTIFACT_GC_SCAN_FREE && counts[0] != 0u)) {
        return zr_artifact_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                rowIndex,
                layout->ownershipMapOffset);
    }
    cursor = ZR_ARTIFACT_LAYOUT_MAP_HEADER_ENCODED_SIZE;
    for (segment = 0u; segment < 3u; ++segment) {
        TZrUInt32 index;
        TZrUInt32 previous = 0u;
        for (index = 0u; index < counts[segment]; ++index) {
            TZrUInt32 offset = zr_artifact_read_u32(map + cursor);
            if (offset >= layout->byteSize || (index > 0u && offset <= previous)) {
                return zr_artifact_fail(
                        diagnostic,
                        ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                        ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                        rowIndex,
                        layout->ownershipMapOffset + cursor);
            }
            previous = offset;
            cursor += 4u;
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static TZrBool artifact_metadata_state_shape_is_valid(
        const SZrArtifactMetadataStateRow *state,
        const SZrArtifactTypeDefRow *typeDef) {
    return (TZrBool)(
            state->preservationState >= ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
            state->preservationState <= ZR_ARTIFACT_METADATA_PRESERVATION_FULL &&
            (state->category != ZR_ARTIFACT_REFLECTION_CATEGORY_ERASED ||
             state->preservationState ==
                     ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY) &&
            state->metadataGeneration != 0u &&
            (state->flags & ~ZR_ARTIFACT_METADATA_STATE_FLAG_KNOWN_MASK) == 0u &&
            state->typeSignatureHash != 0u && state->layoutHash != 0u &&
            state->callableContractHash != 0u && state->metadataHash != 0u &&
            state->metadataHash ==
                    ZrCore_Artifact_ComputeMetadataStateHash(state) &&
            state->typeSignatureHash == typeDef->typeSignatureHash &&
            artifact_metadata_category_matches_type(state->category, typeDef));
}

static TZrBool artifact_metadata_record_shape_is_valid(
        const SZrArtifactMetadataRecordRow *record,
        const TZrByte *blob,
        TZrUInt32 blobLength) {
    TZrUInt64 payloadEnd =
            (TZrUInt64)record->payloadOffset + record->payloadLength;

    if (record->kind < ZR_ARTIFACT_METADATA_RECORD_ATTRIBUTE_DATA ||
        record->kind > ZR_ARTIFACT_METADATA_RECORD_DECLARATION_FLAG ||
        record->retention < ZR_ARTIFACT_METADATA_RETENTION_RUNTIME ||
        record->retention > ZR_ARTIFACT_METADATA_RETENTION_COMPILE_TOOL ||
        (record->flags & ~ZR_ARTIFACT_METADATA_RECORD_FLAG_KNOWN_MASK) != 0u ||
        record->payloadLength == 0u || payloadEnd > blobLength || blob == ZR_NULL ||
        record->metadataGeneration == 0u || record->reserved0 != 0u ||
        record->recordHash == 0u) {
        return ZR_FALSE;
    }
    return (TZrBool)(record->recordHash == ZrCore_Artifact_ComputeMetadataRecordHash(
            record,
            blob + record->payloadOffset,
            record->payloadLength));
}

static TZrUInt32 artifact_metadata_input_count_records(
        const SZrArtifactDocument *document,
        TZrMetadataToken typeToken) {
    const SZrArtifactSectionInput *records = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE);
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    for (index = 0u; records != ZR_NULL && index < records->elementCount; ++index) {
        const SZrArtifactMetadataRecordRow *record =
                &((const SZrArtifactMetadataRecordRow *)records->data)[index];
        TZrMetadataToken ownerTypeToken = record->ownerToken;
        if (ZR_METADATA_TOKEN_TABLE(ownerTypeToken) == ZR_METADATA_TABLE_MEMBER_DEF &&
            !artifact_metadata_input_find_member_owner(
                    document, ownerTypeToken, &ownerTypeToken)) {
            continue;
        }
        if (ownerTypeToken == typeToken) ++count;
    }
    return count;
}

static EZrArtifactStatus artifact_metadata_validate_input_links(
        const SZrArtifactDocument *document,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactSectionInput *members = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE);
    const SZrArtifactSectionInput *properties = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE);
    TZrUInt32 index;

    for (index = 0u; members != ZR_NULL && index < members->elementCount; ++index) {
        const SZrArtifactMemberDefRow *row =
                &((const SZrArtifactMemberDefRow *)members->data)[index];
        if (!artifact_metadata_input_has_type_token(document, row->ownerTypeToken)) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
                    index,
                    0u);
        }
    }
    for (index = 0u; properties != ZR_NULL && index < properties->elementCount; ++index) {
        const SZrArtifactPropertyDefRow *row =
                &((const SZrArtifactPropertyDefRow *)properties->data)[index];
        if (!artifact_metadata_input_has_type_token(document, row->ownerTypeToken) ||
            (row->getterToken != 0u &&
             !artifact_metadata_input_member_has_owner(
                     document, row->getterToken, row->ownerTypeToken)) ||
            (row->setterToken != 0u &&
             !artifact_metadata_input_member_has_owner(
                     document, row->setterToken, row->ownerTypeToken)) ||
            (row->initializerToken != 0u &&
             !artifact_metadata_input_member_has_owner(
                     document, row->initializerToken, row->ownerTypeToken))) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE,
                    index,
                    0u);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_metadata_validate_input_state(
        const SZrArtifactDocument *document,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrArtifactSectionInput *states = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE);
    const SZrArtifactSectionInput *records = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE);
    const SZrArtifactSectionInput *blob = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_METADATA_BLOB_HEAP);
    const SZrArtifactSectionInput *mapHeap = artifact_metadata_find_input(
            document, ZR_ARTIFACT_SECTION_LAYOUT_MAP_HEAP);
    TZrUInt32 index;

    if (states == ZR_NULL) return ZR_ARTIFACT_STATUS_OK;
    for (index = 0u; index < states->elementCount; ++index) {
        const SZrArtifactMetadataStateRow *state =
                &((const SZrArtifactMetadataStateRow *)states->data)[index];
        SZrArtifactTypeDefRow typeDef;
        SZrArtifactLayoutRow layout;
        TZrUInt32 memberCount = artifact_metadata_input_count_members(
                document, state->typeToken);
        TZrUInt32 propertyCount = artifact_metadata_input_count_properties(
                document, state->typeToken);
        TZrUInt32 recordCount = artifact_metadata_input_count_records(
                document, state->typeToken);
        EZrArtifactStatus status;

        if ((index > 0u &&
             ((const SZrArtifactMetadataStateRow *)states->data)[index - 1u].typeToken >=
                     state->typeToken) ||
            !artifact_metadata_input_find_type_def(document, state->typeToken, &typeDef) ||
            !artifact_metadata_state_shape_is_valid(state, &typeDef) ||
            !artifact_metadata_input_find_layout(document, state->typeToken, &layout) ||
            layout.layoutHash != state->layoutHash ||
            !artifact_metadata_input_has_contract_hash(
                    document, state->callableContractHash) ||
            memberCount != state->retainedMemberCount ||
            propertyCount != state->retainedPropertyCount ||
            recordCount != state->retainedMetaRecordCount ||
            (state->preservationState ==
                     ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
             (memberCount != 0u || propertyCount != 0u || recordCount != 0u)) ||
            (state->preservationState == ZR_ARTIFACT_METADATA_PRESERVATION_MEMBERS &&
             recordCount != 0u) ||
            (state->preservationState !=
                     ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
             (typeDef.flags & ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE) != 0u &&
             !artifact_metadata_input_find_member_owner(
                     document, typeDef.constructorToken, ZR_NULL))) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
                    index,
                    0u);
        }
        status = artifact_metadata_validate_layout_map(
                mapHeap != ZR_NULL ? (const TZrByte *)mapHeap->data : ZR_NULL,
                mapHeap != ZR_NULL ? mapHeap->elementCount : 0u,
                &layout,
                index,
                diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    for (index = 0u; records != ZR_NULL && index < records->elementCount; ++index) {
        const SZrArtifactMetadataRecordRow *record =
                &((const SZrArtifactMetadataRecordRow *)records->data)[index];
        TZrMetadataToken ownerTypeToken = record->ownerToken;
        TZrUInt32 stateIndex;
        const SZrArtifactMetadataStateRow *ownerState = ZR_NULL;

        if (ZR_METADATA_TOKEN_TABLE(ownerTypeToken) == ZR_METADATA_TABLE_MEMBER_DEF) {
            if (!artifact_metadata_input_find_member_owner(
                        document, ownerTypeToken, &ownerTypeToken)) {
                return zr_artifact_fail(
                        diagnostic,
                        ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                        ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
                        index,
                        0u);
            }
        } else if (!artifact_metadata_input_find_type_def(
                           document, ownerTypeToken, ZR_NULL)) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
                    index,
                    0u);
        }
        for (stateIndex = 0u; stateIndex < states->elementCount; ++stateIndex) {
            const SZrArtifactMetadataStateRow *candidate =
                    &((const SZrArtifactMetadataStateRow *)states->data)[stateIndex];
            if (candidate->typeToken == ownerTypeToken) {
                ownerState = candidate;
                break;
            }
        }
        if (ownerState == ZR_NULL ||
            ownerState->preservationState != ZR_ARTIFACT_METADATA_PRESERVATION_FULL ||
            ownerState->metadataGeneration != record->metadataGeneration ||
            !artifact_metadata_record_shape_is_valid(
                    record,
                    blob != ZR_NULL ? (const TZrByte *)blob->data : ZR_NULL,
                    blob != ZR_NULL ? blob->elementCount : 0u)) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
                    index,
                    record->payloadOffset);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static TZrBool artifact_metadata_decoded_find_type_def(
        const SZrArtifactView *view,
        TZrMetadataToken token,
        SZrArtifactTypeDefRow *outRow) {
    SZrArtifactSectionView section;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactTypeDefRow row;
        ZrCore_Artifact_ReadTypeDefRow(&section, index, &row, ZR_NULL);
        if (row.token == token) {
            if (outRow != ZR_NULL) *outRow = row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_decoded_has_type_token(
        const SZrArtifactView *view,
        TZrMetadataToken token) {
    SZrArtifactSectionView section;
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    TZrUInt32 index;

    if (table == ZR_METADATA_TABLE_TYPE_DEF) {
        return artifact_metadata_decoded_find_type_def(view, token, ZR_NULL);
    }
    if (table != ZR_METADATA_TABLE_TYPE_REF &&
        table != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }
    if (ZrCore_Artifact_FindSection(
                view,
                table == ZR_METADATA_TABLE_TYPE_REF
                        ? ZR_ARTIFACT_SECTION_TYPE_REF_TABLE
                        : ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactTypeIdentityRow row;
        ZrCore_Artifact_ReadTypeIdentityRow(&section, index, &row, ZR_NULL);
        if (row.token == token) return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_decoded_find_member_owner(
        const SZrArtifactView *view,
        TZrMetadataToken memberToken,
        TZrMetadataToken *outOwnerToken) {
    SZrArtifactSectionView section;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
                &section,
                ZR_NULL) == ZR_ARTIFACT_STATUS_OK) {
        for (index = 0u; index < section.elementCount; ++index) {
            SZrArtifactMemberDefRow row;
            ZrCore_Artifact_ReadMemberDefRow(&section, index, &row, ZR_NULL);
            if (row.token == memberToken) {
                if (outOwnerToken != ZR_NULL) *outOwnerToken = row.ownerTypeToken;
                return ZR_TRUE;
            }
        }
    }
    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE,
                &section,
                ZR_NULL) == ZR_ARTIFACT_STATUS_OK) {
        for (index = 0u; index < section.elementCount; ++index) {
            SZrArtifactPropertyDefRow row;
            ZrCore_Artifact_ReadPropertyDefRow(&section, index, &row, ZR_NULL);
            if (row.token == memberToken) {
                if (outOwnerToken != ZR_NULL) *outOwnerToken = row.ownerTypeToken;
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_decoded_member_has_owner(
        const SZrArtifactView *view,
        TZrMetadataToken memberToken,
        TZrMetadataToken expectedOwnerToken) {
    TZrMetadataToken actualOwnerToken = 0u;

    return (TZrBool)(artifact_metadata_decoded_find_member_owner(
                             view, memberToken, &actualOwnerToken) &&
                     actualOwnerToken == expectedOwnerToken);
}

static TZrBool artifact_metadata_decoded_find_layout(
        const SZrArtifactView *view,
        TZrMetadataToken typeToken,
        SZrArtifactLayoutRow *outRow) {
    SZrArtifactSectionView section;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactLayoutRow row;
        ZrCore_Artifact_ReadLayoutRow(&section, index, &row, ZR_NULL);
        if (row.typeToken == typeToken) {
            if (outRow != ZR_NULL) *outRow = row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool artifact_metadata_decoded_has_contract_hash(
        const SZrArtifactView *view,
        TZrUInt64 contractHash) {
    SZrArtifactSectionView section;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactContractRow row;
        ZrCore_Artifact_ReadContractRow(&section, index, &row, ZR_NULL);
        if (row.contractHash == contractHash) return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrUInt32 artifact_metadata_decoded_count_members(
        const SZrArtifactView *view,
        TZrMetadataToken typeToken) {
    SZrArtifactSectionView section;
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return 0u;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactMemberDefRow row;
        ZrCore_Artifact_ReadMemberDefRow(&section, index, &row, ZR_NULL);
        if (row.ownerTypeToken == typeToken) ++count;
    }
    return count;
}

static TZrUInt32 artifact_metadata_decoded_count_properties(
        const SZrArtifactView *view,
        TZrMetadataToken typeToken) {
    SZrArtifactSectionView section;
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return 0u;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactPropertyDefRow row;
        ZrCore_Artifact_ReadPropertyDefRow(&section, index, &row, ZR_NULL);
        if (row.ownerTypeToken == typeToken) ++count;
    }
    return count;
}

static TZrUInt32 artifact_metadata_decoded_count_records(
        const SZrArtifactView *view,
        TZrMetadataToken typeToken) {
    SZrArtifactSectionView section;
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
                &section,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return 0u;
    }
    for (index = 0u; index < section.elementCount; ++index) {
        SZrArtifactMetadataRecordRow record;
        TZrMetadataToken ownerTypeToken;
        ZrCore_Artifact_ReadMetadataRecordRow(&section, index, &record, ZR_NULL);
        ownerTypeToken = record.ownerToken;
        if (ZR_METADATA_TOKEN_TABLE(ownerTypeToken) == ZR_METADATA_TABLE_MEMBER_DEF &&
            !artifact_metadata_decoded_find_member_owner(
                    view, ownerTypeToken, &ownerTypeToken)) {
            continue;
        }
        if (ownerTypeToken == typeToken) ++count;
    }
    return count;
}

static EZrArtifactStatus artifact_metadata_validate_decoded_links(
        const SZrArtifactView *view,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSectionView section;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
                &section,
                ZR_NULL) == ZR_ARTIFACT_STATUS_OK) {
        for (index = 0u; index < section.elementCount; ++index) {
            SZrArtifactMemberDefRow row;
            ZrCore_Artifact_ReadMemberDefRow(&section, index, &row, ZR_NULL);
            if (!artifact_metadata_decoded_has_type_token(view, row.ownerTypeToken)) {
                return zr_artifact_fail(
                        diagnostic,
                        ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                        section.kind,
                        index,
                        section.byteOffset + index * section.elementSize);
            }
        }
    }
    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE,
                &section,
                ZR_NULL) == ZR_ARTIFACT_STATUS_OK) {
        for (index = 0u; index < section.elementCount; ++index) {
            SZrArtifactPropertyDefRow row;
            ZrCore_Artifact_ReadPropertyDefRow(&section, index, &row, ZR_NULL);
            if (!artifact_metadata_decoded_has_type_token(view, row.ownerTypeToken) ||
                (row.getterToken != 0u &&
                 !artifact_metadata_decoded_member_has_owner(
                         view, row.getterToken, row.ownerTypeToken)) ||
                (row.setterToken != 0u &&
                 !artifact_metadata_decoded_member_has_owner(
                         view, row.setterToken, row.ownerTypeToken)) ||
                (row.initializerToken != 0u &&
                 !artifact_metadata_decoded_member_has_owner(
                         view, row.initializerToken, row.ownerTypeToken))) {
                return zr_artifact_fail(
                        diagnostic,
                        ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                        section.kind,
                        index,
                        section.byteOffset + index * section.elementSize);
            }
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_metadata_validate_decoded_state(
        const SZrArtifactView *view,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSectionView states;
    SZrArtifactSectionView records;
    SZrArtifactSectionView blob;
    SZrArtifactSectionView mapHeap;
    TZrBool hasRecords;
    TZrBool hasBlob;
    TZrBool hasMapHeap;
    TZrUInt32 index;

    if (ZrCore_Artifact_FindSection(
                view,
                ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
                &states,
                ZR_NULL) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_ARTIFACT_STATUS_OK;
    }
    hasRecords = (TZrBool)(ZrCore_Artifact_FindSection(
            view,
            ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
            &records,
            ZR_NULL) == ZR_ARTIFACT_STATUS_OK);
    hasBlob = (TZrBool)(ZrCore_Artifact_FindSection(
            view,
            ZR_ARTIFACT_SECTION_METADATA_BLOB_HEAP,
            &blob,
            ZR_NULL) == ZR_ARTIFACT_STATUS_OK);
    hasMapHeap = (TZrBool)(ZrCore_Artifact_FindSection(
            view,
            ZR_ARTIFACT_SECTION_LAYOUT_MAP_HEAP,
            &mapHeap,
            ZR_NULL) == ZR_ARTIFACT_STATUS_OK);
    for (index = 0u; index < states.elementCount; ++index) {
        SZrArtifactMetadataStateRow state;
        SZrArtifactMetadataStateRow previous = {0};
        SZrArtifactTypeDefRow typeDef;
        SZrArtifactLayoutRow layout;
        TZrUInt32 memberCount;
        TZrUInt32 propertyCount;
        TZrUInt32 recordCount;
        EZrArtifactStatus status;

        ZrCore_Artifact_ReadMetadataStateRow(&states, index, &state, ZR_NULL);
        if (index > 0u) {
            ZrCore_Artifact_ReadMetadataStateRow(
                    &states, index - 1u, &previous, ZR_NULL);
        }
        memberCount = artifact_metadata_decoded_count_members(view, state.typeToken);
        propertyCount = artifact_metadata_decoded_count_properties(view, state.typeToken);
        recordCount = artifact_metadata_decoded_count_records(view, state.typeToken);
        if ((index > 0u && previous.typeToken >= state.typeToken) ||
            !artifact_metadata_decoded_find_type_def(view, state.typeToken, &typeDef) ||
            !artifact_metadata_state_shape_is_valid(&state, &typeDef) ||
            !artifact_metadata_decoded_find_layout(view, state.typeToken, &layout) ||
            layout.layoutHash != state.layoutHash ||
            !artifact_metadata_decoded_has_contract_hash(
                    view, state.callableContractHash) ||
            memberCount != state.retainedMemberCount ||
            propertyCount != state.retainedPropertyCount ||
            recordCount != state.retainedMetaRecordCount ||
            (state.preservationState ==
                     ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
             (memberCount != 0u || propertyCount != 0u || recordCount != 0u)) ||
            (state.preservationState == ZR_ARTIFACT_METADATA_PRESERVATION_MEMBERS &&
             recordCount != 0u) ||
            (state.preservationState !=
                     ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
             (typeDef.flags & ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE) != 0u &&
             !artifact_metadata_decoded_find_member_owner(
                     view, typeDef.constructorToken, ZR_NULL))) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    states.kind,
                    index,
                    states.byteOffset + index * states.elementSize);
        }
        status = artifact_metadata_validate_layout_map(
                hasMapHeap ? mapHeap.data : ZR_NULL,
                hasMapHeap ? mapHeap.byteLength : 0u,
                &layout,
                index,
                diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    for (index = 0u; hasRecords && index < records.elementCount; ++index) {
        SZrArtifactMetadataRecordRow record;
        SZrArtifactMetadataStateRow ownerState = {0};
        TZrMetadataToken ownerTypeToken;
        TZrUInt32 stateIndex;
        TZrBool foundState = ZR_FALSE;

        ZrCore_Artifact_ReadMetadataRecordRow(&records, index, &record, ZR_NULL);
        ownerTypeToken = record.ownerToken;
        if (ZR_METADATA_TOKEN_TABLE(ownerTypeToken) == ZR_METADATA_TABLE_MEMBER_DEF) {
            if (!artifact_metadata_decoded_find_member_owner(
                        view, ownerTypeToken, &ownerTypeToken)) {
                return zr_artifact_fail(
                        diagnostic,
                        ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                        records.kind,
                        index,
                        records.byteOffset + index * records.elementSize);
            }
        } else if (!artifact_metadata_decoded_find_type_def(
                           view, ownerTypeToken, ZR_NULL)) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    records.kind,
                    index,
                    records.byteOffset + index * records.elementSize);
        }
        for (stateIndex = 0u; stateIndex < states.elementCount; ++stateIndex) {
            ZrCore_Artifact_ReadMetadataStateRow(
                    &states, stateIndex, &ownerState, ZR_NULL);
            if (ownerState.typeToken == ownerTypeToken) {
                foundState = ZR_TRUE;
                break;
            }
        }
        if (!foundState ||
            ownerState.preservationState != ZR_ARTIFACT_METADATA_PRESERVATION_FULL ||
            ownerState.metadataGeneration != record.metadataGeneration ||
            !artifact_metadata_record_shape_is_valid(
                    &record,
                    hasBlob ? blob.data : ZR_NULL,
                    hasBlob ? blob.byteLength : 0u)) {
            return zr_artifact_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    records.kind,
                    index,
                    records.byteOffset + index * records.elementSize);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus zr_artifact_metadata_graph_validate_input(
        const SZrArtifactDocument *document,
        SZrArtifactDiagnostic *diagnostic) {
    EZrArtifactStatus status =
            artifact_metadata_validate_input_links(document, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    return artifact_metadata_validate_input_state(document, diagnostic);
}

EZrArtifactStatus zr_artifact_metadata_graph_validate_decoded(
        const SZrArtifactView *view,
        SZrArtifactDiagnostic *diagnostic) {
    EZrArtifactStatus status =
            artifact_metadata_validate_decoded_links(view, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    return artifact_metadata_validate_decoded_state(view, diagnostic);
}
