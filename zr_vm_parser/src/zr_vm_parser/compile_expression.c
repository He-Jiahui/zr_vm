//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_vm_conf.h"

#include <string.h>

// 前向声明
void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_identifier(SZrCompilerState *cs, SZrAstNode *node);
static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_type_cast_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_function_call(SZrCompilerState *cs, SZrAstNode *node);
static void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node);

// 辅助函数声明（在 compiler.c 中实现，需要声明为 extern 或包含头文件）
// 为了简化，我们直接在 compile_expression.c 中重新声明这些函数
// 注意：这些函数应该在同一编译单元中，或者通过头文件共享

// 前向声明辅助函数（实际实现在 compiler.c 中）
TZrInstruction create_instruction_0(EZrInstructionCode opcode, TUInt16 operandExtra);
TZrInstruction create_instruction_1(EZrInstructionCode opcode, TUInt16 operandExtra, TInt32 operand);
TZrInstruction create_instruction_2(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt16 operand1, TUInt16 operand2);
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction);
TUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value);
TUInt32 find_local_var(SZrCompilerState *cs, SZrString *name);
TUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name);
TUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TBool inStack);
TUInt32 allocate_stack_slot(SZrCompilerState *cs);
TUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name);
TUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name);
TZrSize create_label(SZrCompilerState *cs);
void resolve_label(SZrCompilerState *cs, TZrSize labelId);
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId);
void enter_scope(SZrCompilerState *cs);
void exit_scope(SZrCompilerState *cs);
void compile_statement(SZrCompilerState *cs, SZrAstNode *node);

// 类型转换辅助函数
static void emit_type_conversion(SZrCompilerState *cs, TUInt32 destSlot, TUInt32 srcSlot, EZrInstructionCode conversionOpcode) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    // 类型转换指令格式: operandExtra = destSlot, operand1[0] = srcSlot, operand1[1] = 0
    TZrInstruction convInst = create_instruction_2(conversionOpcode, (TUInt16)destSlot, (TUInt16)srcSlot, 0);
    emit_instruction(cs, convInst);
}

// 带原型信息的类型转换辅助函数（用于 TO_STRUCT 和 TO_OBJECT）
static void emit_type_conversion_with_prototype(SZrCompilerState *cs, TUInt32 destSlot, TUInt32 srcSlot, 
                                                EZrInstructionCode conversionOpcode, TUInt32 prototypeConstantIndex) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    // 类型转换指令格式: operandExtra = destSlot, operand1[0] = srcSlot, operand1[1] = prototypeConstantIndex
    TZrInstruction convInst = create_instruction_2(conversionOpcode, (TUInt16)destSlot, (TUInt16)srcSlot, (TUInt16)prototypeConstantIndex);
    emit_instruction(cs, convInst);
}

// 在脚本 AST 中查找类型定义（struct 或 class）
// 返回找到的 AST 节点，如果未找到返回 ZR_NULL
static SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找 struct 或 class 声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt == ZR_NULL) {
            continue;
        }
        
        // 检查是否是 struct 声明
        if (stmt->type == ZR_AST_STRUCT_DECLARATION) {
            SZrIdentifier *structName = stmt->data.structDeclaration.name;
            if (structName != ZR_NULL && structName->name != ZR_NULL) {
                if (ZrStringEqual(structName->name, typeName)) {
                    return stmt;
                }
            }
        }
        
        // 检查是否是 class 声明
        if (stmt->type == ZR_AST_CLASS_DECLARATION) {
            SZrIdentifier *className = stmt->data.classDeclaration.name;
            if (className != ZR_NULL && className->name != ZR_NULL) {
                if (ZrStringEqual(className->name, typeName)) {
                    return stmt;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 编译字面量
static void compile_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrTypeValue constantValue;
    ZrValueResetAsNull(&constantValue);
    TUInt32 constantIndex = 0;
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL: {
            TBool value = node->data.booleanLiteral.value;
            ZrValueInitAsInt(cs->state, &constantValue, value ? 1 : 0);
            constantValue.type = ZR_VALUE_TYPE_BOOL;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_INTEGER_LITERAL: {
            TInt64 value = node->data.integerLiteral.value;
            ZrValueInitAsInt(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_FLOAT_LITERAL: {
            TDouble value = node->data.floatLiteral.value;
            ZrValueInitAsFloat(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_STRING_LITERAL: {
            SZrString *value = node->data.stringLiteral.value;
            if (value != ZR_NULL) {
                ZrValueInitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
                constantValue.type = ZR_VALUE_TYPE_STRING;
                constantIndex = add_constant(cs, &constantValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
                emit_instruction(cs, inst);
            }
            break;
        }
        
        case ZR_AST_CHAR_LITERAL: {
            TChar value = node->data.charLiteral.value;
            ZrValueInitAsInt(cs->state, &constantValue, (TInt64)value);
            constantValue.type = ZR_VALUE_TYPE_INT8;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_NULL_LITERAL: {
            ZrValueResetAsNull(&constantValue);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        default:
            ZrCompilerError(cs, "Unexpected literal type", node->location);
            break;
    }
}

// 编译标识符
static void compile_identifier(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IDENTIFIER_LITERAL) {
        ZrCompilerError(cs, "Expected identifier", node->location);
        return;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        ZrCompilerError(cs, "Identifier name is null", node->location);
        return;
    }
    
    ZR_UNUSED_PARAMETER(ZrStringGetNativeString(name));
    
    // 重要：先查找局部变量（包括闭包变量）
    // 如果存在同名的局部变量，它会覆盖全局的 zr 对象
    // 这是作用域规则：局部变量优先于全局对象
    TUInt32 localVarIndex = find_local_var(cs, name);
    if (localVarIndex != (TUInt32)-1) {
        // 找到局部变量：使用 GET_STACK
        // 即使这个变量名是 "zr"，也使用局部变量而不是全局 zr 对象
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TInt32)localVarIndex);
        emit_instruction(cs, inst);
        return;
    }
    
    // 查找闭包变量（在局部变量之后，但在全局对象之前）
    TUInt32 closureVarIndex = find_closure_var(cs, name);
    if (closureVarIndex != (TUInt32)-1) {
        // 闭包变量：根据 inStack 标志选择使用 GET_CLOSURE 还是 GETUPVAL
        // 即使这个变量名是 "zr"，也使用闭包变量而不是全局 zr 对象
        TUInt32 destSlot = allocate_stack_slot(cs);
        
        // 获取闭包变量信息以检查 inStack 标志
        SZrFunctionClosureVariable *closureVar = (SZrFunctionClosureVariable *)ZrArrayGet(&cs->closureVars, closureVarIndex);
        if (closureVar != ZR_NULL && closureVar->inStack) {
            // 变量在栈上，使用 GETUPVAL
            // GETUPVAL 格式: operandExtra = destSlot, operand1[0] = closureVarIndex, operand1[1] = 0
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), (TUInt16)destSlot, (TUInt16)closureVarIndex, 0);
            emit_instruction(cs, inst);
        } else {
            // 变量不在栈上，使用 GET_CLOSURE
            // GET_CLOSURE 格式: operandExtra = destSlot, operand1[0] = closureVarIndex, operand1[1] = 0
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_CLOSURE), (TUInt16)destSlot, (TUInt16)closureVarIndex, 0);
            emit_instruction(cs, inst);
        }
        return;
    }
    
    // 只有在没有找到局部变量和闭包变量的情况下，才检查是否是全局关键字 "zr"
    // 这样可以确保局部变量能够覆盖全局的 zr 对象
    TNativeString varNameStr;
    TZrSize nameLen;
    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        varNameStr = ZrStringGetNativeStringShort(name);
        nameLen = name->shortStringLength;
    } else {
        varNameStr = ZrStringGetNativeString(name);
        nameLen = name->longStringLength;
    }
    
    // 检查是否是 "zr" 全局对象
    // 注意：只有在没有局部变量覆盖的情况下才会到达这里
    if (nameLen == 2 && varNameStr[0] == 'z' && varNameStr[1] == 'r') {
        // 使用 GET_GLOBAL 指令获取全局 zr 对象
        TUInt32 destSlot = allocate_stack_slot(cs);
        // GET_GLOBAL 格式: operandExtra = destSlot, operand1[0] = 0, operand1[1] = 0
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TUInt16)destSlot, 0);
        emit_instruction(cs, inst);
        return;
    }
    
    // 尝试作为父函数的子函数访问（使用 GET_SUB_FUNCTION）
    // 在编译时查找子函数索引，而不是使用名称常量
    // GET_SUB_FUNCTION 通过索引直接访问 childFunctionList，这是编译时确定的静态索引
    TUInt32 childFunctionIndex = find_child_function_index(cs, name);
    if (childFunctionIndex != (TUInt32)-1) {
        // 找到子函数索引，生成 GET_SUB_FUNCTION 指令
        // GET_SUB_FUNCTION 格式: operandExtra = destSlot, operand1[0] = childFunctionIndex, operand1[1] = 0
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction getSubFuncInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), (TUInt16)destSlot, (TUInt16)childFunctionIndex, 0);
        emit_instruction(cs, getSubFuncInst);
        return;
    }
    
    // 如果不是子函数，尝试作为全局对象（zr）的属性访问（使用 GET_GLOBAL + GETTABLE）
    // 1. 使用 GET_GLOBAL 获取全局 zr 对象
    TUInt32 globalObjSlot = allocate_stack_slot(cs);
    TZrInstruction getGlobalInst = create_instruction_0(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TUInt16)globalObjSlot);
    emit_instruction(cs, getGlobalInst);
    
    // 2. 将属性名作为字符串常量压栈
    SZrTypeValue nameValue;
    ZrValueInitAsRawObject(cs->state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    nameValue.type = ZR_VALUE_TYPE_STRING;
    TUInt32 nameConstantIndex = add_constant(cs, &nameValue);
    TUInt32 nameSlot = allocate_stack_slot(cs);
    TZrInstruction getNameInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), nameSlot, (TInt32)nameConstantIndex);
    emit_instruction(cs, getNameInst);
    
    // 3. 使用 GETTABLE 访问属性
    TUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)destSlot, (TUInt16)globalObjSlot, (TUInt16)nameSlot);
    emit_instruction(cs, getTableInst);
    
    // 注意：不要释放临时栈槽（globalObjSlot 和 nameSlot），因为它们在运行时仍在使用
    // GETTABLE 指令会在运行时从这些槽位读取值，所以不能提前释放
    // destSlot 是结果槽位，会保留在栈上
}

