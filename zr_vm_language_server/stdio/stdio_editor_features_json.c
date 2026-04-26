#include "zr_vm_language_server_stdio_internal.h"

static cJSON *serialize_folding_range(const SZrLspFoldingRange *range) {
    cJSON *json;
    char *kindText;

    if (range == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_START_LINE, range->startLine);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_START_CHARACTER, range->startCharacter);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_END_LINE, range->endLine);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_END_CHARACTER, range->endCharacter);
    kindText = zr_string_to_c_string(range->kind);
    if (kindText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_KIND, kindText);
    }
    free(kindText);
    return json;
}

cJSON *serialize_folding_ranges_array(SZrArray *ranges) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || ranges == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < ranges->length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(ranges, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_folding_range(*rangePtr));
        }
    }

    return json;
}

static cJSON *serialize_selection_range(const SZrLspSelectionRange *range) {
    cJSON *json;

    if (range == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(range->range));
    if (range->hasParent) {
        cJSON *parent = cJSON_CreateObject();
        if (parent != NULL) {
            cJSON_AddItemToObject(parent, ZR_LSP_FIELD_RANGE, serialize_range(range->parentRange));
            if (range->hasGrandParent) {
                cJSON *grandParent = cJSON_CreateObject();
                if (grandParent != NULL) {
                    cJSON_AddItemToObject(grandParent,
                                          ZR_LSP_FIELD_RANGE,
                                          serialize_range(range->grandParentRange));
                    cJSON_AddItemToObject(parent, ZR_LSP_FIELD_PARENT, grandParent);
                }
            }
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_PARENT, parent);
        }
    }
    return json;
}

cJSON *serialize_selection_ranges_array(SZrArray *ranges) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || ranges == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < ranges->length; index++) {
        SZrLspSelectionRange **rangePtr = (SZrLspSelectionRange **)ZrCore_Array_Get(ranges, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_selection_range(*rangePtr));
        }
    }

    return json;
}

static cJSON *serialize_document_link(const SZrLspDocumentLink *link) {
    cJSON *json;
    char *targetText;
    char *tooltipText;

    if (link == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(link->range));
    targetText = zr_string_to_c_string(link->target);
    tooltipText = zr_string_to_c_string(link->tooltip);
    if (targetText != NULL) {
        cJSON *data = cJSON_CreateObject();

        cJSON_AddStringToObject(json, ZR_LSP_FIELD_TARGET, targetText);
        if (data != NULL) {
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_TARGET, targetText);
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_RANGE, serialize_range(link->range));
            if (tooltipText != NULL) {
                cJSON_AddStringToObject(data, ZR_LSP_FIELD_TOOLTIP, tooltipText);
            }
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DATA, data);
        }
    }
    if (tooltipText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_TOOLTIP, tooltipText);
    }

    free(targetText);
    free(tooltipText);
    return json;
}

cJSON *serialize_document_links_array(SZrArray *links) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || links == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < links->length; index++) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(links, index);
        if (linkPtr != ZR_NULL && *linkPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_document_link(*linkPtr));
        }
    }

    return json;
}

static cJSON *serialize_code_lens(const SZrLspCodeLens *lens) {
    cJSON *json;
    cJSON *command;
    cJSON *arguments;
    char *titleText;
    char *commandText;
    char *argumentText;

    if (lens == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    command = cJSON_CreateObject();
    arguments = cJSON_CreateArray();
    if (json == NULL || command == NULL || arguments == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(command);
        cJSON_Delete(arguments);
        return NULL;
    }

    titleText = zr_string_to_c_string(lens->commandTitle);
    commandText = zr_string_to_c_string(lens->command);
    argumentText = zr_string_to_c_string(lens->argument);

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(lens->range));
    cJSON_AddStringToObject(command, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
    cJSON_AddStringToObject(command, ZR_LSP_FIELD_COMMAND, commandText != NULL ? commandText : "");
    if (argumentText != NULL) {
        cJSON_AddItemToArray(arguments, cJSON_CreateString(argumentText));
    }
    if (lens->hasPositionArgument) {
        cJSON *position = cJSON_CreateObject();
        if (position != NULL) {
            cJSON_AddNumberToObject(position, ZR_LSP_FIELD_LINE, lens->positionArgument.line);
            cJSON_AddNumberToObject(position, ZR_LSP_FIELD_CHARACTER, lens->positionArgument.character);
            cJSON_AddItemToArray(arguments, position);
        }
    }
    {
        cJSON *data = cJSON_CreateObject();
        if (data != NULL) {
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_RANGE, serialize_range(lens->range));
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_COMMAND, commandText != NULL ? commandText : "");
            if (argumentText != NULL) {
                cJSON_AddStringToObject(data, ZR_LSP_FIELD_ARGUMENT, argumentText);
            }
            if (lens->hasPositionArgument) {
                cJSON_AddItemToObject(data, ZR_LSP_FIELD_POSITION, serialize_position(lens->positionArgument));
            }
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DATA, data);
        }
    }
    cJSON_AddItemToObject(command, ZR_LSP_FIELD_ARGUMENTS, arguments);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_COMMAND, command);

    free(titleText);
    free(commandText);
    free(argumentText);
    return json;
}

cJSON *serialize_code_lens_array(SZrArray *lenses) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || lenses == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < lenses->length; index++) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(lenses, index);
        if (lensPtr != ZR_NULL && *lensPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_code_lens(*lensPtr));
        }
    }

    return json;
}

static cJSON *serialize_hierarchy_item(const SZrLspHierarchyItem *item) {
    cJSON *json;
    char *nameText;
    char *detailText;
    char *uriText;

    if (item == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    nameText = zr_string_to_c_string(item->name);
    detailText = zr_string_to_c_string(item->detail);
    uriText = zr_string_to_c_string(item->uri);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_NAME, nameText != NULL ? nameText : "");
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, item->kind);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(item->range));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_SELECTION_RANGE, serialize_range(item->selectionRange));
    if (detailText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_DETAIL, detailText);
    }

    free(nameText);
    free(detailText);
    free(uriText);
    return json;
}

cJSON *serialize_hierarchy_items_array(SZrArray *items) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || items == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_hierarchy_item(*itemPtr));
        }
    }

    return json;
}

static cJSON *serialize_ranges_array(SZrArray *ranges) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || ranges == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < ranges->length; index++) {
        SZrLspRange *range = (SZrLspRange *)ZrCore_Array_Get(ranges, index);
        if (range != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_range(*range));
        }
    }

    return json;
}

static cJSON *serialize_hierarchy_call(const SZrLspHierarchyCall *call, TZrBool outgoing) {
    cJSON *json;

    if (call == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json,
                          outgoing ? ZR_LSP_FIELD_TO : ZR_LSP_FIELD_FROM,
                          serialize_hierarchy_item(call->item));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_FROM_RANGES, serialize_ranges_array((SZrArray *)&call->fromRanges));
    return json;
}

cJSON *serialize_hierarchy_calls_array(SZrArray *calls, TZrBool outgoing) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || calls == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < calls->length; index++) {
        SZrLspHierarchyCall **callPtr = (SZrLspHierarchyCall **)ZrCore_Array_Get(calls, index);
        if (callPtr != ZR_NULL && *callPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_hierarchy_call(*callPtr, outgoing));
        }
    }

    return json;
}
