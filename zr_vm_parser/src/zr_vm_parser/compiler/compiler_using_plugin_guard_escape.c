#include "compiler_using_plugin_guard_escape_internal.h"

#include <stdio.h>

/**
 * @brief 按字符串身份或内容判断名称是否已进入扫描集合。
 * @note 身份比较只作快速路径；不同字符串对象仍按内容匹配。
 */
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

/**
 * @brief 将尚未出现的名称指针加入扫描集合。
 * @pre names 已由当前扫描初始化，scan->cs->state 在同步扫描期间有效。
 * @note 数组只保存 AST 所有的借用名称指针；释放集合只释放数组缓冲区，不释放字符串。
 * BUG: Array_Init/扩容分配失败会留下空 head，而下方无状态返回的 Array_Push 会断言或向空地址复制。
 */
void plugin_guard_push_name_unique(SZrPluginGuardEscapeScan *scan,
                                   SZrArray *names,
                                   SZrString *name) {
    if (scan == ZR_NULL || scan->cs == ZR_NULL || names == ZR_NULL || name == ZR_NULL ||
        plugin_guard_name_array_contains(names, name)) {
        return;
    }

    ZrCore_Array_Push(scan->cs->state, names, &name);
}

/**
 * @brief 把越界类别写入普通编译错误，并让递归扫描立即失败。
 * @note reason 来自扫描器的固定边界标签；空标签使用通用 guard 范围说明。
 */
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
                 "plugin_type_escape: plugin guard value cannot escape through %s",
                 reason);
    } else {
        snprintf(message,
                 sizeof(message),
                 "plugin_type_escape: plugin guard value cannot escape its guard scope");
    }
    ZrParser_Compiler_Error(scan->cs, message, location);
    return ZR_FALSE;
}

/**
 * @brief 从受支持的简单绑定表达式取名称，供扫描集合记录变量身份。
 * @return 标识符或无成员 primary 的借用名称；其他表达式形状返回 ZR_NULL。
 * @note 返回值仍由输入 AST 持有，调用方不得在 AST 生命周期外保留。
 */
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

/**
 * @brief 扫描 guard 正文，并在成功或诊断失败后统一释放三组临时名称数组。
 * @pre scan->cs 与 state 有效；数组由入口完成初始化；AST 和其中的名称在本次同步遍历期间存活。
 * @note 仅在 pluginNames 非空时遍历 body；elseBody 不在插件绑定的有效范围内。名称与 moduleName 均为借用值。
 */
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
    /* 模块名只借用字面量 AST 值；编译入口随后仍负责验证 import 路径形状。 */
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
    /* 调用方解析出的绑定节点仅按支持的裸名形状加入集合；复杂节点不作为名称来源。 */
    for (TZrSize index = 0; index < bindings->count; index++) {
        SZrString *bindingName = plugin_guard_bare_identifier_name(bindings->nodes[index]);
        if (bindingName != ZR_NULL) {
            plugin_guard_push_name_unique(&scan, &scan.localNames, bindingName);
            plugin_guard_push_name_unique(&scan, &scan.pluginNames, bindingName);
        }
    }
    return plugin_guard_validate_scan(&scan, stmt);
}
