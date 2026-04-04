#include "parser_internal.h"

void ZrParser_State_Init(SZrParserState *ps, SZrState *state, const TZrChar *source, TZrSize sourceLength,
                         SZrString *sourceName) {
    ZR_ASSERT(ps != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    ps->state = state;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;
    ps->errorCallback = ZR_NULL;
    ps->errorUserData = ZR_NULL;
    ps->suppressErrorOutput = ZR_FALSE;

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

// 清理解析器状态

void ZrParser_State_Free(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return;
    }

    if (ps->lexer != ZR_NULL) {
        // 释放词法分析器的缓冲区
        if (ps->lexer->buffer != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(ps->state->global, ps->lexer->buffer, ps->lexer->bufferSize,
                                          ZR_MEMORY_NATIVE_TYPE_STRING);
        }
        ZrCore_Memory_RawFreeWithType(ps->state->global, ps->lexer, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
    }
}

// 期望特定 token

SZrAstNode *parse_script(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析可选的模块声明
    SZrAstNode *moduleName = ZR_NULL;
    if (current_percent_directive_equals(ps, "module")) {
        moduleName = parse_module_declaration(ps);
    }

    // 解析语句列表
    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    TZrSize stmtCount = 0;
    TZrSize errorCount = 0;
    while (ps->lexer->t.token != ZR_TK_EOS) {
        // 保存错误状态
        ZR_UNUSED_PARAMETER(ps->hasError);
        ZR_UNUSED_PARAMETER(ps->errorMessage);

        // 重置错误状态（临时）
        ps->hasError = ZR_FALSE;
        ps->errorMessage = ZR_NULL;

        SZrAstNode *stmt = parse_top_level_statement(ps);
        if (stmt != ZR_NULL) {
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
                        fprintf(stderr, "  Too many consecutive errors (%zu), stopping parse\n", errorCount);
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
                    // 如果遇到 %，需要特殊处理（可能是 %test 或 %compileTime）
                    if (token == ZR_TK_PERCENT) {
                        // 检查下一个 token 是否是标识符
                        EZrToken nextToken = peek_token(ps);
                        if (nextToken == ZR_TK_IDENTIFIER) {
                            // 可能是 %test 或 %compileTime，停止跳过
                            break;
                        } else {
                            // 不是有效的指令，跳过 % token
                            ZrParser_Lexer_Next(ps->lexer);
                            skipCount++;
                            continue;
                        }
                    }
                    // 跳过当前 token
                    ZrParser_Lexer_Next(ps->lexer);
                    skipCount++;
                }
            } else {
                // 没有错误但返回 NULL，可能是遇到了不支持的语法
                EZrToken currentToken = ps->lexer->t.token;
                if (!ps->suppressErrorOutput) {
                    fprintf(stderr, "  Warning: Failed to parse statement %zu (token: %d), skipping\n", stmtCount,
                            currentToken);
                }
                // 尝试跳过当前 token 继续解析
                if (currentToken != ZR_TK_EOS) {
                    // 特殊处理 % token：如果后面不是有效的指令，需要跳过
                    if (currentToken == ZR_TK_PERCENT) {
                        EZrToken nextToken = peek_token(ps);
                        if (nextToken != ZR_TK_IDENTIFIER) {
                            // 不是有效的指令，跳过 % token
                            ZrParser_Lexer_Next(ps->lexer);
                        } else {
                            // 可能是有效的指令，但解析失败了，跳过 % 和标识符
                            ZrParser_Lexer_Next(ps->lexer); // 跳过 %
                            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                                ZrParser_Lexer_Next(ps->lexer); // 跳过标识符
                            }
                        }
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
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.script.moduleName = moduleName;
    node->data.script.statements = statements;
    return node;
}

SZrAstNode *ZrParser_ParseWithState(SZrParserState *ps) {
    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL || ps->hasError) {
        return ZR_NULL;
    }

    return parse_script(ps);
}

// 解析源代码，返回 AST 根节点

SZrAstNode *ZrParser_Parse(SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName) {
    SZrParserState ps;
    SZrAstNode *ast;

    ZrParser_State_Init(&ps, state, source, sourceLength, sourceName);
    ast = ZrParser_ParseWithState(&ps);
    ZrParser_State_Free(&ps);
    return ast;
}
