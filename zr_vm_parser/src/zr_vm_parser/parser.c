#include "parser_internal.h"

// 首次读取 token 在此发生；编译器、LSP 和迁移入口随后配置各自的诊断回调。
void ZrParser_State_Init(SZrParserState *ps, SZrState *state, const TZrChar *source, TZrSize sourceLength,
                         SZrString *sourceName) {
    ZR_ASSERT(ps != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    ps->state = state;
    ps->hasError = ZR_FALSE;
    ps->hasFatalError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;
    ps->errorCallback = ZR_NULL;
    ps->structuredErrorCallback = ZR_NULL;
    ps->errorUserData = ZR_NULL;
    ps->suppressErrorOutput = ZR_FALSE;
    ps->enableLegacyMigrationParsing = ZR_FALSE;

    // 初始化词法分析器
    ps->lexer = ZrCore_Memory_RawMallocWithType(state->global, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
    if (ps->lexer == ZR_NULL) {
        ps->hasError = ZR_TRUE;
        ps->errorMessage = "Failed to allocate lexer state";
        return;
    }

    ZrParser_Lexer_Init(ps->lexer, state, source, sourceLength, sourceName);

    // 初始化当前位置
    SZrFilePosition startPos = ZrParser_FilePosition_Create(0, 1, 1);
    SZrFilePosition endPos = ZrParser_FilePosition_Create(0, 1, 1);
    ps->currentLocation = ZrParser_FileRange_Create(startPos, endPos, sourceName);
}

// 只回收本状态分配的词法器，不接管源文本或已经交给调用方的 AST。

void ZrParser_State_Free(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return;
    }

    if (ps->lexer != ZR_NULL) {
        ZrParser_Lexer_Free(ps->lexer);
        ZrCore_Memory_RawFreeWithType(ps->state->global, ps->lexer, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
        ps->lexer = ZR_NULL;
    }
}

// 可恢复的脚本入口：错误语句通过同步 token 跳过，已成功构造的节点进入脚本数组。

SZrAstNode *parse_script(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析可选的模块声明
    SZrAstNode *moduleName = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_MODULE) {
        moduleName = parse_module_declaration(ps);
    }

    // 解析语句列表
    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    if (statements == ZR_NULL) {
        // BUG: 前面的模块声明若已构造，此失败分支没有释放 moduleName；分配失败时泄漏子树。
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    TZrSize stmtCount = 0;
    TZrSize errorCount = 0;
    while (ps->lexer->t.token != ZR_TK_EOS) {
        // 保存错误状态
        ZR_UNUSED_PARAMETER(ps->hasError);
        ZR_UNUSED_PARAMETER(ps->errorMessage);

        // BUG: `var bad = ; var good = 1;` 中第二轮清除首轮语法错误；无回调的 AST-only
        // 调用方仅检查非空 AST，会接受丢失 bad 声明的脚本（见 module_init_analysis.c）。
        // 重置错误状态（临时）
        ps->hasError = ZR_FALSE;
        ps->errorMessage = ZR_NULL;

        SZrAstNode *stmt = parse_top_level_statement(ps);
        if (stmt != ZR_NULL) {
            // BUG: Add 返回 void 且扩容失败时静默返回；本处仍计数并继续，stmt 泄漏且语句从 AST 消失。
            ZrParser_AstNodeArray_Add(ps->state, statements, stmt);
            stmtCount++;
            errorCount = 0; // 重置错误计数
        } else {
            // 检查是否真的发生了错误
            if (ps->hasError) {
                errorCount++;
                // 错误信息已经在 report_error 中输出，这里只输出统计信息
                // printf("  Parser error at statement %zu (已在上方显示详细信息)\n", stmtCount);

                // 如果连续错误太多，停止解析
                if (errorCount >= ZR_PARSER_MAX_CONSECUTIVE_ERRORS) {
                    if (!ps->suppressErrorOutput) {
                        ZrCore_Log_Diagnosticf(ps->state,
                                               ZR_LOG_LEVEL_ERROR,
                                               ZR_OUTPUT_CHANNEL_STDERR,
                                               "  Too many consecutive errors (%zu), stopping parse\n",
                                               errorCount);
                    }
                    break;
                }

                // 尝试错误恢复：跳过到下一个可能的语句开始位置
                // 跳过当前 token 直到遇到分号、换行或语句开始关键字
                TZrSize skipCount = 0;
                while (ps->lexer->t.token != ZR_TK_EOS && skipCount < ZR_PARSER_MAX_RECOVERY_SKIP_TOKENS) {
                    EZrToken token = ps->lexer->t.token;
                    // 如果遇到分号，跳过它并继续
                    if (token == ZR_TK_SEMICOLON) {
                        ZrParser_Lexer_Next(ps->lexer);
                        break;
                    }
                    // 如果遇到可能的语句开始关键字，停止跳过
                    if (token == ZR_TK_VAR || token == ZR_TK_STRUCT || token == ZR_TK_CLASS || token == ZR_TK_USING ||
                        token == ZR_TK_INTERFACE || token == ZR_TK_ENUM || token == ZR_TK_TEST ||
                        token == ZR_TK_INTERMEDIATE || token == ZR_TK_MODULE || token == ZR_TK_IDENTIFIER) {
                        break;
                    }
                    if (token == ZR_TK_PERCENT) {
                        ZrParser_Lexer_Next(ps->lexer);
                        skipCount++;
                        continue;
                    }
                    // 跳过当前 token
                    ZrParser_Lexer_Next(ps->lexer);
                    skipCount++;
                }
            } else {
                // 没有错误但返回 NULL，可能是遇到了不支持的语法
                EZrToken currentToken = ps->lexer->t.token;
                if (!ps->suppressErrorOutput) {
                    ZrCore_Log_Diagnosticf(ps->state,
                                           ZR_LOG_LEVEL_WARNING,
                                           ZR_OUTPUT_CHANNEL_STDERR,
                                           "  Warning: Failed to parse statement %zu (token: %d), skipping\n",
                                           stmtCount,
                                           currentToken);
                }
                // 尝试跳过当前 token 继续解析
                if (currentToken != ZR_TK_EOS) {
                    if (currentToken == ZR_TK_PERCENT) {
                        ZrParser_Lexer_Next(ps->lexer);
                    } else {
                        ZrParser_Lexer_Next(ps->lexer);
                    }
                }
            }
        }
    }
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange scriptLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_SCRIPT, scriptLoc);
    if (node == ZR_NULL) {
        // BUG: AstNodeArray_Free 只释放容器；先前收集的语句和 moduleName 均未回收。
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.script.moduleName = moduleName;
    node->data.script.statements = statements;
    return node;
}

SZrAstNode *ZrParser_ParseWithState(SZrParserState *ps) {
    SZrAstNode *ast;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL || ps->hasError) {
        return ZR_NULL;
    }

    // 旧语法禁用时，fatal 标志使整棵可恢复 AST 无效；普通错误仍按历史策略返回节点。
    ast = parse_script(ps);
    if (ps->hasFatalError && !ps->enableLegacyMigrationParsing) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, ast);
        }
        return ZR_NULL;
    }
    return ast;
}