// 编译一元表达式
static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_UNARY_EXPRESSION) {
        ZrCompilerError(cs, "Expected unary expression", node->location);
        return;
    }
    
    const TChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;
    
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    // 处理$和new操作符（这些操作符的参数是类型名称，需要特殊处理）
    if (strcmp(op, "$") == 0) {
        // $操作符：创建struct实例
        // 参数可能是Identifier（类型名称）或PrimaryExpression（$TypeName(...)）
        SZrString *typeName = ZR_NULL;
        SZrAstNodeArray *constructorArgs = ZR_NULL;
        
        if (arg != ZR_NULL && arg->type == ZR_AST_IDENTIFIER_LITERAL) {
            // 参数是类型名称：$TypeName
            typeName = arg->data.identifier.name;
        } else if (arg != ZR_NULL && arg->type == ZR_AST_PRIMARY_EXPRESSION) {
            // 参数是主表达式：$TypeName(...)
            SZrPrimaryExpression *primary = &arg->data.primaryExpression;
            
            // 从property中提取类型名称
            if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                typeName = primary->property->data.identifier.name;
                
                // 检查members中是否有函数调用
                if (primary->members != ZR_NULL && primary->members->count > 0) {
                    SZrAstNode *firstMember = primary->members->nodes[0];
                    if (firstMember != ZR_NULL && firstMember->type == ZR_AST_FUNCTION_CALL) {
                        SZrFunctionCall *call = &firstMember->data.functionCall;
                        constructorArgs = call->args;
                    }
                }
            }
        }
        
        if (typeName == ZR_NULL) {
            ZrCompilerError(cs, "$ operator requires a type name", node->location);
            return;
        }
        
        // 验证类型名称是否在类型环境中
        if (cs->typeEnv != ZR_NULL && !ZrTypeEnvironmentLookupType(cs->typeEnv, typeName)) {
            // 类型未找到，检查是否是已定义的struct（通过AST查找）
            SZrAstNode *typeDecl = find_type_declaration(cs, typeName);
            if (typeDecl == ZR_NULL || typeDecl->type != ZR_AST_STRUCT_DECLARATION) {
                static TChar errorMsg[256];
                TNativeString nameStr;
                TZrSize nameLen;
                if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    nameStr = ZrStringGetNativeStringShort(typeName);
                    nameLen = typeName->shortStringLength;
                } else {
                    nameStr = ZrStringGetNativeString(typeName);
                    nameLen = typeName->longStringLength;
                }
                snprintf(errorMsg, sizeof(errorMsg), "Type '%.*s' not found", (int)nameLen, nameStr);
                ZrCompilerError(cs, errorMsg, node->location);
                return;
            }
        }
        
        // 将类型名称添加到常量池
        SZrTypeValue typeNameValue;
        ZrValueInitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
        typeNameValue.type = ZR_VALUE_TYPE_STRING;
        TUInt32 typeNameConstantIndex = add_constant(cs, &typeNameValue);
        
        // 创建struct实例（使用TO_STRUCT指令）
        // TO_STRUCT指令格式: operandExtra = destSlot, operand1[0] = sourceSlot, operand1[1] = typeNameConstantIndex
        // 对于$操作符，sourceSlot可以是null或空对象
        TUInt32 nullSlot = allocate_stack_slot(cs);
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 nullConstantIndex = add_constant(cs, &nullValue);
        
        // 先加载null值到栈
        TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)nullSlot, (TInt32)nullConstantIndex);
        emit_instruction(cs, nullInst);
        
        // 然后使用TO_STRUCT指令创建struct实例
        TZrInstruction structInst = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_STRUCT), (TUInt16)destSlot, (TUInt16)nullSlot, (TUInt16)typeNameConstantIndex);
        emit_instruction(cs, structInst);
        
        // 如果有构造函数参数，编译参数并调用constructor（如果存在）
        // 调用constructor（如果存在），需要查找prototype的constructor元方法
        // constructor调用在运行时通过元方法机制实现
        if (constructorArgs != ZR_NULL && constructorArgs->count > 0) {
            // 编译构造函数参数（参数会放在栈上）
            TUInt32 argCount = (TUInt32)constructorArgs->count;
            for (TZrSize i = 0; i < constructorArgs->count; i++) {
                SZrAstNode *argNode = constructorArgs->nodes[i];
                if (argNode != ZR_NULL) {
                    compile_expression(cs, argNode);
                }
            }
            
            // 调用constructor元方法
            // 注意：constructor调用在运行时通过prototype的metaTable查找
            // 这里生成函数调用指令，运行时会在prototype的metaTable中查找CONSTRUCTOR元方法
            // 对象实例在destSlot，参数在destSlot+1到destSlot+argCount
            TUInt32 resultSlot = allocate_stack_slot(cs);
            // 使用FUNCTION_CALL指令调用constructor
            // 运行时会在prototype的metaTable中查找CONSTRUCTOR元方法并调用
            TZrInstruction callConstructorInst = create_instruction_2(
                ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 
                (TUInt16)resultSlot, 
                (TUInt16)destSlot,  // 对象实例作为函数值（运行时查找constructor元方法）
                (TUInt16)argCount
            );
            emit_instruction(cs, callConstructorInst);
            
            // 注意：实际的constructor查找和调用在运行时通过execution.c中的元方法机制实现
            // 这里只是生成调用指令，运行时会在prototype链中查找CONSTRUCTOR元方法
        }
    } else if (strcmp(op, "new") == 0) {
        // new操作符：创建class实例
        // 参数应该是Identifier（类型名称）或PrimaryExpression（TypeName(...)）
        SZrString *typeName = ZR_NULL;
        SZrAstNodeArray *constructorArgs = ZR_NULL;
        
        if (arg != ZR_NULL && arg->type == ZR_AST_IDENTIFIER_LITERAL) {
            // 参数是类型名称
            typeName = arg->data.identifier.name;
        } else if (arg != ZR_NULL && arg->type == ZR_AST_PRIMARY_EXPRESSION) {
            // 参数是主表达式：TypeName(...)
            SZrPrimaryExpression *primary = &arg->data.primaryExpression;
            
            // 从property中提取类型名称
            if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                typeName = primary->property->data.identifier.name;
                
                // 检查members中是否有函数调用
                if (primary->members != ZR_NULL && primary->members->count > 0) {
                    SZrAstNode *firstMember = primary->members->nodes[0];
                    if (firstMember != ZR_NULL && firstMember->type == ZR_AST_FUNCTION_CALL) {
                        SZrFunctionCall *call = &firstMember->data.functionCall;
                        constructorArgs = call->args;
                    }
                }
            }
        }
        
        if (typeName == ZR_NULL) {
            ZrCompilerError(cs, "new operator requires a type name", node->location);
            return;
        }
        
        if (typeName != ZR_NULL) {
            // 将类型名称添加到常量池
            SZrTypeValue typeNameValue;
            ZrValueInitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
            typeNameValue.type = ZR_VALUE_TYPE_STRING;
            TUInt32 typeNameConstantIndex = add_constant(cs, &typeNameValue);
            
            // 创建class实例（使用CREATE_OBJECT指令创建空对象，然后使用TO_OBJECT设置prototype）
            // 先创建空对象
            TZrInstruction createObjInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TUInt16)destSlot);
            emit_instruction(cs, createObjInst);
            
            // 然后使用TO_OBJECT指令设置prototype
            TZrInstruction toObjInst = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_OBJECT), (TUInt16)destSlot, (TUInt16)destSlot, (TUInt16)typeNameConstantIndex);
            emit_instruction(cs, toObjInst);
            
            // 调用constructor（从基类到派生类），需要查找prototype链的constructor元方法
            // 注意：constructor调用在运行时通过prototype链查找，从基类到派生类依次调用
            // TODO: 这里暂时不生成constructor调用指令，因为new操作符创建的对象可能还没有完全初始化
            // 实际的constructor调用应该在对象创建后，通过prototype链查找CONSTRUCTOR元方法
            // 如果需要立即调用constructor，可以在TO_OBJECT指令后添加constructor调用
            // 但通常constructor调用应该在对象完全创建后进行
        } else {
            ZrCompilerError(cs, "new operator requires a type name", node->location);
        }
    } else {
        // 其他一元操作符：先编译操作数
        compile_expression(cs, arg);
        TUInt32 argSlot = cs->stackSlotCount - 1;
        
        if (strcmp(op, "!") == 0) {
            // 逻辑非
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_NOT), destSlot, (TUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "~") == 0) {
            // 位非
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_NOT), destSlot, (TUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "-") == 0) {
            // 取负
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), destSlot, (TUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "+") == 0) {
            // 正号：直接使用操作数（不需要额外指令）
            // 将结果复制到目标槽位
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TInt32)argSlot);
            emit_instruction(cs, inst);
        } else {
            ZrCompilerError(cs, "Unknown unary operator", node->location);
        }
    }
}

