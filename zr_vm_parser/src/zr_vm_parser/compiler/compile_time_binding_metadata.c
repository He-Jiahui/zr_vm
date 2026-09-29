#include "compile_time_binding_metadata.h"

#include <string.h>

/**
 * @brief 读取可用于静态路径键比较的原生文本。
 * @note 空字符串对象或无法取得原生视图时按空路径片段处理；调用者只应传可存活的编译期字符串。
 */
static const TZrChar *binding_string_or_empty(SZrString *value) {
    const TZrChar *text = value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
    return text != ZR_NULL ? text : "";
}

/**
 * @brief 在 parser state 管辖的字符串堆中复制文本，返回供 binding metadata 持有的字符串。
 * @pre outString 是可写输出；text 可空，空文本仍会产生一个有效字符串对象。
 * @return 分配成功时返回 true，失败时 false；所有权交给 state/GC 管理。
 */
static TZrBool binding_make_string(SZrState *state, const TZrChar *text, SZrString **outString) {
    TZrSize length;

    if (state == ZR_NULL || outString == ZR_NULL) {
        return ZR_FALSE;
    }

    if (text == ZR_NULL) {
        text = "";
    }

    length = strlen(text);
    *outString = ZrCore_String_Create(state, (TZrNativeString)text, length);
    return *outString != ZR_NULL;
}

/**
 * @brief 把嵌套对象的静态属性路径接到已有前缀上，统一使用点分隔路径。
 * @note 空前缀/后缀直接复制另一段；非空两段经临时原生缓冲区拼接，最终字符串归 state 管理。
 * @return 状态、缓冲区或字符串分配失败时返回 false，不发布部分路径。
 */
