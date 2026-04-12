//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_object_conf.h"

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/constant_reference.h"
static const TZrChar *get_ast_node_type_name(EZrAstNodeType type) {
    switch (type) {
        case ZR_AST_SCRIPT: return "SCRIPT";
        case ZR_AST_MODULE_DECLARATION: return "MODULE_DECLARATION";
        case ZR_AST_VARIABLE_DECLARATION: return "VARIABLE_DECLARATION";
        case ZR_AST_FUNCTION_DECLARATION: return "FUNCTION_DECLARATION";
        case ZR_AST_EXPRESSION_STATEMENT: return "EXPRESSION_STATEMENT";
        case ZR_AST_ASSIGNMENT_EXPRESSION: return "ASSIGNMENT_EXPRESSION";
        case ZR_AST_BINARY_EXPRESSION: return "BINARY_EXPRESSION";
        case ZR_AST_UNARY_EXPRESSION: return "UNARY_EXPRESSION";
        case ZR_AST_CONDITIONAL_EXPRESSION: return "CONDITIONAL_EXPRESSION";
        case ZR_AST_LOGICAL_EXPRESSION: return "LOGICAL_EXPRESSION";
        case ZR_AST_FUNCTION_CALL: return "FUNCTION_CALL";
        case ZR_AST_MEMBER_EXPRESSION: return "MEMBER_EXPRESSION";
        case ZR_AST_PRIMARY_EXPRESSION: return "PRIMARY_EXPRESSION";
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION: return "PROTOTYPE_REFERENCE_EXPRESSION";
        case ZR_AST_CONSTRUCT_EXPRESSION: return "CONSTRUCT_EXPRESSION";
        case ZR_AST_IDENTIFIER_LITERAL: return "IDENTIFIER";
        case ZR_AST_BOOLEAN_LITERAL: return "BOOLEAN_LITERAL";
        case ZR_AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case ZR_AST_FLOAT_LITERAL: return "FLOAT_LITERAL";
        case ZR_AST_STRING_LITERAL: return "STRING_LITERAL";
        case ZR_AST_CHAR_LITERAL: return "CHAR_LITERAL";
        case ZR_AST_NULL_LITERAL: return "NULL_LITERAL";
        case ZR_AST_ARRAY_LITERAL: return "ARRAY_LITERAL";
        case ZR_AST_OBJECT_LITERAL: return "OBJECT_LITERAL";
        case ZR_AST_BLOCK: return "BLOCK";
        case ZR_AST_RETURN_STATEMENT: return "RETURN_STATEMENT";
        case ZR_AST_IF_EXPRESSION: return "IF_EXPRESSION";
        case ZR_AST_WHILE_LOOP: return "WHILE_LOOP";
        case ZR_AST_FOR_LOOP: return "FOR_LOOP";
        case ZR_AST_FOREACH_LOOP: return "FOREACH_LOOP";
        case ZR_AST_LAMBDA_EXPRESSION: return "LAMBDA_EXPRESSION";
        case ZR_AST_TEST_DECLARATION: return "TEST_DECLARATION";
        case ZR_AST_SWITCH_EXPRESSION: return "SWITCH_EXPRESSION";
        case ZR_AST_SWITCH_CASE: return "SWITCH_CASE";
        case ZR_AST_SWITCH_DEFAULT: return "SWITCH_DEFAULT";
        case ZR_AST_BREAK_CONTINUE_STATEMENT: return "BREAK_CONTINUE_STATEMENT";
        case ZR_AST_THROW_STATEMENT: return "THROW_STATEMENT";
        case ZR_AST_OUT_STATEMENT: return "OUT_STATEMENT";
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: return "TRY_CATCH_FINALLY_STATEMENT";
        case ZR_AST_KEY_VALUE_PAIR: return "KEY_VALUE_PAIR";
        case ZR_AST_UNPACK_LITERAL: return "UNPACK_LITERAL";
        case ZR_AST_GENERATOR_EXPRESSION: return "GENERATOR_EXPRESSION";
        case ZR_AST_DECORATOR_EXPRESSION: return "DECORATOR_EXPRESSION";
        case ZR_AST_DESTRUCTURING_OBJECT: return "DESTRUCTURING_OBJECT";
        case ZR_AST_DESTRUCTURING_ARRAY: return "DESTRUCTURING_ARRAY";
        case ZR_AST_PARAMETER: return "PARAMETER";
        case ZR_AST_PARAMETER_LIST: return "PARAMETER_LIST";
        case ZR_AST_TYPE: return "TYPE";
        case ZR_AST_GENERIC_TYPE: return "GENERIC_TYPE";
        case ZR_AST_TUPLE_TYPE: return "TUPLE_TYPE";
        case ZR_AST_GENERIC_DECLARATION: return "GENERIC_DECLARATION";
        case ZR_AST_STRUCT_DECLARATION: return "STRUCT_DECLARATION";
        case ZR_AST_CLASS_DECLARATION: return "CLASS_DECLARATION";
        case ZR_AST_INTERFACE_DECLARATION: return "INTERFACE_DECLARATION";
        case ZR_AST_ENUM_DECLARATION: return "ENUM_DECLARATION";
        case ZR_AST_INTERMEDIATE_STATEMENT: return "INTERMEDIATE_STATEMENT";
        case ZR_AST_INTERMEDIATE_DECLARATION: return "INTERMEDIATE_DECLARATION";
        case ZR_AST_INTERMEDIATE_CONSTANT: return "INTERMEDIATE_CONSTANT";
        case ZR_AST_INTERMEDIATE_INSTRUCTION: return "INTERMEDIATE_INSTRUCTION";
        case ZR_AST_INTERMEDIATE_INSTRUCTION_PARAMETER: return "INTERMEDIATE_INSTRUCTION_PARAMETER";
        case ZR_AST_STRUCT_FIELD: return "STRUCT_FIELD";
        case ZR_AST_STRUCT_METHOD: return "STRUCT_METHOD";
        case ZR_AST_STRUCT_META_FUNCTION: return "STRUCT_META_FUNCTION";
        case ZR_AST_CLASS_FIELD: return "CLASS_FIELD";
        case ZR_AST_CLASS_METHOD: return "CLASS_METHOD";
        case ZR_AST_CLASS_PROPERTY: return "CLASS_PROPERTY";
        case ZR_AST_CLASS_META_FUNCTION: return "CLASS_META_FUNCTION";
        case ZR_AST_INTERFACE_FIELD_DECLARATION: return "INTERFACE_FIELD_DECLARATION";
        case ZR_AST_INTERFACE_METHOD_SIGNATURE: return "INTERFACE_METHOD_SIGNATURE";
        case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: return "INTERFACE_PROPERTY_SIGNATURE";
        case ZR_AST_INTERFACE_META_SIGNATURE: return "INTERFACE_META_SIGNATURE";
        case ZR_AST_ENUM_MEMBER: return "ENUM_MEMBER";
        case ZR_AST_META_IDENTIFIER: return "META_IDENTIFIER";
        case ZR_AST_ACCESS_MODIFIER: return "ACCESS_MODIFIER";
        case ZR_AST_PROPERTY_GET: return "PROPERTY_GET";
        case ZR_AST_PROPERTY_SET: return "PROPERTY_SET";
        default: return "UNKNOWN";
    }
}