// 编译类型转换表达式
static void compile_type_cast_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_TYPE_CAST_EXPRESSION) {
        ZrCompilerError(cs, "Expected type cast expression", node->location);
        return;
    }
    
    SZrType *targetType = node->data.typeCastExpression.targetType;
    SZrAstNode *expression = node->data.typeCastExpression.expression;
    
    if (targetType == ZR_NULL || expression == ZR_NULL) {
        ZrCompilerError(cs, "Invalid type cast expression", node->location);
        return;
    }
    
    // 先编译要转换的表达式
    compile_expression(cs, expression);
    
    TUInt32 srcSlot = cs->stackSlotCount - 1;
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    // 根据目标类型生成相应的转换指令
    // 首先检查类型名称
    if (targetType->name != ZR_NULL && targetType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = targetType->name->data.identifier.name;
        TNativeString typeNameStr;
        TZrSize nameLen;
        
        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            typeNameStr = ZrStringGetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            typeNameStr = ZrStringGetNativeString(typeName);
            nameLen = typeName->longStringLength;
        }
        
        // 检查基本类型
        if (nameLen == 3 && strncmp(typeNameStr, "int", 3) == 0) {
            // 转换为 int
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_INT));
            return;
        } else if (nameLen == 5 && strncmp(typeNameStr, "float", 5) == 0) {
            // 转换为 float
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
            return;
        } else if (nameLen == 6 && strncmp(typeNameStr, "string", 6) == 0) {
            // 转换为 string
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
            return;
        } else if (nameLen == 4 && strncmp(typeNameStr, "bool", 4) == 0) {
            // 转换为 bool
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_BOOL));
            return;
        }
        
        // 对于 struct 和 class 类型，查找类型定义
        SZrAstNode *typeDecl = find_type_declaration(cs, typeName);
        if (typeDecl != ZR_NULL) {
            // 将类型名称作为常量存储（运行时通过类型名称查找或创建原型）
            SZrTypeValue typeNameValue;
            ZrValueResetAsNull(&typeNameValue);
            ZrValueInitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
            typeNameValue.type = ZR_VALUE_TYPE_STRING;
            TUInt32 typeNameConstantIndex = add_constant(cs, &typeNameValue);
            
            // 根据类型声明类型生成相应的转换指令
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                // 生成 TO_STRUCT 指令，将类型名称常量索引作为操作数
                emit_type_conversion_with_prototype(cs, destSlot, srcSlot, 
                                                    ZR_INSTRUCTION_ENUM(TO_STRUCT), 
                                                    typeNameConstantIndex);
                return;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                // 生成 TO_OBJECT 指令，将类型名称常量索引作为操作数
                emit_type_conversion_with_prototype(cs, destSlot, srcSlot, 
                                                    ZR_INSTRUCTION_ENUM(TO_OBJECT), 
                                                    typeNameConstantIndex);
                return;
            }
        }
        
        // 如果未找到类型定义，使用 TO_STRING 作为默认转换
        // 可能需要支持从其他模块导入的类型
        // 注意：从其他模块导入的类型在运行时通过模块系统查找
        // 编译器无法在编译期确定所有类型，因此使用运行时类型查找
        // 如果类型未在当前模块找到，运行时会在全局模块注册表中查找
        emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    } else {
        // TODO: 对于复杂类型（泛型、元组等），暂时使用 TO_STRING
        // 实现完整的类型转换逻辑
        // 注意：复杂类型的转换需要根据具体类型实现
        // 泛型类型转换需要实例化类型参数
        // 元组类型转换需要逐个元素转换
        // TODO: 这里暂时使用TO_STRING作为默认转换，未来可以扩展支持更多类型
        emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    }
}

// 编译二元表达式
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BINARY_EXPRESSION) {
        ZrCompilerError(cs, "Expected binary expression", node->location);
        return;
    }
    
    const TChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    
    // 调试：检查操作符字符串
    if (op == ZR_NULL) {
        ZrCompilerError(cs, "Binary operator string is NULL", node->location);
        return;
    }
    
    // 临时调试：输出操作符字符串的实际值
    // 注意：这里使用 fprintf 来调试，确认 op 的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu)\n", 
    //         op, (int)(unsigned char)op[0], strlen(op));
    
    // 推断左右操作数的类型
    SZrInferredType leftType, rightType, resultType;
    TBool hasTypeInfo = ZR_FALSE;
    if (cs->typeEnv != ZR_NULL) {
        if (infer_expression_type(cs, left, &leftType) && infer_expression_type(cs, right, &rightType)) {
            hasTypeInfo = ZR_TRUE;
            // 推断结果类型
            if (!infer_binary_expression_type(cs, node, &resultType)) {
                // 类型推断失败，使用默认类型
                ZrInferredTypeInit(cs->state, &resultType, ZR_VALUE_TYPE_OBJECT);
            }
        } else {
            // 类型推断失败，清理已推断的类型
            if (infer_expression_type(cs, left, &leftType)) {
                ZrInferredTypeFree(cs->state, &leftType);
            }
            if (infer_expression_type(cs, right, &rightType)) {
                ZrInferredTypeFree(cs->state, &rightType);
            }
        }
    }
    
    // 编译左操作数
    compile_expression(cs, left);
    TUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 编译右操作数
    compile_expression(cs, right);
    TUInt32 rightSlot = cs->stackSlotCount - 1;
    
    // 如果需要类型转换，插入转换指令
    // 注意：对于比较操作，需要保留 leftType 和 rightType 用于选择正确的比较指令
    TBool isComparisonOp = (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
                            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                            strcmp(op, "==") == 0 || strcmp(op, "!=") == 0);
    
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 对于比较操作，不应该根据 resultType（布尔类型）来转换操作数
            // 比较操作的结果才是布尔类型，但操作数应该保持其原始数值类型
            // 实现完整的类型提升逻辑（例如：int 和 float 比较时，将 int 提升为 float）
            // 类型提升规则：
            // 1. 如果一个是float，另一个是int，将int提升为float
            // 2. 如果一个是更大的整数类型，将较小的整数类型提升为较大的类型
            // 3. 其他情况保持原类型
            EZrValueType leftValueType = leftType.baseType;
            EZrValueType rightValueType = rightType.baseType;
            
            // 检查是否需要类型提升
            if (ZR_VALUE_IS_TYPE_FLOAT(leftValueType) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValueType)) {
                // 左操作数是float，右操作数是int，将右操作数提升为float
                TUInt32 promotedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, promotedRightSlot, rightSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
                rightSlot = promotedRightSlot;
            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValueType) && ZR_VALUE_IS_TYPE_FLOAT(rightValueType)) {
                // 左操作数是int，右操作数是float，将左操作数提升为float
                TUInt32 promotedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, promotedLeftSlot, leftSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
                leftSlot = promotedLeftSlot;
            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValueType) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValueType)) {
                // 两个都是整数类型，提升到较大的类型
                // 类型提升顺序：INT8 < INT16 < INT32 < INT64
                // 注意：目前只有 TO_INT 指令（提升到 INT32），对于 INT16 和 INT64 的提升需要特殊处理
                if (leftValueType < rightValueType) {
                    // 左操作数类型较小，提升左操作数
                    // 如果目标是 INT32 或更大，使用 TO_INT 提升到 INT32
                    if (rightValueType == ZR_VALUE_TYPE_INT32 || rightValueType == ZR_VALUE_TYPE_INT64) {
                        if (leftValueType < ZR_VALUE_TYPE_INT32) {
                            TUInt32 promotedLeftSlot = allocate_stack_slot(cs);
                            emit_type_conversion(cs, promotedLeftSlot, leftSlot, ZR_INSTRUCTION_ENUM(TO_INT));
                            leftSlot = promotedLeftSlot;
                        }
                    }
                    // TODO: 对于 INT16 的提升，需要添加 TO_INT16 指令支持
                } else if (rightValueType < leftValueType) {
                    // 右操作数类型较小，提升右操作数
                    // 如果目标是 INT32 或更大，使用 TO_INT 提升到 INT32
                    if (leftValueType == ZR_VALUE_TYPE_INT32 || leftValueType == ZR_VALUE_TYPE_INT64) {
                        if (rightValueType < ZR_VALUE_TYPE_INT32) {
                            TUInt32 promotedRightSlot = allocate_stack_slot(cs);
                            emit_type_conversion(cs, promotedRightSlot, rightSlot, ZR_INSTRUCTION_ENUM(TO_INT));
                            rightSlot = promotedRightSlot;
                        }
                    }
                    // TODO: 对于 INT16 的提升，需要添加 TO_INT16 指令支持
                }
            }
            // 其他情况（如都是float，或类型不兼容）保持原类型
        } else {
            // 对于非比较操作，根据结果类型进行转换
            // 检查左操作数是否需要转换
            EZrInstructionCode leftConvOp = ZrInferredTypeGetConversionOpcode(&leftType, &resultType);
            if (leftConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TUInt32 convertedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedLeftSlot, leftSlot, leftConvOp);
                leftSlot = convertedLeftSlot;
            }
            
            // 检查右操作数是否需要转换
            EZrInstructionCode rightConvOp = ZrInferredTypeGetConversionOpcode(&rightType, &resultType);
            if (rightConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TUInt32 convertedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedRightSlot, rightSlot, rightConvOp);
                rightSlot = convertedRightSlot;
            }
            
            // 对于非比较操作，可以立即清理类型信息
            ZrInferredTypeFree(cs->state, &leftType);
            ZrInferredTypeFree(cs->state, &rightType);
        }
        // resultType 在比较操作中需要使用，所以稍后清理
    }
    
    TUInt32 destSlot = allocate_stack_slot(cs);
    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD);  // 默认（通用加法指令）
    
    // 根据操作符和类型选择指令
    // 临时调试输出：检查操作符字符串的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu), hasTypeInfo=%d\n", 
    //         op, (int)(unsigned char)op[0], strlen(op), hasTypeInfo);
    
    if (strcmp(op, "+") == 0) {
        // 根据结果类型选择 ADD/ADD_INT/ADD_FLOAT/ADD_STRING
        if (!hasTypeInfo) {
            // 类型不明确，使用通用 ADD 指令
            opcode = ZR_INSTRUCTION_ENUM(ADD);
        } else if (resultType.baseType == ZR_VALUE_TYPE_STRING) {
            opcode = ZR_INSTRUCTION_ENUM(ADD_STRING);
        } else if (resultType.baseType == ZR_VALUE_TYPE_FLOAT || resultType.baseType == ZR_VALUE_TYPE_DOUBLE) {
            opcode = ZR_INSTRUCTION_ENUM(ADD_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
        }
    } else if (strcmp(op, "-") == 0) {
        if (!hasTypeInfo) {
            // 类型不明确，使用通用 SUB 指令
            opcode = ZR_INSTRUCTION_ENUM(SUB);
        } else if (resultType.baseType == ZR_VALUE_TYPE_FLOAT || resultType.baseType == ZR_VALUE_TYPE_DOUBLE) {
            opcode = ZR_INSTRUCTION_ENUM(SUB_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
        }
    } else if (strcmp(op, "*") == 0) {
        if (hasTypeInfo && (resultType.baseType == ZR_VALUE_TYPE_FLOAT || resultType.baseType == ZR_VALUE_TYPE_DOUBLE)) {
            opcode = ZR_INSTRUCTION_ENUM(MUL_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
        }
    } else if (strcmp(op, "/") == 0) {
        if (hasTypeInfo && (resultType.baseType == ZR_VALUE_TYPE_FLOAT || resultType.baseType == ZR_VALUE_TYPE_DOUBLE)) {
            opcode = ZR_INSTRUCTION_ENUM(DIV_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
        }
    } else if (strcmp(op, "%") == 0) {
        if (hasTypeInfo && (resultType.baseType == ZR_VALUE_TYPE_FLOAT || resultType.baseType == ZR_VALUE_TYPE_DOUBLE)) {
            opcode = ZR_INSTRUCTION_ENUM(MOD_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
        }
    } else if (strcmp(op, "**") == 0) {
        if (hasTypeInfo && (resultType.baseType == ZR_VALUE_TYPE_FLOAT || resultType.baseType == ZR_VALUE_TYPE_DOUBLE)) {
            opcode = ZR_INSTRUCTION_ENUM(POW_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(POW_SIGNED);
        }
    } else if (strcmp(op, "<<") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT);
    } else if (strcmp(op, ">>") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT);
    } else if (strcmp(op, "==") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL);
    } else if (strcmp(op, "!=") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL);
    } else if (strcmp(op, "<") == 0) {
        // 根据操作数类型选择比较指令
        // 对于比较操作，使用 leftType 或 rightType 来判断操作数类型（它们应该相同或兼容）
        if (hasTypeInfo) {
            // 优先检查是否有浮点数类型
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);  // 默认使用有符号整数比较
        }
    } else if (strcmp(op, ">") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
        }
    } else if (strcmp(op, "<=") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
        }
    } else if (strcmp(op, ">=") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);
        }
    } else if (strcmp(op, "&&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_AND);
    } else if (strcmp(op, "||") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_OR);
    } else if (strcmp(op, "&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_AND);
    } else if (strcmp(op, "|") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_OR);
    } else if (strcmp(op, "^") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_XOR);
    } else {
        ZrCompilerError(cs, "Unknown binary operator", node->location);
        return;
    }
    
    TZrInstruction inst = create_instruction_2(opcode, destSlot, (TUInt16)leftSlot, (TUInt16)rightSlot);
    emit_instruction(cs, inst);
    
    // 清理类型信息
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 比较操作：清理 leftType, rightType 和 resultType
            ZrInferredTypeFree(cs->state, &leftType);
            ZrInferredTypeFree(cs->state, &rightType);
        }
        ZrInferredTypeFree(cs->state, &resultType);
    }
}