static TZrBool binding_join_path(SZrState *state, SZrString *prefix, SZrString *suffix, SZrString **outPath) {
    const TZrChar *prefixText;
    const TZrChar *suffixText;
    TZrSize prefixLength;
    TZrSize suffixLength;
    TZrSize totalLength;
    TZrChar *buffer;
    TZrBool ok;

    if (state == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }

    prefixText = binding_string_or_empty(prefix);
    suffixText = binding_string_or_empty(suffix);
    prefixLength = strlen(prefixText);
    suffixLength = strlen(suffixText);
    if (prefixLength == 0) {
        return binding_make_string(state, suffixText, outPath);
    }
    if (suffixLength == 0) {
        return binding_make_string(state, prefixText, outPath);
    }

    totalLength = prefixLength + 1 + suffixLength;
    buffer = (TZrChar *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        totalLength + 1,
                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(buffer, prefixText, prefixLength);
    buffer[prefixLength] = '.';
    memcpy(buffer + prefixLength + 1, suffixText, suffixLength);
    buffer[totalLength] = '\0';
    ok = binding_make_string(state, buffer, outPath);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  buffer,
                                  totalLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    return ok;
}

/**
 * @brief 仅把静态对象键规范化为属性名，供 compile-time binding 路径收集使用。
 * @return identifier/string literal 返回 AST 所有的字符串；动态表达式键返回 false。
 */
static TZrBool binding_extract_property_name(SZrState *state, SZrAstNode *keyNode, SZrString **outName) {
    if (state == ZR_NULL || keyNode == ZR_NULL || outName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (keyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outName = keyNode->data.identifier.name;
        return *outName != ZR_NULL;
    }

    if (keyNode->type == ZR_AST_STRING_LITERAL) {
        *outName = keyNode->data.stringLiteral.value;
        return *outName != ZR_NULL;
    }

    *outName = ZR_NULL;
    return ZR_FALSE;
}

/**
 * @brief 将一条“路径 -> compile-time 函数”映射追加到当前收集数组。
 * @note 数组复制记录本身，path/targetName 字符串仍由 AST、state 或来源 metadata 保持存活。
 * BUG: 顶层变量 initializer 是 identifier 或无 member 的函数 primary 时，将 prefix=NULL 原样写入 path；导入侧用空字符串查询且 FindPath 跳过 NULL path，故函数别名无法查回。
 * @return 参数不完整或目标种类为空时返回 false；数组本身由调用方初始化并拥有。
 */
static TZrBool binding_append(SZrState *state,
                              SZrArray *bindings,
                              SZrString *path,
                              TZrUInt8 targetKind,
                              SZrString *targetName) {
    SZrFunctionCompileTimePathBinding binding;

    if (state == ZR_NULL || bindings == ZR_NULL || targetKind == ZR_COMPILE_TIME_BINDING_TARGET_NONE ||
        targetName == ZR_NULL) {
        return ZR_FALSE;
    }

    binding.path = path;
    binding.targetKind = targetKind;
    binding.targetName = targetName;
    ZrCore_Array_Push(state, bindings, &binding);
    return ZR_TRUE;
}

/**
 * @brief 复制变量已解析的路径映射，并在嵌套对象边界前加上本次访问前缀。
 * @note 来源 binding 的 target identity 不变；只重建 path 字符串，供外层变量继续组合。
 */
static TZrBool binding_append_prefixed_bindings(SZrCompileTimeBindingResolver *resolver,
                                                SZrArray *bindings,
                                                SZrString *prefix,
                                                const SZrFunctionCompileTimeVariableInfo *info) {
    if (resolver == ZR_NULL || bindings == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < info->pathBindingCount; index++) {
        const SZrFunctionCompileTimePathBinding *sourceBinding = &info->pathBindings[index];
        SZrString *joinedPath = ZR_NULL;

        if (!binding_join_path(resolver->state, prefix, sourceBinding->path, &joinedPath) ||
            !binding_append(resolver->state, bindings, joinedPath, sourceBinding->targetKind, sourceBinding->targetName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/**
 * @brief 判断完整路径是否落在某个属性前缀下，并返回相对尾段。
 * @note 精确命中产生空 suffix；非精确命中必须紧随点分隔符，避免 `a` 匹配 `ab`。
 */
static TZrBool binding_path_matches_prefix(SZrString *path, SZrString *prefix, SZrString **outSuffix, SZrState *state) {
    const TZrChar *pathText;
    const TZrChar *prefixText;
    TZrSize prefixLength;

    if (outSuffix == ZR_NULL || state == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSuffix = ZR_NULL;
    pathText = binding_string_or_empty(path);
    prefixText = binding_string_or_empty(prefix);
    prefixLength = strlen(prefixText);
    if (prefixLength == 0) {
        return binding_make_string(state, pathText, outSuffix);
    }

    /* 只有完整字段边界才拆出 suffix，不能把同前缀的相邻字段误认成子树。 */
    if (strcmp(pathText, prefixText) == 0) {
        return binding_make_string(state, "", outSuffix);
    }

    if (strncmp(pathText, prefixText, prefixLength) != 0 || pathText[prefixLength] != '.') {
        return ZR_FALSE;
    }

    return binding_make_string(state, pathText + prefixLength + 1, outSuffix);
}

/* 下列 mutually-recursive 收集器通过 resolver 回查同组变量的 compile-time initializer。 */
static TZrBool binding_collect_from_node(SZrCompileTimeBindingResolver *resolver,
                                         SZrAstNode *node,
                                         SZrString *prefix,
                                         SZrArray *bindings);

/**
 * @brief 惰性解析一个来源变量的 initializer，把其中可静态定位的 compile-time 函数存入 info。
 * @pre variable->info 是本次构建期间由调用方拥有的输出记录；state 与非空查找 callback 在调用期间有效。
 * @return 收集或输出分配失败时返回 false；成功后 isResolved 表示结果已稳定。
 * @note isResolving 用于打断循环依赖递归；遇到当前栈上的同一变量时返回成功但不制造虚构绑定。
 */
static TZrBool binding_resolve_source_variable(SZrCompileTimeBindingResolver *resolver,
                                               SZrCompileTimeBindingSourceVariable *variable) {
    SZrArray collectedBindings;

    if (resolver == ZR_NULL || resolver->state == ZR_NULL || variable == ZR_NULL || variable->info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (variable->isResolved) {
        return ZR_TRUE;
    }
    /* 循环引用在当前路径上先视作无新增结论，避免无限递归；外层仍完成本地收集。 */
    if (variable->isResolving) {
        return ZR_TRUE;
    }

    /* 先标记正在解析并清空输出；递归收集完成前不向消费者发布半成品表。 */
    variable->isResolving = ZR_TRUE;
    variable->info->pathBindings = ZR_NULL;
    variable->info->pathBindingCount = 0;
    ZrCore_Array_Init(resolver->state,
                      &collectedBindings,
                      sizeof(SZrFunctionCompileTimePathBinding),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);

    if (variable->value != ZR_NULL &&
        !binding_collect_from_node(resolver, variable->value, ZR_NULL, &collectedBindings)) {
        if (collectedBindings.isValid && collectedBindings.head != ZR_NULL) {
            ZrCore_Array_Free(resolver->state, &collectedBindings);
        }
        variable->isResolving = ZR_FALSE;
        return ZR_FALSE;
    }

    /* 临时数组只作构建缓冲；成功时把条目复制到 info 的函数级 metadata 所有权区。 */
    if (collectedBindings.length > 0) {
        variable->info->pathBindings = (SZrFunctionCompileTimePathBinding *)ZrCore_Memory_RawMallocWithType(
                resolver->state->global,
                sizeof(SZrFunctionCompileTimePathBinding) * collectedBindings.length,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (variable->info->pathBindings == ZR_NULL) {
            ZrCore_Array_Free(resolver->state, &collectedBindings);
            variable->isResolving = ZR_FALSE;
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(variable->info->pathBindings,
                              collectedBindings.head,
                              sizeof(SZrFunctionCompileTimePathBinding) * collectedBindings.length);
        variable->info->pathBindingCount = (TZrUInt32)collectedBindings.length;
    }

    if (collectedBindings.isValid && collectedBindings.head != ZR_NULL) {
        ZrCore_Array_Free(resolver->state, &collectedBindings);
    }
    variable->isResolving = ZR_FALSE;
    variable->isResolved = ZR_TRUE;
    return ZR_TRUE;
}

/**
 * @brief 解析裸 identifier：优先识别函数目标，否则追踪同名 compile-time 变量的嵌套映射。
 * @note 不在 resolver 中的普通运行时标识符对 metadata 不贡献映射，且不算解析失败。
 */
static TZrBool binding_collect_from_identifier(SZrCompileTimeBindingResolver *resolver,
                                               SZrString *identifier,
                                               SZrString *prefix,
                                               SZrArray *bindings) {
    SZrCompileTimeBindingSourceVariable *variable;
    SZrCompileTimeFunction *functionInfo;

    if (resolver == ZR_NULL || identifier == ZR_NULL || bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    functionInfo = resolver->findFunction != ZR_NULL ? resolver->findFunction(resolver->userData, identifier) : ZR_NULL;
    /* 名称先归属函数表；仅非函数名称才可能代表需展开的 compile-time 变量。 */
    if (functionInfo != ZR_NULL) {
        return binding_append(resolver->state,
                              bindings,
                              prefix,
                              ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION,
                              functionInfo->name);
    }

    variable = resolver->findVariable != ZR_NULL ? resolver->findVariable(resolver->userData, identifier) : ZR_NULL;
    if (variable == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!binding_resolve_source_variable(resolver, variable)) {
        return ZR_FALSE;
    }

    return binding_append_prefixed_bindings(resolver, bindings, prefix, variable->info);
}

/**
 * @brief 处理静态 member 链；支持 compile-time function 本身和对象变量中的嵌套函数路径。
 * @pre primary 与 members 来自同一 initializer AST，prefix 表示当前外层对象字段。
 * @note Computed/call 等无法静态序列化的访问不产出路径绑定，不改变普通表达式的执行语义。
 */
static TZrBool binding_collect_from_primary(SZrCompileTimeBindingResolver *resolver,
                                            SZrPrimaryExpression *primary,
                                            SZrString *prefix,
                                            SZrArray *bindings) {
    SZrString *rootName;
    SZrString *requestedPath = ZR_NULL;
    SZrCompileTimeBindingSourceVariable *variable;
    SZrCompileTimeFunction *functionInfo;

    if (resolver == ZR_NULL || primary == ZR_NULL || bindings == ZR_NULL || primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL || primary->property->data.identifier.name == ZR_NULL) {
        return ZR_TRUE;
    }

    rootName = primary->property->data.identifier.name;
    if (!ZrParser_CompileTimeBinding_BuildStaticMemberPath(resolver->state,
                                                           primary->members,
                                                           0,
                                                           primary->members != ZR_NULL ? primary->members->count : 0,
                                                           &requestedPath)) {
        return ZR_TRUE;
    }

    functionInfo = resolver->findFunction != ZR_NULL ? resolver->findFunction(resolver->userData, rootName) : ZR_NULL;
    /* module.function 作为叶函数直接绑定；更长访问链不能被误记为函数别名。 */
    if (functionInfo != ZR_NULL) {
        if (strcmp(binding_string_or_empty(requestedPath), "") == 0) {
            return binding_append(resolver->state,
                                  bindings,
                                  prefix,
                                  ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION,
                                  functionInfo->name);
        }
        return ZR_TRUE;
    }

    variable = resolver->findVariable != ZR_NULL ? resolver->findVariable(resolver->userData, rootName) : ZR_NULL;
    if (variable == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!binding_resolve_source_variable(resolver, variable)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < variable->info->pathBindingCount; index++) {
        const SZrFunctionCompileTimePathBinding *sourceBinding = &variable->info->pathBindings[index];
        SZrString *suffix = ZR_NULL;
        SZrString *joinedPath = ZR_NULL;

        if (!binding_path_matches_prefix(sourceBinding->path, requestedPath, &suffix, resolver->state)) {
            continue;
        }

        if (!binding_join_path(resolver->state, prefix, suffix, &joinedPath) ||
            !binding_append(resolver->state,
                            bindings,
                            joinedPath,
                            sourceBinding->targetKind,
                            sourceBinding->targetName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/**
 * @brief 深入静态对象字面量，把每个静态键接入路径后继续收集其值中的函数绑定。
 * @note computed key、无值或非键值元素不进入静态表；未知的动态内容留给原编译期求值路径。
 */
static TZrBool binding_collect_from_object_literal(SZrCompileTimeBindingResolver *resolver,
                                                   SZrObjectLiteral *objectLiteral,
                                                   SZrString *prefix,
                                                   SZrArray *bindings) {
    if (resolver == ZR_NULL || objectLiteral == ZR_NULL || bindings == ZR_NULL || objectLiteral->properties == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < objectLiteral->properties->count; index++) {
        SZrAstNode *propertyNode = objectLiteral->properties->nodes[index];
        SZrString *propertyName = ZR_NULL;
        SZrString *propertyPath = ZR_NULL;

        /* 只投影固定键；computed key 和非键值节点继续由正常 evaluator 决定语义。 */
        if (propertyNode == ZR_NULL || propertyNode->type != ZR_AST_KEY_VALUE_PAIR ||
            propertyNode->data.keyValuePair.value == ZR_NULL ||
            propertyNode->data.keyValuePair.keyIsComputed ||
            !binding_extract_property_name(resolver->state, propertyNode->data.keyValuePair.key, &propertyName)) {
            continue;
        }

        /* TODO: 含 '.' 的 string key 与嵌套字段会拼成同一路径；验证 import 别名查找是否误配。 */
        if (!binding_join_path(resolver->state, prefix, propertyName, &propertyPath) ||
            !binding_collect_from_node(resolver,
                                       propertyNode->data.keyValuePair.value,
                                       propertyPath,
                                       bindings)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/**
 * @brief 限定 metadata 解析只观察 identifier、primary member chain 与对象字面量三种形态。
 * @return 空/无效输入或已支持结构的资源失败返回 false；其余 AST 形态视为没有可收集路径。
 */
static TZrBool binding_collect_from_node(SZrCompileTimeBindingResolver *resolver,
                                         SZrAstNode *node,
                                         SZrString *prefix,
                                         SZrArray *bindings) {
    if (resolver == ZR_NULL || node == ZR_NULL || bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return binding_collect_from_identifier(resolver, node->data.identifier.name, prefix, bindings);
        case ZR_AST_PRIMARY_EXPRESSION:
            return binding_collect_from_primary(resolver, &node->data.primaryExpression, prefix, bindings);
        case ZR_AST_OBJECT_LITERAL:
            return binding_collect_from_object_literal(resolver, &node->data.objectLiteral, prefix, bindings);
        default:
            return ZR_TRUE;
    }
}

/**
 * @brief 对一组来源变量惰性解析并写回各自的 function-path metadata。
 * @pre resolver 提供 state，variables 指向可写连续记录；若提供查找 callbacks，它们须在本次同步调用中有效。
 * @return 首个变量解析失败即返回 false；不创建函数，也不转移来源变量或 AST 的所有权。
 * @note 编译函数 typed metadata 与 source-import module 收集都用它建立可序列化的路径表。
 */
TZrBool ZrParser_CompileTimeBinding_ResolveAll(SZrCompileTimeBindingResolver *resolver,
                                               SZrCompileTimeBindingSourceVariable *variables,
                                               TZrSize variableCount) {
    if (resolver == ZR_NULL || resolver->state == ZR_NULL || variables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < variableCount; index++) {
        if (!binding_resolve_source_variable(resolver, &variables[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/**
 * @brief 将 primary 的指定 member 子范围编码为点分隔静态路径。
 * @pre outPath 可写；members 中被选中的节点必须是 identifier property 的非 computed member。
 * @return 输出为 state 管理的新字符串；空/越界起始范围产出空路径，动态或畸形 member 返回 false。
 * @note compile-time import 与 metadata 收集共用此编码，确保写入表和后续查找使用同一键格式。
 */
TZrBool ZrParser_CompileTimeBinding_BuildStaticMemberPath(SZrState *state,
                                                          const SZrAstNodeArray *members,
                                                          TZrSize startIndex,
                                                          TZrSize endIndex,
                                                          SZrString **outPath) {
    TZrSize totalLength = 0;
    TZrChar *buffer;
    TZrSize cursor = 0;
    TZrBool wroteAny = ZR_FALSE;

    if (state == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }

    *outPath = ZR_NULL;
    if (members == ZR_NULL || startIndex >= endIndex || startIndex >= members->count) {
        return binding_make_string(state, "", outPath);
    }

    if (endIndex > members->count) {
        endIndex = members->count;
    }

    /* 第一遍拒绝动态/异常 member 并计算精确空间；验证失败时不产生可误用的半路径。 */
    for (TZrSize index = startIndex; index < endIndex; index++) {
        SZrAstNode *memberNode = members->nodes[index];
        SZrMemberExpression *memberExpr;
        const TZrChar *memberName;

        if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
            return ZR_FALSE;
        }
        memberExpr = &memberNode->data.memberExpression;
        if (memberExpr->computed || memberExpr->property == ZR_NULL ||
            memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL ||
            memberExpr->property->data.identifier.name == ZR_NULL) {
            return ZR_FALSE;
        }
        memberName = binding_string_or_empty(memberExpr->property->data.identifier.name);
        totalLength += strlen(memberName);
        if (wroteAny) {
            totalLength += 1;
        }
        wroteAny = ZR_TRUE;
    }

    if (!wroteAny) {
        return binding_make_string(state, "", outPath);
    }

    /* 第二遍只执行已验证的 identifier 拼接，避免路径编码器兼任 AST evaluator。 */
    buffer = (TZrChar *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        totalLength + 1,
                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = startIndex; index < endIndex; index++) {
        SZrAstNode *memberNode = members->nodes[index];
        const TZrChar *memberName = binding_string_or_empty(memberNode->data.memberExpression.property->data.identifier.name);
        TZrSize memberLength = strlen(memberName);

        if (cursor > 0) {
            buffer[cursor++] = '.';
        }
        memcpy(buffer + cursor, memberName, memberLength);
        cursor += memberLength;
    }
    buffer[cursor] = '\0';

    wroteAny = binding_make_string(state, buffer, outPath);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  buffer,
                                  totalLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    return wroteAny;
}

/**
 * @brief 在已解析变量 metadata 中按完整静态路径查找一条目标绑定。
 * @pre info/path 是同一导入或编译 metadata 生命周期内的借用对象。
 * @return 命中时返回 info 内部只读记录，未命中或输入为空返回 null；调用者不得释放结果。
 */
const SZrFunctionCompileTimePathBinding *ZrParser_CompileTimeBinding_FindPath(
        const SZrFunctionCompileTimeVariableInfo *info,
        SZrString *path) {
    if (info == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < info->pathBindingCount; index++) {
        const SZrFunctionCompileTimePathBinding *binding = &info->pathBindings[index];
        if (binding->path != ZR_NULL && ZrCore_String_Equal(binding->path, path)) {
            return binding;
        }
    }

    return ZR_NULL;
}