TZrBool ZrParser_State_SeekToTokenStart(
        SZrParserState *ps,
        TZrSize sourceOffset) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->hasError ||
        sourceOffset >= ps->lexer->sourceLength) {
        return ZR_FALSE;
    }

    // 增量重解析用顺序扫描寻找精确 token 边界，不复制或回退词法状态。
    while (ps->lexer->t.token != ZR_TK_EOS &&
           ps->lexer->tokenStartOffset < sourceOffset) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    return ps->lexer->t.token != ZR_TK_EOS &&
           ps->lexer->tokenStartOffset == sourceOffset;
}

SZrAstNode *ZrParser_ParseTopLevelStatementWithState(SZrParserState *ps) {
    SZrAstNode *statement;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL ||
        ps->hasError || ps->lexer->t.token == ZR_TK_EOS) {
        return ZR_NULL;
    }

    // 局部重解析只能转移完整且无错误的子树，失败分支自行释放半成品。
    statement = parse_top_level_statement(ps);
    if (statement == ZR_NULL || ps->hasError ||
        (ps->hasFatalError && !ps->enableLegacyMigrationParsing)) {
        if (statement != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, statement);
        }
        return ZR_NULL;
    }

    return statement;
}

// 简单入口把临时词法状态限制在本次调用内，AST 的释放责任仍交给调用者。

SZrAstNode *ZrParser_Parse(SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName) {
    SZrParserState ps;
    SZrAstNode *ast;

    ZrParser_State_Init(&ps, state, source, sourceLength, sourceName);
    ast = ZrParser_ParseWithState(&ps);
    ZrParser_State_Free(&ps);
    return ast;
}