// 编译赋值表达式
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        ZrCompilerError(cs, "Expected assignment expression", node->location);
        return;
    }
    
    const TChar *op = node->data.assignmentExpression.op.op;
    SZrAstNode *left = node->data.assignmentExpression.left;
    SZrAstNode *right = node->data.assignmentExpression.right;
    
    // 编译右值
    compile_expression(cs, right);
    TUInt32 rightSlot = cs->stackSlotCount - 1;
    
    // 处理左值（标识符）
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = left->data.identifier.name;
        TUInt32 localVarIndex = find_local_var(cs, name);
        
        if (localVarIndex != (TUInt32)-1) {
            // 简单赋值
            if (strcmp(op, "=") == 0) {
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)localVarIndex, (TInt32)rightSlot);
                emit_instruction(cs, inst);
            } else {
                // 复合赋值：先读取左值，执行运算，再赋值
                TUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), leftSlot, (TInt32)localVarIndex);
                emit_instruction(cs, getInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TUInt16)leftSlot, (TUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 赋值
                TZrInstruction setInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)localVarIndex, (TInt32)resultSlot);
                emit_instruction(cs, setInst);
            }
        } else {
            // 查找闭包变量
            TUInt32 closureVarIndex = find_closure_var(cs, name);
            if (closureVarIndex != (TUInt32)-1) {
                // 获取闭包变量信息以检查 inStack 标志
                SZrFunctionClosureVariable *closureVar = (SZrFunctionClosureVariable *)ZrArrayGet(&cs->closureVars, closureVarIndex);
                TBool useUpval = (closureVar != ZR_NULL && closureVar->inStack);
                
                // 闭包变量：根据 inStack 标志选择使用 SET_CLOSURE 还是 SETUPVAL
                // 简单赋值
                if (strcmp(op, "=") == 0) {
                    if (useUpval) {
                        // SETUPVAL 格式: operandExtra = closureVarIndex, operand1[0] = rightSlot, operand1[1] = 0
                        TZrInstruction setUpvalInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), (TUInt16)closureVarIndex, (TUInt16)rightSlot, 0);
                        emit_instruction(cs, setUpvalInst);
                    } else {
                        // SET_CLOSURE 格式: operandExtra = closureVarIndex, operand1[0] = rightSlot, operand1[1] = 0
                        TZrInstruction setClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE), (TUInt16)closureVarIndex, (TUInt16)rightSlot, 0);
                        emit_instruction(cs, setClosureInst);
                    }
                } else {
                    // 复合赋值：先读取，执行运算，再写入
                    TUInt32 leftSlot = allocate_stack_slot(cs);
                    if (useUpval) {
                        TZrInstruction getUpvalInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), (TUInt16)leftSlot, (TUInt16)closureVarIndex, 0);
                        emit_instruction(cs, getUpvalInst);
                    } else {
                        TZrInstruction getClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_CLOSURE), (TUInt16)leftSlot, (TUInt16)closureVarIndex, 0);
                        emit_instruction(cs, getClosureInst);
                    }
                    
                    // 执行运算
                    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                    if (strcmp(op, "+=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                    } else if (strcmp(op, "-=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                    } else if (strcmp(op, "*=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                    } else if (strcmp(op, "/=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                    } else if (strcmp(op, "%=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                    }
                    
                    TUInt32 resultSlot = allocate_stack_slot(cs);
                    TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TUInt16)leftSlot, (TUInt16)rightSlot);
                    emit_instruction(cs, opInst);
                    
                    // 写入闭包变量
                    if (useUpval) {
                        TZrInstruction setUpvalInst2 = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), (TUInt16)closureVarIndex, (TUInt16)resultSlot, 0);
                        emit_instruction(cs, setUpvalInst2);
                    } else {
                        TZrInstruction setClosureInst2 = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE), (TUInt16)closureVarIndex, (TUInt16)resultSlot, 0);
                        emit_instruction(cs, setClosureInst2);
                    }
                    
                    // 释放临时栈槽
                    cs->stackSlotCount -= 2; // leftSlot 和 resultSlot
                }
                return;
            }
            
            // 尝试作为全局变量访问（使用 SET_TABLE）
            // 1. 获取全局对象（zr 对象）
            TUInt32 globalSlot = allocate_stack_slot(cs);
            TZrInstruction getGlobalInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TUInt16)globalSlot, 0);
            emit_instruction(cs, getGlobalInst);
            
            // 2. 将变量名转换为字符串常量并压入栈
            SZrTypeValue nameValue;
            ZrValueInitAsRawObject(cs->state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
            nameValue.type = ZR_VALUE_TYPE_STRING;
            TUInt32 nameConstantIndex = add_constant(cs, &nameValue);
            TUInt32 keySlot = allocate_stack_slot(cs);
            TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)keySlot, (TInt32)nameConstantIndex);
            emit_instruction(cs, getKeyInst);
            
            // 对于复合赋值，需要先读取值，执行运算，再写入
            if (strcmp(op, "=") != 0) {
                // 读取全局变量值
                TUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)leftSlot, (TUInt16)globalSlot, (TUInt16)keySlot);
                emit_instruction(cs, getTableInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TUInt16)leftSlot, (TUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 写入全局变量（使用 SET_TABLE）
                // SET_TABLE 格式: operandExtra = destSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)resultSlot, (TUInt16)globalSlot, (TUInt16)keySlot);
                emit_instruction(cs, setTableInst);
                
                // 释放临时栈槽
                cs->stackSlotCount -= 3; // leftSlot, resultSlot, globalSlot, keySlot (但 resultSlot 会被保留)
            } else {
                // 简单赋值：直接使用 SET_TABLE
                // SET_TABLE 格式: operandExtra = destSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)rightSlot, (TUInt16)globalSlot, (TUInt16)keySlot);
                emit_instruction(cs, setTableInst);
                
                // 释放临时栈槽
                cs->stackSlotCount -= 2; // globalSlot 和 keySlot
            }
        }
    } else {
        // 处理成员访问等复杂左值
        // 支持 obj.prop = value 和 arr[index] = value
        // 注意：成员访问在 primary expression 中处理，这里需要处理 primary expression 作为左值
        if (left->type == ZR_AST_PRIMARY_EXPRESSION) {
            SZrPrimaryExpression *primary = &left->data.primaryExpression;
            
            // 检查是否是成员访问（members 数组不为空且最后一个成员是 MemberExpression）
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                SZrAstNode *lastMember = primary->members->nodes[primary->members->count - 1];
                if (lastMember != ZR_NULL && lastMember->type == ZR_AST_MEMBER_EXPRESSION) {
                    // 编译整个 primary expression 以获取对象和键
                    // 先编译基础属性（对象）
                    if (primary->property != ZR_NULL) {
                        compile_expression(cs, primary->property);
                        TUInt32 objSlot = cs->stackSlotCount - 1;
                        
                        // 处理成员访问链，获取最后一个成员访问的键
                        SZrMemberExpression *memberExpr = &lastMember->data.memberExpression;
                        if (memberExpr->property != ZR_NULL) {
                            compile_expression(cs, memberExpr->property);
                            TUInt32 keySlot = cs->stackSlotCount - 1;
                            
                            // 使用 SETTABLE 设置对象属性
                            // SETTABLE 格式: operandExtra = valueSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                            if (strcmp(op, "=") == 0) {
                                // 简单赋值
                                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)rightSlot, (TUInt16)objSlot, (TUInt16)keySlot);
                                emit_instruction(cs, setTableInst);
                            } else {
                                // 复合赋值：先读取，执行运算，再写入
                                TUInt32 leftValueSlot = allocate_stack_slot(cs);
                                TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)leftValueSlot, (TUInt16)objSlot, (TUInt16)keySlot);
                                emit_instruction(cs, getTableInst);
                                
                                // 执行运算
                                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                                if (strcmp(op, "+=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                                } else if (strcmp(op, "-=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                                } else if (strcmp(op, "*=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                                } else if (strcmp(op, "/=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                                } else if (strcmp(op, "%=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                                }
                                
                                TUInt32 resultSlot = allocate_stack_slot(cs);
                                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TUInt16)leftValueSlot, (TUInt16)rightSlot);
                                emit_instruction(cs, opInst);
                                
                                // 使用 SETTABLE 写入结果
                                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)resultSlot, (TUInt16)objSlot, (TUInt16)keySlot);
                                emit_instruction(cs, setTableInst);
                                
                                // 释放临时栈槽
                                cs->stackSlotCount -= 2; // leftValueSlot 和 resultSlot
                            }
                            
                            // 释放临时栈槽
                            cs->stackSlotCount -= 2; // objSlot 和 keySlot
                            return;
                        }
                    }
                }
            }
        }
        
        // 其他复杂左值暂不支持
        ZrCompilerError(cs, "Complex left-hand side not supported yet", node->location);
    }
}