// 注意：由于字符串对象可能已被垃圾回收，我们不在 print_ast_node 中打印字符串内容
// 这可以避免段错误。如果需要打印字符串内容，需要确保字符串对象在打印期间有效

// 递归打印 AST 节点
static void print_ast_node(SZrState *state, FILE *file, SZrAstNode *node, TZrSize indent) {
    if (node == ZR_NULL) {
        for (TZrSize i = 0; i < indent; i++) fprintf(file, "  ");
        fprintf(file, "NULL\n");
        return;
    }
    
    // 打印缩进
    for (TZrSize i = 0; i < indent; i++) fprintf(file, "  ");
    
    // 打印节点类型和位置
    const TZrChar *typeName = get_ast_node_type_name(node->type);
    fprintf(file, "%s", typeName);
    
    // 打印位置信息
    // 注意：如果 source 指向的字符串对象已被释放，这里可能会崩溃
    // 为了安全，我们只打印行号和列号，不打印文件名
    if (node->location.start.line > 0) {
        fprintf(file, " [line:%d:col:%d]", 
                node->location.start.line, node->location.start.column);
    }
    fprintf(file, "\n");
    
    // 根据节点类型打印详细信息
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->moduleName != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "module: ");
                print_ast_node(state, file, script->moduleName, indent + 1);
            }
            if (script->statements != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "statements (%zu):\n", script->statements->count);
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    print_ast_node(state, file, script->statements->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_MODULE_DECLARATION: {
            SZrModuleDeclaration *module = &node->data.moduleDeclaration;
            if (module->name != ZR_NULL) {
                print_ast_node(state, file, module->name, indent + 1);
            }
            break;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *var = &node->data.variableDeclaration;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "pattern: ");
            print_ast_node(state, file, var->pattern, indent + 1);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "value: ");
            print_ast_node(state, file, var->value, indent + 1);
            break;
        }
        case ZR_AST_IDENTIFIER_LITERAL: {
            SZrIdentifier *ident = &node->data.identifier;
            if (ident->name != ZR_NULL) {
                TZrNativeString nameStr = ZrCore_String_GetNativeString(ident->name);
                if (nameStr != ZR_NULL) {
                    fprintf(file, "  name: \"%s\"\n", nameStr);
                } else {
                    fprintf(file, "  name: <null>\n");
                }
            }
            break;
        }
        case ZR_AST_STRING_LITERAL: {
            SZrStringLiteral *str = &node->data.stringLiteral;
            if (str->value != ZR_NULL) {
                TZrNativeString valueStr = ZrCore_String_GetNativeString(str->value);
                if (valueStr != ZR_NULL) {
                    fprintf(file, "  value: \"%s\"\n", valueStr);
                } else {
                    fprintf(file, "  value: <null>\n");
                }
            } else if (str->literal != ZR_NULL) {
                TZrNativeString literalStr = ZrCore_String_GetNativeString(str->literal);
                if (literalStr != ZR_NULL) {
                    fprintf(file, "  literal: \"%s\"\n", literalStr);
                } else {
                    fprintf(file, "  literal: <null>\n");
                }
            } else {
                fprintf(file, "  value: <null>\n");
            }
            break;
        }
        case ZR_AST_INTEGER_LITERAL: {
            SZrIntegerLiteral *intLit = &node->data.integerLiteral;
            fprintf(file, "  value: %lld\n", (long long)intLit->value);
            break;
        }
        case ZR_AST_FLOAT_LITERAL: {
            SZrFloatLiteral *floatLit = &node->data.floatLiteral;
            // 打印数值，不打印字符串字面量（避免段错误）
            fprintf(file, "  value: %f\n", floatLit->value);
            if (floatLit->literal != ZR_NULL) {
                fprintf(file, "  literal: <string>\n");
            }
            break;
        }
        case ZR_AST_BOOLEAN_LITERAL: {
            SZrBooleanLiteral *boolLit = &node->data.booleanLiteral;
            fprintf(file, "  value: %s\n", boolLit->value ? "true" : "false");
            break;
        }
        case ZR_AST_NULL_LITERAL: {
            fprintf(file, "  value: null\n");
            break;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assign = &node->data.assignmentExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "left: ");
            print_ast_node(state, file, assign->left, indent + 1);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "op: %s\n", assign->op.op ? assign->op.op : "=");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "right: ");
            print_ast_node(state, file, assign->right, indent + 1);
            break;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *bin = &node->data.binaryExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "left: ");
            print_ast_node(state, file, bin->left, indent + 1);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "op: %s\n", bin->op.op ? bin->op.op : "?");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "right: ");
            print_ast_node(state, file, bin->right, indent + 1);
            break;
        }
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *call = &node->data.functionCall;
            if (call->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args (%zu):\n", call->args->count);
                for (TZrSize i = 0; i < call->args->count; i++) {
                    print_ast_node(state, file, call->args->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *member = &node->data.memberExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "computed: %s\n", member->computed ? "true" : "false");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "property: ");
            print_ast_node(state, file, member->property, indent + 1);
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "property: ");
            print_ast_node(state, file, primary->property, indent + 1);
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "members (%zu):\n", primary->members->count);
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    print_ast_node(state, file, primary->members->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION: {
            SZrPrototypeReferenceExpression *prototypeRef = &node->data.prototypeReferenceExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "target: ");
            print_ast_node(state, file, prototypeRef->target, indent + 1);
            break;
        }
        case ZR_AST_CONSTRUCT_EXPRESSION: {
            SZrConstructExpression *construct = &node->data.constructExpression;
            const TZrChar *ownership = "none";
            if (construct->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE) {
                ownership = "unique";
            } else if (construct->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED) {
                ownership = "shared";
            } else if (construct->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
                ownership = "weak";
            }

            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "kind: %s\n", construct->isNew ? "new" : "prototype-call");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "using: %s\n", construct->isUsing ? "true" : "false");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "ownership: %s\n", ownership);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "target: ");
            print_ast_node(state, file, construct->target, indent + 1);
            if (construct->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args (%zu):\n", construct->args->count);
                for (TZrSize i = 0; i < construct->args->count; i++) {
                    print_ast_node(state, file, construct->args->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrExpressionStatement *stmt = &node->data.expressionStatement;
            print_ast_node(state, file, stmt->expr, indent + 1);
            break;
        }
        case ZR_AST_LAMBDA_EXPRESSION: {
            SZrLambdaExpression *lambda = &node->data.lambdaExpression;
            // 打印参数列表
            if (lambda->params != ZR_NULL && lambda->params->count > 0) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "params (%zu):\n", lambda->params->count);
                for (TZrSize i = 0; i < lambda->params->count; i++) {
                    print_ast_node(state, file, lambda->params->nodes[i], indent + 2);
                }
            } else {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "params: ()\n");
            }
            // 打印可变参数（如果有）
            if (lambda->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args: <parameter>\n");
            }
            // 打印函数体
            if (lambda->block != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "body: ");
                print_ast_node(state, file, lambda->block, indent + 1);
            }
            break;
        }
        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *test = &node->data.testDeclaration;
            // 打印测试名称
            if (test->name != ZR_NULL && test->name->name != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "name: ");
                print_ast_node(state, file, (SZrAstNode *)test->name, indent + 1);
            }
            // 打印参数列表
            if (test->params != ZR_NULL && test->params->count > 0) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "params (%zu):\n", test->params->count);
                for (TZrSize i = 0; i < test->params->count; i++) {
                    print_ast_node(state, file, test->params->nodes[i], indent + 2);
                }
            }
            // 打印可变参数（如果有）
            if (test->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args: <parameter>\n");
            }
            // 打印函数体
            if (test->body != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "body: ");
                print_ast_node(state, file, test->body, indent + 1);
            }
            break;
        }
        default:
            // 对于其他类型，只打印类型名称
            break;
    }
}

// 写入语法树文件 (.zrs)
ZR_PARSER_API TZrBool ZrParser_Writer_WriteSyntaxTreeFile(SZrState *state, SZrAstNode *ast, const TZrChar *filename) {
    if (state == ZR_NULL || ast == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }
    
    FILE *file = fopen(filename, "w");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    
    fprintf(file, "// ZR Syntax Tree File (.zrs)\n");
    fprintf(file, "// Generated from parsed AST\n\n");
    
    print_ast_node(state, file, ast, 0);
    
    fclose(file);
    return ZR_TRUE;
}
