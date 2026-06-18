#include "compiler_using_plugin_guard_escape_internal.h"

#include <stdio.h>

TZrBool plugin_guard_name_array_contains(SZrArray *names, SZrString *name) {
    if (names == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < names->length; index++) {
        SZrString **candidate = (SZrString **)ZrCore_Array_Get(names, index);
        if (candidate != ZR_NULL && *candidate != ZR_NULL &&
            (*candidate == name || ZrCore_String_Equal(*candidate, name))) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void plugin_guard_push_name_unique(SZrPluginGuardEscapeScan *scan,
                                   SZrArray *names,
                                   SZrString *name) {
    if (scan == ZR_NULL || scan->cs == ZR_NULL || names == ZR_NULL || name == ZR_NULL ||
        plugin_guard_name_array_contains(names, name)) {
        return;
    }

    ZrCore_Array_Push(scan->cs->state, names, &name);
}

TZrBool plugin_guard_report_escape(SZrPluginGuardEscapeScan *scan,
                                   SZrFileRange location,
                                   const TZrChar *reason) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (scan == ZR_NULL || scan->cs == ZR_NULL) {
        return ZR_FALSE;
    }

    if (reason != ZR_NULL) {
        snprintf(message,
                 sizeof(message),
                 "plugin_type_escape: plugin guard value cannot escape without share() through %s",
                 reason);
    } else {
        snprintf(message,
                 sizeof(message),
                 "plugin_type_escape: plugin guard value cannot escape without share()");
    }
    ZrParser_Compiler_Error(scan->cs, message, location);
    return ZR_FALSE;
}

SZrString *plugin_guard_bare_identifier_name(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION &&
        node->data.primaryExpression.property != ZR_NULL &&
        node->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
        (node->data.primaryExpression.members == ZR_NULL ||
         node->data.primaryExpression.members->count == 0)) {
        return node->data.primaryExpression.property->data.identifier.name;
    }

    return ZR_NULL;
}

static TZrBool plugin_guard_validate_scan(SZrPluginGuardEscapeScan *scan, SZrUsingStatement *stmt) {
    TZrBool ok = ZR_TRUE;

    if (scan == ZR_NULL || stmt == ZR_NULL) {
        return ZR_TRUE;
    }

    if (scan->pluginNames.length > 0) {
        ok = plugin_guard_scan_statement(scan, stmt->body);
    }

    ZrCore_Array_Free(scan->cs->state, &scan->localNames);
    ZrCore_Array_Free(scan->cs->state, &scan->pluginNames);
    ZrCore_Array_Free(scan->cs->state, &scan->shadowNames);
    return ok;
}

TZrBool ZrParser_Compiler_ValidateUsingPluginGuardEscape(SZrCompilerState *cs, SZrUsingStatement *stmt) {
    SZrPluginGuardEscapeScan scan;
    SZrString *bindingName;
    SZrAstNode *modulePathNode;

    if (cs == ZR_NULL || stmt == ZR_NULL || stmt->pattern == ZR_NULL ||
        stmt->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        stmt->pattern->data.identifier.name == ZR_NULL) {
        return ZR_TRUE;
    }

    bindingName = stmt->pattern->data.identifier.name;
    scan.cs = cs;
    scan.moduleName = ZR_NULL;
    if (stmt->resource != ZR_NULL && stmt->resource->type == ZR_AST_IMPORT_EXPRESSION) {
        modulePathNode = stmt->resource->data.importExpression.modulePath;
        if (modulePathNode != ZR_NULL &&
            modulePathNode->type == ZR_AST_STRING_LITERAL &&
            modulePathNode->data.stringLiteral.value != ZR_NULL) {
            scan.moduleName = modulePathNode->data.stringLiteral.value;
        }
    }
    ZrCore_Array_Init(cs->state, &scan.localNames, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &scan.pluginNames, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &scan.shadowNames, sizeof(SZrString *), 4);
    plugin_guard_push_name_unique(&scan, &scan.localNames, bindingName);
    plugin_guard_push_name_unique(&scan, &scan.pluginNames, bindingName);
    return plugin_guard_validate_scan(&scan, stmt);
}

TZrBool ZrParser_Compiler_ValidateUsingPluginGuardEscapeBindings(SZrCompilerState *cs,
                                                                  SZrUsingStatement *stmt,
                                                                  SZrAstNodeArray *bindings,
                                                                  SZrString *moduleName) {
    SZrPluginGuardEscapeScan scan;

    if (cs == ZR_NULL || stmt == ZR_NULL || bindings == ZR_NULL) {
        return ZR_TRUE;
    }

    scan.cs = cs;
    scan.moduleName = moduleName;
    ZrCore_Array_Init(cs->state, &scan.localNames, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &scan.pluginNames, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &scan.shadowNames, sizeof(SZrString *), 4);
    for (TZrSize index = 0; index < bindings->count; index++) {
        SZrString *bindingName = plugin_guard_bare_identifier_name(bindings->nodes[index]);
        if (bindingName != ZR_NULL) {
            plugin_guard_push_name_unique(&scan, &scan.localNames, bindingName);
            plugin_guard_push_name_unique(&scan, &scan.pluginNames, bindingName);
        }
    }
    return plugin_guard_validate_scan(&scan, stmt);
}