// 编译逻辑表达式（&& 和 ||，支持短路求值）
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LOGICAL_EXPRESSION) {
        ZrCompilerError(cs, "Expected logical expression", node->location);
        return;
    }
    
    const TChar *op = node->data.logicalExpression.op;
    SZrAstNode *left = node->data.logicalExpression.left;
    SZrAstNode *right = node->data.logicalExpression.right;
    
    // 编译左操作数
    compile_expression(cs, left);
    TUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 分配结果槽位
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    // 创建标签用于短路求值
    TZrSize shortCircuitLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    if (strcmp(op, "&&") == 0) {
        // && 运算符：如果左操作数为false，短路返回false
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为false，跳转到短路标签
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, shortCircuitLabelId);
        
        // 编译右操作数
        compile_expression(cs, right);
        TUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 跳转到结束标签
        TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpEndIndex = cs->instructionCount;
        emit_instruction(cs, jumpEndInst);
        add_pending_jump(cs, jumpEndIndex, endLabelId);
        
        // 短路标签：左操作数为false，结果就是false（已经在destSlot中）
        resolve_label(cs, shortCircuitLabelId);
        
        // 结束标签
        resolve_label(cs, endLabelId);
    } else if (strcmp(op, "||") == 0) {
        // || 运算符：如果左操作数为true，短路返回true
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为true，跳转到结束标签（短路返回true）
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, endLabelId);
        
        // 左操作数为false，需要计算右操作数
        // 编译右操作数
        compile_expression(cs, right);
        TUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)destSlot, (TInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 结束标签：左操作数为true时跳转到这里（结果已经是true）
        resolve_label(cs, endLabelId);
    } else {
        ZrCompilerError(cs, "Unknown logical operator", node->location);
        return;
    }
}

// 编译条件表达式（三元运算符）
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        ZrCompilerError(cs, "Expected conditional expression", node->location);
        return;
    }
    
    SZrAstNode *test = node->data.conditionalExpression.test;
    SZrAstNode *consequent = node->data.conditionalExpression.consequent;
    SZrAstNode *alternate = node->data.conditionalExpression.alternate;
    
    // 编译条件
    compile_expression(cs, test);
    TUInt32 testSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)testSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    compile_expression(cs, consequent);
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    compile_expression(cs, alternate);
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 在脚本 AST 中查找函数声明
static SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || funcName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找函数声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt != ZR_NULL && stmt->type == ZR_AST_FUNCTION_DECLARATION) {
            SZrFunctionDeclaration *funcDecl = &stmt->data.functionDeclaration;
            if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
                if (ZrStringEqual(funcDecl->name->name, funcName)) {
                    return stmt;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 匹配命名参数到位置参数
// 返回重新排列后的参数数组，如果匹配失败返回原数组
static SZrAstNodeArray *match_named_arguments(SZrCompilerState *cs, 
                                               SZrFunctionCall *call,
                                               SZrAstNodeArray *paramList) {
    if (cs == ZR_NULL || call == ZR_NULL || !call->hasNamedArgs || 
        call->args == ZR_NULL || call->argNames == ZR_NULL || paramList == ZR_NULL) {
        return call->args;  // 没有命名参数或无法匹配，返回原数组
    }
    
    // 创建参数映射表：参数名 -> 参数索引
    TZrSize paramCount = paramList->count;
    SZrString **paramNames = ZrMemoryRawMallocWithType(cs->state->global, 
                                                       sizeof(SZrString*) * paramCount, 
                                                       ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (paramNames == ZR_NULL) {
        return call->args;  // 内存分配失败，返回原数组
    }
    
    // 提取参数名
    for (TZrSize i = 0; i < paramCount; i++) {
        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->name != ZR_NULL) {
                paramNames[i] = param->name->name;
            } else {
                paramNames[i] = ZR_NULL;
            }
        } else {
            paramNames[i] = ZR_NULL;
        }
    }
    
    // 创建重新排列的参数数组
    SZrAstNodeArray *reorderedArgs = ZrAstNodeArrayNew(cs->state, paramCount);
    if (reorderedArgs == ZR_NULL) {
        ZrMemoryRawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    
    // 初始化数组，所有位置设为 ZR_NULL（表示未提供）
    // 注意：不能使用 ZrAstNodeArrayAdd 因为当 node 为 ZR_NULL 时会直接返回
    // 所以直接设置数组元素并手动更新 count
    for (TZrSize i = 0; i < paramCount; i++) {
        reorderedArgs->nodes[i] = ZR_NULL;
    }
    reorderedArgs->count = paramCount;  // 手动设置 count
    
    // 标记哪些参数已被提供
    TBool *provided = ZrMemoryRawMallocWithType(cs->state->global, 
                                                sizeof(TBool) * paramCount, 
                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        ZrAstNodeArrayFree(cs->state, reorderedArgs);
        ZrMemoryRawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    for (TZrSize i = 0; i < paramCount; i++) {
        provided[i] = ZR_FALSE;
    }
    
    // 处理位置参数（在命名参数之前）
    TZrSize positionalCount = 0;
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrArrayGet(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
            // 位置参数
            if (positionalCount < paramCount) {
                reorderedArgs->nodes[positionalCount] = call->args->nodes[i];
                provided[positionalCount] = ZR_TRUE;
                positionalCount++;
            } else {
                // 位置参数过多
                ZrCompilerError(cs, "Too many positional arguments", call->args->nodes[i]->location);
                ZrMemoryRawFreeWithType(cs->state->global, provided, sizeof(TBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrMemoryRawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrAstNodeArrayFree(cs->state, reorderedArgs);
                return call->args;
            }
        } else {
            // 遇到命名参数，停止处理位置参数
            break;
        }
    }
    
    // 处理命名参数
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrArrayGet(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            // 命名参数
            SZrString *argName = *namePtr;
            TBool found = ZR_FALSE;
            
            // 查找参数名对应的位置
            for (TZrSize j = 0; j < paramCount; j++) {
                if (paramNames[j] != ZR_NULL) {
                    if (ZrStringEqual(argName, paramNames[j])) {
                        if (provided[j]) {
                            // 参数重复
                            ZrCompilerError(cs, "Duplicate argument name", call->args->nodes[i]->location);
                            ZrMemoryRawFreeWithType(cs->state->global, provided, sizeof(TBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrMemoryRawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrAstNodeArrayFree(cs->state, reorderedArgs);
                            return call->args;
                        }
                        reorderedArgs->nodes[j] = call->args->nodes[i];
                        provided[j] = ZR_TRUE;
                        found = ZR_TRUE;
                        break;
                    }
                }
            }
            
            if (!found) {
                // 未找到匹配的参数名
                TNativeString nameStr = ZrStringGetNativeString(argName);
                TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), "Unknown argument name: %s", nameStr ? nameStr : "<null>");
                ZrCompilerError(cs, errorMsg, call->args->nodes[i]->location);
                ZrMemoryRawFreeWithType(cs->state->global, provided, sizeof(TBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrMemoryRawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrAstNodeArrayFree(cs->state, reorderedArgs);
                return call->args;
            }
        }
    }
    
    // 检查必需参数是否都已提供（TODO: 需要检查默认参数）
    // TODO: 目前简化处理，假设所有参数都是必需的
    
    ZrMemoryRawFreeWithType(cs->state->global, provided, sizeof(TBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrMemoryRawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    
    return reorderedArgs;
}

// 编译函数调用表达式
static void compile_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FUNCTION_CALL) {
        ZrCompilerError(cs, "Expected function call", node->location);
        return;
    }
    
    // 函数调用在 primary expression 中处理
    // 这里只处理参数列表
    SZrAstNodeArray *args = node->data.functionCall.args;
    if (args != ZR_NULL) {
        // 编译所有参数表达式（从右到左压栈，或从左到右，取决于调用约定）
        for (TZrSize i = 0; i < args->count; i++) {
            SZrAstNode *arg = args->nodes[i];
            if (arg != ZR_NULL) {
                compile_expression(cs, arg);
            }
        }
    }
    
    // 注意：实际的函数调用指令（FUNCTION_CALL）应该在 primary expression 中生成
    // 因为需要先编译 callee（被调用表达式）
}

// 编译成员访问表达式
static void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrCompilerError(cs, "Expected member expression", node->location);
        return;
    }
    
    // 成员访问在 primary expression 中处理
    // 这里只处理属性访问
    // 注意：实际的 GETTABLE/SETTABLE 指令应该在 primary expression 中生成
    // 因为需要先编译对象表达式
}

// 编译主表达式（属性访问链和函数调用链）
static void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrCompilerError(cs, "Expected primary expression", node->location);
        return;
    }
    
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    
    // 1. 编译基础属性（标识符或表达式）
    if (primary->property != ZR_NULL) {
        compile_expression(cs, primary->property);
        if (cs->hasError) {
            return;
        }
    } else {
        ZrCompilerError(cs, "Primary expression property is null", node->location);
        return;
    }
    
    TUInt32 currentSlot = cs->stackSlotCount - 1;
    
    // 2. 依次处理成员访问链和函数调用链
    if (primary->members != ZR_NULL) {
        for (TZrSize i = 0; i < primary->members->count; i++) {
            SZrAstNode *member = primary->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            if (member->type == ZR_AST_MEMBER_EXPRESSION) {
                // 成员访问：obj.member 或 obj[key]
                SZrMemberExpression *memberExpr = &member->data.memberExpression;
                
                // 编译键表达式（标识符或计算属性）
                if (memberExpr->property != ZR_NULL) {
                    compile_expression(cs, memberExpr->property);
                    TUInt32 keySlot = cs->stackSlotCount - 1;
                    
                    // 使用 GETTABLE 获取值
                    TUInt32 resultSlot = allocate_stack_slot(cs);
                    TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TUInt16)resultSlot, (TUInt16)currentSlot, (TUInt16)keySlot);
                    emit_instruction(cs, getTableInst);
                    
                    // 释放临时栈槽
                    cs->stackSlotCount -= 2;  // keySlot 和 currentSlot
                    currentSlot = resultSlot;
                }
            } else if (member->type == ZR_AST_FUNCTION_CALL) {
                // 函数调用：obj.method(args)
                SZrFunctionCall *call = &member->data.functionCall;
                
                // 尝试匹配命名参数（如果存在）
                SZrAstNodeArray *argsToCompile = call->args;
                if (call->hasNamedArgs && primary->property != ZR_NULL && 
                    primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                    // 尝试查找函数定义（仅对编译期已知的函数）
                    SZrString *funcName = primary->property->data.identifier.name;
                    
                    // 查找子函数
                    TUInt32 childFuncIndex = find_child_function_index(cs, funcName);
                    if (childFuncIndex != (TUInt32)-1) {
                        // 找到子函数，获取参数列表
                        ZR_UNUSED_PARAMETER(ZrArrayGet(&cs->childFunctions, childFuncIndex));
                        // 从函数对象获取参数列表（需要扩展函数对象存储参数信息）
                        // 注意：函数对象可能没有存储参数信息，需要从AST中获取
                        // 这里先尝试从AST查找函数声明来获取参数列表
                        SZrAstNode *funcDecl = find_function_declaration(cs, funcName);
                        if (funcDecl != ZR_NULL && funcDecl->type == ZR_AST_FUNCTION_DECLARATION) {
                            SZrFunctionDeclaration *funcDeclData = &funcDecl->data.functionDeclaration;
                            if (funcDeclData->params != ZR_NULL) {
                                // 匹配命名参数
                                argsToCompile = match_named_arguments(cs, call, funcDeclData->params);
                            }
                        }
                        // 如果函数对象存储了参数信息，可以从childFunc中获取
                        // 但目前函数对象没有存储参数信息，所以从AST获取
                    } else {
                        // 查找脚本级函数声明
                        SZrAstNode *funcDecl = find_function_declaration(cs, funcName);
                        if (funcDecl != ZR_NULL && funcDecl->type == ZR_AST_FUNCTION_DECLARATION) {
                            SZrFunctionDeclaration *funcDeclData = &funcDecl->data.functionDeclaration;
                            if (funcDeclData->params != ZR_NULL) {
                                // 匹配命名参数
                                argsToCompile = match_named_arguments(cs, call, funcDeclData->params);
                            }
                        }
                    }
                }
                
                // 编译参数
                if (argsToCompile != ZR_NULL) {
                    for (TZrSize j = 0; j < argsToCompile->count; j++) {
                        SZrAstNode *arg = argsToCompile->nodes[j];
                        if (arg != ZR_NULL) {
                            compile_expression(cs, arg);
                            if (cs->hasError) {
                                break;
                            }
                        }
                    }
                }
                
                // 生成函数调用指令
                // 检测尾调用：如果在尾调用上下文中，使用FUNCTION_TAIL_CALL
                TUInt32 argCount = (argsToCompile != ZR_NULL) ? (TUInt32)argsToCompile->count : 0;
                
                // 函数调用指令格式：
                // FUNCTION_CALL/FUNCTION_TAIL_CALL: operandExtra = resultSlot, operand1[0] = functionSlot, operand1[1] = argCount
                // 函数值应该在 functionSlot，参数应该在 functionSlot+1 到 functionSlot+argCount
                // 但是，由于参数是后编译的，它们会在 currentSlot 之后
                // 所以需要调整：函数值在 currentSlot，参数在 currentSlot+1 到 currentSlot+argCount
                TUInt32 resultSlot = allocate_stack_slot(cs);
                EZrInstructionCode callOpcode = cs->isInTailCallContext ? 
                    ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) : ZR_INSTRUCTION_ENUM(FUNCTION_CALL);
                TZrInstruction callInst = create_instruction_2(callOpcode, (TUInt16)resultSlot, (TUInt16)currentSlot, (TUInt16)argCount);
                emit_instruction(cs, callInst);
                
                // 释放参数栈槽和函数槽
                // 注意：FUNCTION_CALL 指令会消耗函数值和所有参数，所以需要释放这些栈槽
                cs->stackSlotCount -= (argCount + 1);  // 参数 + 函数
                currentSlot = resultSlot;
                
                // 如果使用了重新排列的参数数组，释放它
                if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                    ZrAstNodeArrayFree(cs->state, argsToCompile);
                }
            }
        }
    }
}

// 编译数组字面量
static void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ARRAY_LITERAL) {
        ZrCompilerError(cs, "Expected array literal", node->location);
        return;
    }
    
    SZrArrayLiteral *arrayLiteral = &node->data.arrayLiteral;
    
    // 1. 创建空数组对象
    TUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createArrayInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_ARRAY), (TUInt16)destSlot);
    emit_instruction(cs, createArrayInst);
    
    // 2. 编译每个元素并设置到数组中
    if (arrayLiteral->elements != ZR_NULL) {
        for (TZrSize i = 0; i < arrayLiteral->elements->count; i++) {
            SZrAstNode *elemNode = arrayLiteral->elements->nodes[i];
            if (elemNode == ZR_NULL) {
                continue;
            }
            
            // 编译元素值
            compile_expression(cs, elemNode);
            TUInt32 valueSlot = cs->stackSlotCount - 1;
            
            // 创建索引常量
            SZrTypeValue indexValue;
            ZrValueInitAsInt(cs->state, &indexValue, (TInt64)i);
            TUInt32 indexConstantIndex = add_constant(cs, &indexValue);
            
            // 将索引压栈
            TUInt32 indexSlot = allocate_stack_slot(cs);
            TZrInstruction getIndexInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)indexSlot, (TInt32)indexConstantIndex);
            emit_instruction(cs, getIndexInst);
            
            // 使用 SETTABLE 设置数组元素
            // SETTABLE 的格式: operandExtra = valueSlot (destination/value), operand1[0] = tableSlot, operand1[1] = keySlot
            TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)valueSlot, (TUInt16)destSlot, (TUInt16)indexSlot);
            emit_instruction(cs, setTableInst);
            
            // 释放临时栈槽（indexSlot 和 valueSlot）
            cs->stackSlotCount -= 2;
        }
    }
    
    // 3. 数组对象已经在 destSlot，结果留在 destSlot
}

// 编译对象字面量
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_OBJECT_LITERAL) {
        ZrCompilerError(cs, "Expected object literal", node->location);
        return;
    }
    
    SZrObjectLiteral *objectLiteral = &node->data.objectLiteral;
    
    // 1. 创建空对象
    TUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createObjectInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TUInt16)destSlot);
    emit_instruction(cs, createObjectInst);
    
    // 2. 编译每个键值对并设置到对象中
    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *kvNode = objectLiteral->properties->nodes[i];
            if (kvNode == ZR_NULL || kvNode->type != ZR_AST_KEY_VALUE_PAIR) {
                continue;
            }
            
            SZrKeyValuePair *kv = &kvNode->data.keyValuePair;
            
            // 编译键
            if (kv->key != ZR_NULL) {
                // 键可能是标识符、字符串字面量或表达式（计算键）
                if (kv->key->type == ZR_AST_IDENTIFIER_LITERAL) {
                    // 标识符键：转换为字符串常量
                    SZrString *keyName = kv->key->data.identifier.name;
                    if (keyName != ZR_NULL) {
                        SZrTypeValue keyValue;
                        ZrValueInitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyName));
                        keyValue.type = ZR_VALUE_TYPE_STRING;
                        TUInt32 keyConstantIndex = add_constant(cs, &keyValue);
                        
                        TUInt32 keySlot = allocate_stack_slot(cs);
                        TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)keySlot, (TInt32)keyConstantIndex);
                        emit_instruction(cs, getKeyInst);
                        
                        // 编译值
                        compile_expression(cs, kv->value);
                        TUInt32 valueSlot = cs->stackSlotCount - 1;
                        
                        // 使用 SETTABLE 设置对象属性
                        TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)valueSlot, (TUInt16)destSlot, (TUInt16)keySlot);
                        emit_instruction(cs, setTableInst);
                        
                        // 释放临时栈槽
                        cs->stackSlotCount -= 2;
                    }
                } else {
                    // 键是表达式（字符串字面量或计算键）
                    compile_expression(cs, kv->key);
                    TUInt32 keySlot = cs->stackSlotCount - 1;
                    
                    // 编译值
                    compile_expression(cs, kv->value);
                    TUInt32 valueSlot = cs->stackSlotCount - 1;
                    
                    // 使用 SETTABLE 设置对象属性
                    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TUInt16)valueSlot, (TUInt16)destSlot, (TUInt16)keySlot);
                    emit_instruction(cs, setTableInst);
                    
                    // 释放临时栈槽
                    cs->stackSlotCount -= 2;
                }
            }
        }
    }
    
    // 3. 对象已经在 destSlot，结果留在 destSlot
}

// 编译 Lambda 表达式
static void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LAMBDA_EXPRESSION) {
        ZrCompilerError(cs, "Expected lambda expression", node->location);
        return;
    }
    
    SZrLambdaExpression *lambda = &node->data.lambdaExpression;
    
    // Lambda 表达式类似于匿名函数，需要创建一个嵌套函数
    // 1. 创建一个临时的函数声明节点来复用函数编译逻辑
    // 2. 或者直接在这里实现类似的逻辑
    
    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    
    // 创建新的函数对象
    cs->currentFunction = ZrFunctionNew(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create lambda function object", node->location);
        return;
    }
    
    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;
    
    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
    
    // 进入函数作用域
    enter_scope(cs);
    
    // 分析lambda体中引用的外部变量（完整实现）
    // 在编译lambda体之前，先分析所有外部变量引用
    if (oldFunction != ZR_NULL) {
        // 有父函数，需要分析外部变量
        if (lambda->block != ZR_NULL) {
            analyze_external_variables(cs, lambda->block, cs);
        }
    }
    
    // 1. 编译参数列表
    TUInt32 parameterCount = 0;
    if (lambda->params != ZR_NULL) {
        for (TZrSize i = 0; i < lambda->params->count; i++) {
            SZrAstNode *paramNode = lambda->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        TUInt32 paramIndex = allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        // 如果有默认值，编译默认值表达式
                        if (param->defaultValue != ZR_NULL) {
                            compile_expression(cs, param->defaultValue);
                            TUInt32 defaultSlot = cs->stackSlotCount - 1;
                            
                            // 生成 SET_STACK 指令
                            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TUInt16)paramIndex, (TInt32)defaultSlot);
                            emit_instruction(cs, inst);
                        }
                    }
                }
            }
        }
    }
    
    // 检查是否有可变参数
    TBool hasVariableArguments = (lambda->args != ZR_NULL);
    
    // 2. 编译函数体（block）
    if (lambda->block != ZR_NULL) {
        compile_statement(cs, lambda->block);
    }
    
    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrValueResetAsNull(&nullValue);
            TUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)resultSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
            
            TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16)resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst = (TZrInstruction *)ZrArrayGet(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode)lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrValueResetAsNull(&nullValue);
                        TUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16)resultSlot, (TInt32)constantIndex);
                        emit_instruction(cs, inst);
                        
                        TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16)resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            }
        }
    }
    
    // 退出函数作用域
    exit_scope(cs);
    
    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList = (TZrInstruction *)ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TUInt32)cs->instructions.length;
            cs->instructionCount = cs->instructions.length;
        }
    }
    
    // 复制常量列表
    if (cs->constantCount > 0) {
        TZrSize constSize = cs->constantCount * sizeof(SZrTypeValue);
        newFunc->constantValueList = (SZrTypeValue *)ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TUInt32)cs->constantCount;
        }
    }
    
    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrMemoryRawMallocWithType(global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TUInt32)cs->localVars.length;
            cs->localVarCount = cs->localVars.length;
        }
    }
    
    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *)ZrMemoryRawMallocWithType(global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TUInt32)cs->closureVarCount;
        }
    }
    
    // 设置函数元数据
    newFunc->stackSize = (TUInt32)cs->stackSlotCount;
    newFunc->parameterCount = (TUInt16)parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TUInt32)node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TUInt32)node->location.end.line : 0;
    
    // 设置函数名（lambda 表达式是匿名函数，所以为 ZR_NULL）
    newFunc->functionName = ZR_NULL;
    
    // 将新函数添加到子函数列表
    if (oldFunction != ZR_NULL) {
        ZrArrayPush(cs->state, &cs->childFunctions, &newFunc);
    }
    
    // 4. 生成 CREATE_CLOSURE 指令（在恢复状态之前）
    // 将函数对象添加到常量池，然后生成 CREATE_CLOSURE 指令
    // TODO: 注意：这里简化处理，直接将函数对象作为常量
    // 完整的闭包处理已实现：通过 analyze_external_variables 分析外部变量并捕获
    SZrTypeValue funcValue;
    ZrValueInitAsRawObject(cs->state, &funcValue, ZR_CAST_RAW_OBJECT_AS_SUPER(newFunc));
    // 函数对象在 VM 中通常作为 CLOSURE 类型处理
    funcValue.type = ZR_VALUE_TYPE_CLOSURE;
    TUInt32 funcConstantIndex = add_constant(cs, &funcValue);
    
    // 分配目标槽位
    TUInt32 destSlot = allocate_stack_slot(cs);
    
    // 保存闭包变量数量（在恢复状态之前）
    TUInt32 closureVarCount = (TUInt32)newFunc->closureValueLength;
    
    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    
    // 生成 CREATE_CLOSURE 指令
    // CREATE_CLOSURE 的格式: operandExtra = destSlot, operand1[0] = functionConstantIndex, operand1[1] = closureVarCount
    TZrInstruction createClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE), (TUInt16)destSlot, (TUInt16)funcConstantIndex, (TUInt16)closureVarCount);
    emit_instruction(cs, createClosureInst);
}

// 辅助函数：编译块并提取最后一个表达式的值（用于if表达式等场景）
static void compile_block_as_expression(SZrCompilerState *cs, SZrAstNode *blockNode) {
    if (cs == ZR_NULL || blockNode == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (blockNode->type != ZR_AST_BLOCK) {
        // 如果不是块，直接编译为表达式
        compile_expression(cs, blockNode);
        return;
    }
    
    SZrBlock *block = &blockNode->data.block;
    
    // 进入新作用域
    enter_scope(cs);
    
    // 编译块内所有语句
    if (block->body != ZR_NULL && block->body->count > 0) {
        // 编译除最后一个语句外的所有语句
        for (TZrSize i = 0; i < block->body->count - 1; i++) {
            SZrAstNode *stmt = block->body->nodes[i];
            if (stmt != ZR_NULL) {
                compile_statement(cs, stmt);
                if (cs->hasError) {
                    exit_scope(cs);
                    return;
                }
            }
        }
        
        // 编译最后一个语句，并提取其表达式的值
        SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
        if (lastStmt != ZR_NULL) {
            if (lastStmt->type == ZR_AST_EXPRESSION_STATEMENT) {
                // 表达式语句：编译表达式，值留在栈上
                SZrExpressionStatement *exprStmt = &lastStmt->data.expressionStatement;
                if (exprStmt->expr != ZR_NULL) {
                    compile_expression(cs, exprStmt->expr);
                } else {
                    // 空表达式语句，返回null
                    SZrTypeValue nullValue;
                    ZrValueResetAsNull(&nullValue);
                    TUInt32 constantIndex = add_constant(cs, &nullValue);
                    TUInt32 destSlot = allocate_stack_slot(cs);
                    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
                    emit_instruction(cs, inst);
                }
            } else {
                // 其他类型的语句：编译后返回null
                compile_statement(cs, lastStmt);
                SZrTypeValue nullValue;
                ZrValueResetAsNull(&nullValue);
                TUInt32 constantIndex = add_constant(cs, &nullValue);
                TUInt32 destSlot = allocate_stack_slot(cs);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
                emit_instruction(cs, inst);
            }
        } else {
            // 最后一个语句为空，返回null
            SZrTypeValue nullValue;
            ZrValueResetAsNull(&nullValue);
            TUInt32 constantIndex = add_constant(cs, &nullValue);
            TUInt32 destSlot = allocate_stack_slot(cs);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
            emit_instruction(cs, inst);
        }
    } else {
        // 空块，返回null
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = add_constant(cs, &nullValue);
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 If 表达式
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IF_EXPRESSION) {
        ZrCompilerError(cs, "Expected if expression", node->location);
        return;
    }
    
    SZrIfExpression *ifExpr = &node->data.ifExpression;
    
    // 编译条件表达式
    compile_expression(cs, ifExpr->condition);
    TUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)condSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    if (ifExpr->thenExpr != ZR_NULL) {
        // 检查是否是块，如果是块则编译块并提取最后一个表达式的值
        if (ifExpr->thenExpr->type == ZR_AST_BLOCK) {
            compile_block_as_expression(cs, ifExpr->thenExpr);
        } else {
            compile_expression(cs, ifExpr->thenExpr);
        }
    } else {
        // 如果没有then分支，使用null值
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = add_constant(cs, &nullValue);
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    if (ifExpr->elseExpr != ZR_NULL) {
        // 检查是否是块，如果是块则编译块并提取最后一个表达式的值
        if (ifExpr->elseExpr->type == ZR_AST_BLOCK) {
            compile_block_as_expression(cs, ifExpr->elseExpr);
        } else if (ifExpr->elseExpr->type == ZR_AST_IF_EXPRESSION) {
            // else if 情况：递归编译
            compile_if_expression(cs, ifExpr->elseExpr);
        } else {
            compile_expression(cs, ifExpr->elseExpr);
        }
    } else {
        // 如果没有else分支，使用null值
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 constantIndex = add_constant(cs, &nullValue);
        TUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译 Switch 表达式
static void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_SWITCH_EXPRESSION) {
        ZrCompilerError(cs, "Expected switch expression", node->location);
        return;
    }
    
    SZrSwitchExpression *switchExpr = &node->data.switchExpression;
    
    // 编译 switch 表达式
    compile_expression(cs, switchExpr->expr);
    TUInt32 exprSlot = cs->stackSlotCount - 1;
    
    // 分配结果槽位（用于存储匹配的值）
    TUInt32 resultSlot = allocate_stack_slot(cs);
    
    // 创建结束标签
    TZrSize endLabelId = create_label(cs);
    
    // 编译所有 case
    TBool hasMatchedCase = ZR_FALSE;
    if (switchExpr->cases != ZR_NULL) {
        for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[i];
            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }
            
            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
            
            // 编译 case 值
            compile_expression(cs, switchCase->value);
            TUInt32 caseValueSlot = cs->stackSlotCount - 1;
            
            // 比较表达式和 case 值
            TUInt32 compareSlot = allocate_stack_slot(cs);
            TZrInstruction compareInst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL), compareSlot, (TUInt16)exprSlot, (TUInt16)caseValueSlot);
            emit_instruction(cs, compareInst);
            
            // 创建下一个 case 标签
            TZrSize nextCaseLabelId = create_label(cs);
            
            // JUMP_IF false -> next case
            TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TUInt16)compareSlot, 0);
            TZrSize jumpIfIndex = cs->instructionCount;
            emit_instruction(cs, jumpIfInst);
            add_pending_jump(cs, jumpIfIndex, nextCaseLabelId);
            
            // 释放临时栈槽（compareSlot 和 caseValueSlot）
            cs->stackSlotCount -= 2;
            
            // 编译 case 块（作为表达式，需要返回值）
            if (switchExpr->isStatement) {
                // 作为语句：编译块
                if (switchCase->block != ZR_NULL) {
                    compile_statement(cs, switchCase->block);
                }
                // 语句不需要返回值，直接跳转到结束
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
            } else {
                // 作为表达式：编译块，最后一个表达式作为返回值
                if (switchCase->block != ZR_NULL) {
                    SZrBlock *block = &switchCase->block->data.block;
                    if (block->body != ZR_NULL && block->body->count > 0) {
                        // 编译块中所有语句
                        for (TZrSize j = 0; j < block->body->count - 1; j++) {
                            SZrAstNode *stmt = block->body->nodes[j];
                            if (stmt != ZR_NULL) {
                                compile_statement(cs, stmt);
                            }
                        }
                        // 最后一个语句作为返回值（如果是表达式）
                        SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
                        if (lastStmt != ZR_NULL) {
                            // 尝试作为表达式编译
                            compile_expression(cs, lastStmt);
                            TUInt32 lastValueSlot = cs->stackSlotCount - 1;
                            // 复制到结果槽位
                            TZrInstruction copyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), resultSlot, (TInt32)lastValueSlot);
                            emit_instruction(cs, copyInst);
                            cs->stackSlotCount--; // 释放 lastValueSlot
                        }
                    } else {
                        // 空块，返回 null
                        SZrTypeValue nullValue;
                        ZrValueResetAsNull(&nullValue);
                        TUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                        TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TInt32)nullConstantIndex);
                        emit_instruction(cs, nullInst);
                    }
                } else {
                    // 空块，返回 null
                    SZrTypeValue nullValue;
                    ZrValueResetAsNull(&nullValue);
                    TUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                    TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TInt32)nullConstantIndex);
                    emit_instruction(cs, nullInst);
                }
                hasMatchedCase = ZR_TRUE;
                
                // 跳转到结束标签
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
            }
            
            // 解析下一个 case 标签
            resolve_label(cs, nextCaseLabelId);
        }
    }
    
    // 编译 default case
    if (switchExpr->defaultCase != ZR_NULL) {
        SZrSwitchDefault *defaultCase = &switchExpr->defaultCase->data.switchDefault;
        if (switchExpr->isStatement) {
            // 作为语句：编译块
            if (defaultCase->block != ZR_NULL) {
                compile_statement(cs, defaultCase->block);
            }
        } else {
            // 作为表达式：编译块，最后一个表达式作为返回值
            if (defaultCase->block != ZR_NULL) {
                SZrBlock *block = &defaultCase->block->data.block;
                if (block->body != ZR_NULL && block->body->count > 0) {
                    // 编译块中所有语句
                    for (TZrSize j = 0; j < block->body->count - 1; j++) {
                        SZrAstNode *stmt = block->body->nodes[j];
                        if (stmt != ZR_NULL) {
                            compile_statement(cs, stmt);
                        }
                    }
                    // 最后一个语句作为返回值
                    SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
                    if (lastStmt != ZR_NULL) {
                        compile_expression(cs, lastStmt);
                        TUInt32 lastValueSlot = cs->stackSlotCount - 1;
                        // 复制到结果槽位
                        TZrInstruction copyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), resultSlot, (TInt32)lastValueSlot);
                        emit_instruction(cs, copyInst);
                        cs->stackSlotCount--; // 释放 lastValueSlot
                    }
                } else {
                    // 空块，返回 null
                    SZrTypeValue nullValue;
                    ZrValueResetAsNull(&nullValue);
                    TUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                    TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TInt32)nullConstantIndex);
                    emit_instruction(cs, nullInst);
                }
            } else {
                // 空块，返回 null
                SZrTypeValue nullValue;
                ZrValueResetAsNull(&nullValue);
                TUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TInt32)nullConstantIndex);
                emit_instruction(cs, nullInst);
            }
            hasMatchedCase = ZR_TRUE;
        }
    } else if (!switchExpr->isStatement && !hasMatchedCase) {
        // 作为表达式但没有匹配的 case 也没有 default，返回 null
        SZrTypeValue nullValue;
        ZrValueResetAsNull(&nullValue);
        TUInt32 nullConstantIndex = add_constant(cs, &nullValue);
        TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TInt32)nullConstantIndex);
        emit_instruction(cs, nullInst);
    }
    
    // 释放表达式栈槽
    cs->stackSlotCount--;
    
    // 解析结束标签
    resolve_label(cs, endLabelId);
}

// 主编译表达式函数
void compile_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            compile_literal(cs, node);
            break;
        
        case ZR_AST_IDENTIFIER_LITERAL:
            compile_identifier(cs, node);
            break;
        
        // 表达式
        case ZR_AST_UNARY_EXPRESSION:
            compile_unary_expression(cs, node);
            break;
        
        case ZR_AST_TYPE_CAST_EXPRESSION:
            compile_type_cast_expression(cs, node);
            break;
        
        case ZR_AST_BINARY_EXPRESSION:
            compile_binary_expression(cs, node);
            break;
        
        case ZR_AST_LOGICAL_EXPRESSION:
            compile_logical_expression(cs, node);
            break;
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            compile_assignment_expression(cs, node);
            break;
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            compile_conditional_expression(cs, node);
            break;
        
        case ZR_AST_FUNCTION_CALL:
            compile_function_call(cs, node);
            break;
        
        case ZR_AST_MEMBER_EXPRESSION:
            compile_member_expression(cs, node);
            break;
        
        case ZR_AST_PRIMARY_EXPRESSION:
            compile_primary_expression(cs, node);
            break;
        
        case ZR_AST_ARRAY_LITERAL:
            compile_array_literal(cs, node);
            break;
        
        case ZR_AST_OBJECT_LITERAL:
            compile_object_literal(cs, node);
            break;
        
        case ZR_AST_LAMBDA_EXPRESSION:
            compile_lambda_expression(cs, node);
            break;
        
        case ZR_AST_BLOCK:
            // 块作为表达式使用时，编译块并提取最后一个表达式的值
            compile_block_as_expression(cs, node);
            break;
        
        // 控制流结构和语句不应该作为表达式编译，应该先转换为语句
        case ZR_AST_IF_EXPRESSION:
        case ZR_AST_SWITCH_EXPRESSION:
        case ZR_AST_WHILE_LOOP:
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP:
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
        case ZR_AST_OUT_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            ZrCompilerError(cs, "Loop or statement cannot be used as expression", node->location);
            break;
        
        // 解构对象和数组不是表达式，不应该在这里处理
        case ZR_AST_DESTRUCTURING_OBJECT:
        case ZR_AST_DESTRUCTURING_ARRAY:
            // 这些类型应该在变量声明中处理，不应该作为表达式编译
            ZrCompilerError(cs, "Destructuring pattern cannot be used as expression", node->location);
            break;
        
        default:
            // 未处理的表达式类型
            if (node->type == ZR_AST_DESTRUCTURING_OBJECT || node->type == ZR_AST_DESTRUCTURING_ARRAY) {
                // 这不应该作为表达式编译，应该在变量声明中处理
                ZrCompilerError(cs, "Destructuring pattern cannot be used as expression", node->location);
            } else {
                // 创建详细的错误消息，包含类型名称和位置信息
                static TChar errorMsg[256];
                const TChar *typeName = "UNKNOWN";
                switch (node->type) {
                    case ZR_AST_INTERFACE_METHOD_SIGNATURE: typeName = "INTERFACE_METHOD_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_FIELD_DECLARATION: typeName = "INTERFACE_FIELD_DECLARATION"; break;
                    case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: typeName = "INTERFACE_PROPERTY_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_META_SIGNATURE: typeName = "INTERFACE_META_SIGNATURE"; break;
                    case ZR_AST_STRUCT_FIELD: typeName = "STRUCT_FIELD"; break;
                    case ZR_AST_STRUCT_METHOD: typeName = "STRUCT_METHOD"; break;
                    case ZR_AST_STRUCT_META_FUNCTION: typeName = "STRUCT_META_FUNCTION"; break;
                    case ZR_AST_CLASS_FIELD: typeName = "CLASS_FIELD"; break;
                    case ZR_AST_CLASS_METHOD: typeName = "CLASS_METHOD"; break;
                    case ZR_AST_CLASS_PROPERTY: typeName = "CLASS_PROPERTY"; break;
                    case ZR_AST_CLASS_META_FUNCTION: typeName = "CLASS_META_FUNCTION"; break;
                    case ZR_AST_FUNCTION_DECLARATION: typeName = "FUNCTION_DECLARATION"; break;
                    case ZR_AST_STRUCT_DECLARATION: typeName = "STRUCT_DECLARATION"; break;
                    case ZR_AST_CLASS_DECLARATION: typeName = "CLASS_DECLARATION"; break;
                    case ZR_AST_INTERFACE_DECLARATION: typeName = "INTERFACE_DECLARATION"; break;
                    case ZR_AST_ENUM_DECLARATION: typeName = "ENUM_DECLARATION"; break;
                    case ZR_AST_ENUM_MEMBER: typeName = "ENUM_MEMBER"; break;
                    case ZR_AST_MODULE_DECLARATION: typeName = "MODULE_DECLARATION"; break;
                    case ZR_AST_SCRIPT: typeName = "SCRIPT"; break;
                    default: break;
                }
                snprintf(errorMsg, sizeof(errorMsg), 
                        "Unexpected expression type: %s (type %d) at line %d:%d. "
                        "This node type should not be compiled as an expression. "
                        "Please check if it was incorrectly placed in an expression context.",
                        typeName, node->type, 
                        node->location.start.line, node->location.start.column);
                ZrCompilerError(cs, errorMsg, node->location);
            }
            break;
    }
}

