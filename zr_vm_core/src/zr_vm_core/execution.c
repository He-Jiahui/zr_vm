//
// Created by HeJiahui on 2025/6/15.
//

#include "zr_vm_core/execution.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_object_conf.h"

// 辅助函数：从模块中查找类型原型
// 返回找到的原型对象，如果未找到返回 ZR_NULL
static SZrObjectPrototype *find_prototype_in_module(SZrState *state, struct SZrObjectModule *module, 
                                                     SZrString *typeName, EZrObjectPrototypeType expectedType) {
    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从模块的 pub 导出中查找类型
    const SZrTypeValue *typeValue = ZrModuleGetPubExport(state, module, typeName);
    if (typeValue == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 检查值类型是否为对象
    if (typeValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *typeObject = ZR_CAST_OBJECT(state, typeValue->value.object);
    if (typeObject == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 检查对象是否为原型对象
    if (typeObject->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)typeObject;
    
    // 检查原型类型是否匹配
    if (prototype->type == expectedType) {
        return prototype;
    }
    
    return ZR_NULL;
}

// 辅助函数：解析类型名称，支持 "module.TypeName" 格式
// 返回解析后的模块名和类型名，如果类型名不包含模块路径，moduleName 返回 ZR_NULL
static void parse_type_name(SZrState *state, SZrString *fullTypeName, SZrString **moduleName, SZrString **typeName) {
    if (state == ZR_NULL || fullTypeName == ZR_NULL) {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = ZR_NULL;
        return;
    }
    
    // 获取类型名称字符串
    TNativeString typeNameStr;
    TZrSize nameLen;
    if (fullTypeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeNameStr = (TNativeString)ZrStringGetNativeStringShort(fullTypeName);
        nameLen = fullTypeName->shortStringLength;
    } else {
        typeNameStr = (TNativeString)ZrStringGetNativeString(fullTypeName);
        nameLen = fullTypeName->longStringLength;
    }
    
    if (typeNameStr == ZR_NULL || nameLen == 0) {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
        return;
    }
    
    // 查找 '.' 分隔符
    const TChar *dotPos = (const TChar *)memchr(typeNameStr, '.', nameLen);
    if (dotPos == ZR_NULL) {
        // 没有模块路径，类型名就是完整名称
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
        return;
    }
    
    // 解析模块名和类型名
    TZrSize moduleNameLen = (TZrSize)(dotPos - typeNameStr);
    TZrSize typeNameLen = nameLen - moduleNameLen - 1;
    const TChar *typeNameStart = dotPos + 1;
    
    if (moduleNameLen > 0 && typeNameLen > 0) {
        if (moduleName != ZR_NULL) {
            *moduleName = ZrStringCreate(state, typeNameStr, moduleNameLen);
        }
        if (typeName != ZR_NULL) {
            *typeName = ZrStringCreate(state, typeNameStart, typeNameLen);
        }
    } else {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
    }
}

// 辅助函数：查找类型原型（从当前模块或全局模块注册表）
// 返回找到的原型对象，如果未找到返回 ZR_NULL
static SZrObjectPrototype *find_type_prototype(SZrState *state, SZrString *typeName, EZrObjectPrototypeType expectedType) {
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析类型名称，支持 "module.TypeName" 格式
    SZrString *moduleName = ZR_NULL;
    SZrString *actualTypeName = ZR_NULL;
    parse_type_name(state, typeName, &moduleName, &actualTypeName);
    
    if (actualTypeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果指定了模块名，从该模块中查找
    if (moduleName != ZR_NULL) {
        // 从模块注册表中获取模块
        struct SZrObjectModule *module = ZrModuleGetFromCache(state, moduleName);
        if (module != ZR_NULL) {
            SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
            return prototype;
        }
    } else {
        // 没有指定模块名，尝试从当前调用栈的闭包中查找模块
        // 遍历调用栈，查找模块对象（通过闭包变量或其他方式）
        // TODO: 如果函数有模块信息，从模块中查找类型
        // 目前先跳过，因为需要额外的机制来关联函数和模块
        
        // 从全局模块注册表中查找（遍历所有已加载的模块）
        // 注意：由于 ZrHashSet 没有迭代接口，我们需要通过其他方式查找
        // 一个可能的方案是：在模块加载时，将类型原型注册到全局类型表中
        // 或者：通过类型名称的哈希值在注册表中查找对应的模块
        
        // 暂时先尝试从全局 zr 对象中查找（如果类型是全局注册的）
        // 或者：通过元方法机制，让类型系统提供查找功能
    }
    
    // 如果找不到，返回 ZR_NULL（后续可以通过元方法或创建新原型）
    // 注意：完整的实现需要：
    // - 模块加载时将类型原型注册到全局类型表
    // - 或者通过类型名称的模块路径（如 "module.TypeName"）来查找
    return ZR_NULL;
}

// 辅助函数：执行 struct 类型转换
// 将源对象转换为目标 struct 类型
static TBool convert_to_struct(SZrState *state, SZrTypeValue *source, SZrObjectPrototype *targetPrototype, 
                                SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查目标原型类型
    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return ZR_FALSE;
    }
    
    // 如果源值是对象，尝试转换
    if (ZR_VALUE_IS_TYPE_OBJECT(source->type)) {
        SZrObject *sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 创建新的 struct 对象（值类型）
        SZrObject *structObject = ZrObjectNew(state, targetPrototype);
        if (structObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 设置内部类型为 STRUCT
        structObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
        
        // 复制源对象的字段到新对象
        // 对于 struct，字段存储在 nodeMap 中（与普通对象相同）
        // 遍历源对象的 nodeMap，复制匹配的字段到新对象
        if (sourceObject->nodeMap.isValid && sourceObject->nodeMap.buckets != ZR_NULL && sourceObject->nodeMap.elementCount > 0) {
            // 注意：ZrHashSet 没有迭代接口，我们需要通过其他方式复制字段
            // 一个方案是：通过元方法 @to_struct 来处理字段复制
            // 或者：如果源对象已经是 struct 类型，直接复制其 nodeMap
            
            // 暂时先复制所有字段（后续需要根据 struct 定义进行字段验证和类型转换）
            // 由于无法直接迭代 nodeMap，我们依赖元方法或构造函数来处理字段复制
            // 如果源对象有 @to_struct 元方法，应该已经在上层调用了
            // 这里只是创建了新的 struct 对象，字段复制由元方法或构造函数完成
        }
        
        ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(structObject));
        destination->type = ZR_VALUE_TYPE_OBJECT;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

// 辅助函数：执行 class 类型转换
// 将源对象转换为目标 class 类型
static TBool convert_to_class(SZrState *state, SZrTypeValue *source, SZrObjectPrototype *targetPrototype, 
                               SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查目标原型类型
    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        return ZR_FALSE;
    }
    
    // 如果源值是对象，尝试转换
    if (ZR_VALUE_IS_TYPE_OBJECT(source->type)) {
        SZrObject *sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 创建新的 class 对象（引用类型）
        SZrObject *classObject = ZrObjectNew(state, targetPrototype);
        if (classObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 设置内部类型为 OBJECT（class 是引用类型）
        classObject->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
        
        // 复制源对象的字段到新对象
        // 对于 class，字段存储在 nodeMap 中
        // 遍历源对象的 nodeMap，复制匹配的字段到新对象
        if (sourceObject->nodeMap.isValid && sourceObject->nodeMap.buckets != ZR_NULL && sourceObject->nodeMap.elementCount > 0) {
            // 注意：ZrHashSet 没有迭代接口，我们需要通过其他方式复制字段
            // 一个方案是：通过元方法 @to_object 来处理字段复制
            // 或者：如果源对象已经是 class 类型，直接复制其 nodeMap
            
            // 暂时先复制所有字段（后续需要根据 class 定义进行字段验证和类型转换）
            // 由于无法直接迭代 nodeMap，我们依赖元方法或构造函数来处理字段复制
            // 如果源对象有 @to_object 元方法，应该已经在上层调用了
            // 这里只是创建了新的 class 对象，字段复制由元方法或构造函数完成
        }
        
        ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(classObject));
        destination->type = ZR_VALUE_TYPE_OBJECT;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

void ZrExecute(SZrState *state, SZrCallInfo *callInfo) {
    SZrClosure *closure;
    SZrTypeValue *constants;
    TZrStackValuePointer base;
    SZrTypeValue ret;
    ZrValueResetAsNull(&ret);
    const TZrInstruction *programCounter;
    TZrDebugSignal trap;
    SZrTypeValue *opA;
    SZrTypeValue *opB;
    /*
     * registers macros
     */

    /*
     *
     */
    ZR_INSTRUCTION_DISPATCH_TABLE
#define DONE(N) ZR_INSTRUCTION_DONE(instruction, programCounter, N)
// extra operand
#define E(INSTRUCTION) INSTRUCTION.instruction.operandExtra
// 4 OPERANDS
#define A0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[0]
#define B0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[1]
#define C0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[2]
#define D0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[3]
// 2 OPERANDS
#define A1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[0]
#define B1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[1]
// 1 OPERAND
#define A2(INSTRUCTION) INSTRUCTION.instruction.operand.operand2[0]

#define BASE(OFFSET) (base + (OFFSET))
#define CONST(OFFSET) (constants + (OFFSET))
#define CLOSURE(OFFSET) (closure->closureValuesExtend[OFFSET])

#define ALGORITHM_1(REGION, OP, TYPE) ZR_VALUE_FAST_SET(destination, REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2(REGION, OP, TYPE)                                                                                  \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2(CVT, REGION, OP, TYPE)                                                                         \
    ZR_VALUE_FAST_SET(destination, CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2(REGION, OP, TYPE, RIGHT)                                                                     \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2(REGION, OP_FUNC, TYPE)                                                                        \
    ZR_VALUE_FAST_SET(destination, REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION),    \
                      TYPE);

#define UPDATE_TRAP(CALL_INFO) (trap = (CALL_INFO)->context.context.trap)
#define UPDATE_BASE(CALL_INFO) (base = (CALL_INFO)->functionBase.valuePointer + 1)
#define UPDATE_STACK(CALL_INFO)                                                                                        \
    {                                                                                                                  \
        if (ZR_UNLIKELY(trap)) {                                                                                       \
            UPDATE_BASE(CALL_INFO);                                                                                    \
        }                                                                                                              \
    }
#define SAVE_PC(STATE, CALL_INFO) ((CALL_INFO)->context.context.programCounter = programCounter)
#define SAVE_STATE(STATE, CALL_INFO)                                                                                   \
    (SAVE_PC(STATE, CALL_INFO), ((STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer))
    // MODIFIABLE: ERROR & STACK & HOOK
#if defined(_MSC_VER)
    // MSVC 不支持语句表达式，使用 do-while 循环
    #define PROTECT_ESH(STATE, CALL_INFO, EXP) \
        do { \
            SAVE_PC(STATE, CALL_INFO); \
            (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer; \
            EXP; \
            UPDATE_TRAP(CALL_INFO); \
        } while(0)
    #define PROTECT_EH(STATE, CALL_INFO, EXP) \
        do { \
            SAVE_PC(STATE, CALL_INFO); \
            EXP; \
            UPDATE_TRAP(CALL_INFO); \
        } while(0)
    #define PROTECT_E(STATE, CALL_INFO, EXP) \
        do { \
            SAVE_PC(STATE, CALL_INFO); \
            (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer; \
            EXP; \
        } while(0)
#else
    // GCC/Clang 支持语句表达式
    #define PROTECT_ESH(STATE, CALL_INFO, EXP) (SAVE_STATE(STATE, CALL_INFO), (EXP), UPDATE_TRAP(CALL_INFO))
    #define PROTECT_EH(STATE, CALL_INFO, EXP) (SAVE_PC(STATE, CALL_INFO), (EXP), UPDATE_TRAP(CALL_INFO))
    #define PROTECT_E(STATE, CALL_INFO, EXP) (SAVE_STATE(STATE, CALL_INFO), (EXP))
#endif

#define JUMP(CALL_INFO, INSTRUCTION, OFFSET)                                                                           \
    {                                                                                                                  \
        programCounter += A2(INSTRUCTION) + (OFFSET);                                                                  \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    }

LZrStart:
    trap = state->debugHookSignal;
LZrReturning: {
    SZrTypeValue *functionBaseValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
    closure = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
    constants = closure->function->constantValueList;
    programCounter = callInfo->context.context.programCounter - 1;
    base = callInfo->functionBase.valuePointer + 1;
}
    if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {
        // todo
    }
    for (;;) {

        TZrInstruction instruction;
        /*
         * fetch instruction
         */
        ZR_INSTRUCTION_FETCH(instruction, programCounter, trap = ZrDebugTraceExecution(state, programCounter);
                             UPDATE_STACK(callInfo), 1);
        // 检查 programCounter 是否超出指令范围
        const TZrInstruction *instructionsEnd =
                closure->function->instructionsList + closure->function->instructionsLength;
        if (ZR_UNLIKELY(programCounter >= instructionsEnd)) {
            // 超出指令范围，退出循环（相当于隐式返回）
            break;
        }
        // debug line
#if ZR_DEBUG
#endif
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);
        ZR_ASSERT(base <= state->stackTop.valuePointer &&
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);

        SZrTypeValue *destination = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : &BASE(E(instruction))->value;

        ZR_INSTRUCTION_DISPATCH(instruction) {
            ZR_INSTRUCTION_LABEL(GET_STACK) { *destination = BASE(A2(instruction))->value; }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_STACK) { BASE(A2(instruction))->value = *destination; }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // BASE(B1(instruction))->value = *CONST(ret.value.nativeObject.nativeUInt64);
                *destination = *CONST(A2(instruction));
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                //*CONST(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                *CONST(A2(instruction)) = *destination;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // closure function to access
                ZrValueCopy(state, destination, ZrClosureValueGetValue(CLOSURE(A2(instruction))));
                // BASE(B1(instruction))->value = CLOSURE(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                SZrClosureValue *closureValue = CLOSURE(A2(instruction));
                SZrTypeValue *value = ZrClosureValueGetValue(closureValue);
                SZrTypeValue *newValue = destination;
                // closure function to access
                ZrValueCopy(state, value, newValue);
                // CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                ZrValueBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue), newValue);
                // *CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_BOOL) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_TO_BOOL);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 2, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        state->stackTop.valuePointer = metaBase + 2;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            if (returnValue->type == ZR_VALUE_TYPE_BOOL) {
                                ZrValueCopy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeInt64 != 0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeUInt64 != 0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeDouble != 0.0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_STRING(opA->type)) {
                        SZrString *str = ZR_CAST_STRING(state, opA->value.object);
                        TZrSize len = (str->shortStringLength < 0xFF) ? str->shortStringLength : str->longStringLength;
                        ZR_VALUE_FAST_SET(destination, nativeBool, len > 0, ZR_VALUE_TYPE_BOOL);
                    } else {
                        // 对象类型，默认返回 true
                        ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_INT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_TO_INT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 2, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        state->stackTop.valuePointer = metaBase + 2;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrValueCopy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrValueInitAsInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrValueInitAsInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZrValueInitAsInt(state, destination, (TInt64) opA->value.nativeObject.nativeUInt64);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrValueInitAsInt(state, destination, (TInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrValueInitAsInt(state, destination, opA->value.nativeObject.nativeBool ? 1 : 0);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrValueInitAsInt(state, destination, 0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_UINT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_TO_UINT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 2, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        state->stackTop.valuePointer = metaBase + 2;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrValueCopy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrValueInitAsUInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrValueInitAsUInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZrValueInitAsUInt(state, destination, (TUInt64) opA->value.nativeObject.nativeInt64);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrValueInitAsUInt(state, destination, (TUInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrValueInitAsUInt(state, destination, opA->value.nativeObject.nativeBool ? 1 : 0);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrValueInitAsUInt(state, destination, 0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_TO_FLOAT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 2, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        state->stackTop.valuePointer = metaBase + 2;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_FLOAT(returnValue->type)) {
                                ZrValueCopy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrValueInitAsFloat(state, destination, 0.0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrValueInitAsFloat(state, destination, 0.0);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZrValueInitAsFloat(state, destination, (TFloat64) opA->value.nativeObject.nativeInt64);
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZrValueInitAsFloat(state, destination, (TFloat64) opA->value.nativeObject.nativeUInt64);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrValueInitAsFloat(state, destination, opA->value.nativeObject.nativeBool ? 1.0 : 0.0);
                    } else {
                        // 其他类型无法转换，返回 0.0
                        ZrValueInitAsFloat(state, destination, 0.0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRING) {
                opA = &BASE(A1(instruction))->value;
                SZrString *result = ZrValueConvertToString(state, opA);
                if (result != ZR_NULL) {
                    ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                } else {
                    // 转换失败，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRUCT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TUInt16 typeNameConstantIndex = B1(instruction);
                if (closure->function != ZR_NULL && typeNameConstantIndex < closure->function->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 注意：ZR_META_TO_STRUCT 可能不存在，暂时不使用元方法，直接查找原型
                        // TODO: 如果定义了 ZR_META_TO_STRUCT，使用它；否则直接查找原型
                        SZrMeta *meta = ZR_NULL; // ZrValueGetMeta(state, opA, ZR_META_TO_STRUCT);
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                                TZrStackValuePointer metaBase = savedStackTop;
                                ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                                ZrStackCopyValue(state, metaBase + 1, opA);
                                ZrStackCopyValue(state, metaBase + 2, typeNameValue);
                                state->stackTop.valuePointer = metaBase + 3;
                                ZrFunctionCallWithoutYield(state, metaBase, 1);
                                if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                                    SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                                    ZrValueCopy(state, destination, returnValue);
                                } else {
                                    ZrValueResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = savedStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                if (!convert_to_struct(state, opA, prototype, destination)) {
                                    ZrValueResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 struct（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrValueCopy(state, destination, opA);
                                } else {
                                    ZrValueResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrValueResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_OBJECT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TUInt16 typeNameConstantIndex = B1(instruction);
                if (closure->function != ZR_NULL && typeNameConstantIndex < closure->function->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 注意：ZR_META_TO_OBJECT 可能不存在，暂时不使用元方法，直接查找原型
                        // TODO: 如果定义了 ZR_META_TO_OBJECT，使用它；否则直接查找原型
                        SZrMeta *meta = ZR_NULL; // ZrValueGetMeta(state, opA, ZR_META_TO_OBJECT);
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                                TZrStackValuePointer metaBase = savedStackTop;
                                ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                                ZrStackCopyValue(state, metaBase + 1, opA);
                                ZrStackCopyValue(state, metaBase + 2, typeNameValue);
                                state->stackTop.valuePointer = metaBase + 3;
                                ZrFunctionCallWithoutYield(state, metaBase, 1);
                                if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                                    SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                                    ZrValueCopy(state, destination, returnValue);
                                } else {
                                    ZrValueResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = savedStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                if (!convert_to_class(state, opA, prototype, destination)) {
                                    ZrValueResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 class（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrValueCopy(state, destination, opA);
                                } else {
                                    ZrValueResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrValueResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_ADD);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, +, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, +, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_STRING) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type));
                SZrString *str1 = ZR_CAST_STRING(state, opA->value.object);
                SZrString *str2 = ZR_CAST_STRING(state, opB->value.object);
                TNativeString native1 = ZrStringGetNativeString(str1);
                TNativeString native2 = ZrStringGetNativeString(str2);
                TZrSize len1 = (str1->shortStringLength < 0xFF) ? str1->shortStringLength : str1->longStringLength;
                TZrSize len2 = (str2->shortStringLength < 0xFF) ? str2->shortStringLength : str2->longStringLength;
                TZrSize totalLen = len1 + len2;
                char *buffer = (char *) malloc(totalLen + 1);
                if (buffer != ZR_NULL) {
                    memcpy(buffer, native1, len1);
                    memcpy(buffer + len1, native2, len2);
                    buffer[totalLen] = '\0';
                    SZrString *result = ZrStringCreateFromNative(state, buffer);
                    free(buffer);
                    if (result != ZR_NULL) {
                        ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                    } else {
                        ZrValueResetAsNull(destination);
                    }
                } else {
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_SUB);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, -, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, -, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_MUL);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, *, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, *, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, *, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(NEG) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_NEG);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 2, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        state->stackTop.valuePointer = metaBase + 2;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_DIV);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: divide by zero
                if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                    // ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                    ZrDebugRunError(state, "divide by zero");
                }
                ALGORITHM_2(nativeInt64, /, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: divide by zero
                if (ZR_UNLIKELY(opB->value.nativeObject.nativeUInt64 == 0)) {
                    // ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                    ZrDebugRunError(state, "divide by zero");
                }
                ALGORITHM_2(nativeUInt64, /, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, /, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_MOD);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: modulo by zero
                if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                    ZrDebugRunError(state, "modulo by zero");
                }
                TInt64 divisor = opB->value.nativeObject.nativeInt64;
                if (ZR_UNLIKELY(divisor < 0)) {
                    divisor = -divisor;
                }
                ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: modulo by zero
                if (opB->value.nativeObject.nativeUInt64 == 0) {
                    ZrDebugRunError(state, "modulo by zero");
                }
                ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_FUNC_2(nativeDouble, fmod, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_POW);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: power domain error
                TInt64 valueA = opA->value.nativeObject.nativeInt64;
                TInt64 valueB = opB->value.nativeObject.nativeInt64;
                if (ZR_UNLIKELY(valueA == 0 && valueB <= 0)) {
                    ZrDebugRunError(state, "power domain error");
                }
                if (ZR_UNLIKELY(valueA < 0)) {
                    ZrDebugRunError(state, "power domain error");
                }
                ALGORITHM_FUNC_2(nativeInt64, ZrMathIntPower, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: power domain error
                TUInt64 valueA = opA->value.nativeObject.nativeUInt64;
                TUInt64 valueB = opB->value.nativeObject.nativeUInt64;
                if (ZR_UNLIKELY(valueA == 0 && valueB == 0)) {
                    ZrDebugRunError(state, "power domain error");
                }
                ALGORITHM_FUNC_2(nativeUInt64, ZrMathUIntPower, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_FUNC_2(nativeDouble, pow, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_SHIFT_LEFT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrValueGetMeta(state, opA, ZR_META_SHIFT_RIGHT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        ZrFunctionCheckStackAndGc(state, 3, savedStackTop);
                        TZrStackValuePointer metaBase = savedStackTop;
                        ZrStackSetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
                        ZrStackCopyValue(state, metaBase + 1, opA);
                        ZrStackCopyValue(state, metaBase + 2, opB);
                        state->stackTop.valuePointer = metaBase + 3;
                        ZrFunctionCallWithoutYield(state, metaBase, 1);
                        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
                            SZrTypeValue *returnValue = ZrStackGetValue(metaBase);
                            ZrValueCopy(state, destination, returnValue);
                        } else {
                            ZrValueResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = savedStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL, ZR_TRUE);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    ALGORITHM_1(nativeBool, !, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL,
                                      opA->value.nativeObject.nativeInt64 == 0);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL,
                                      opA->value.nativeObject.nativeDouble == 0);
                } else {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL, ZR_FALSE);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, &&, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, ||, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TBool result = ZrValueEqual(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TBool result = !ZrValueEqual(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_NOT) {
                opA = &BASE(A1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type));
                ALGORITHM_1(nativeInt64, ~, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, &, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, |, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_XOR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, ^, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_CALL) {
                // FUNCTION_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize returnCount = E(instruction);  // 返回值数量
                
                opA = &BASE(functionSlot)->value;
                // 检查函数值是否为空
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                // 函数类型可以是 ZR_VALUE_TYPE_FUNCTION 或 ZR_VALUE_TYPE_CLOSURE
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FUNCTION(opA->type) || ZR_VALUE_IS_TYPE_CLOSURE(opA->type));
                
                // 设置栈顶指针（函数在 functionSlot，参数在 functionSlot+1 到 functionSlot+parametersCount）
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                SZrCallInfo *nextCallInfo = ZrFunctionPreCall(state, BASE(functionSlot), returnCount);
                if (nextCallInfo == ZR_NULL) {
                    // NULL means native call
                    trap = callInfo->context.context.trap;
                } else {
                    // a vm call
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_TAIL_CALL) {
                // FUNCTION_TAIL_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize returnCount = E(instruction);  // 返回值数量
                
                opA = &BASE(functionSlot)->value;
                // 检查函数值是否为空
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                // 函数类型可以是 ZR_VALUE_TYPE_FUNCTION 或 ZR_VALUE_TYPE_CLOSURE
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FUNCTION(opA->type) || ZR_VALUE_IS_TYPE_CLOSURE(opA->type));
                
                // 设置栈顶指针
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // 尾调用：重用当前调用帧
                // 保存当前程序计数器
                callInfo->context.context.programCounter = programCounter;
                // 设置尾调用标志
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                // 准备调用参数（函数在BASE(functionSlot)，参数在BASE(functionSlot+1)到BASE(functionSlot+parametersCount)）
                TZrStackValuePointer functionPointer = BASE(functionSlot);
                // 调用函数（重用当前callInfo）
                SZrCallInfo *nextCallInfo = ZrFunctionPreCall(state, functionPointer, returnCount);
                if (nextCallInfo == ZR_NULL) {
                    // Native调用，清除尾调用标志
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    trap = callInfo->context.context.trap;
                } else {
                    // VM调用：对于尾调用，重用当前callInfo而不是创建新的
                    // 但ZrFunctionPreCall总是创建新的callInfo，所以我们需要调整
                    // 实际上，对于真正的尾调用优化，我们需要手动设置callInfo的字段
                    // 这里先使用简单的实现：清除尾调用标志，使用普通调用
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_RETURN) {
                // FUNCTION_RETURN 指令格式：
                // operandExtra (E) = 返回值数量 (returnCount)
                // operand1[0] (A1) = 返回值槽位 (resultSlot)
                // operand1[1] (B1) = 可变参数参数数量 (variableArguments, 0 表示非可变参数函数)
                TZrSize returnCount = E(instruction);
                TZrSize resultSlot = A1(instruction);
                TZrSize variableArguments = B1(instruction);

                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                // means the flag of closures to be closed
                // if (A1(instruction)) {
                //     callInfo->yieldContext.returnValueCount = returnCount;
                //     if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
                //         state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                //     }
                //     // todo close closure values:
                //
                //     trap = callInfo->context.context.trap;
                //     if (ZR_UNLIKELY(trap)) {
                //         base = callInfo->functionBase.valuePointer + 1;
                //     }
                // }
                // 如果是可变参数函数，需要调整 functionBase 指针
                // 参考 Lua: if (nparams1) ci->func.p -= ci->u.l.nextraargs + nparams1;
                if (variableArguments > 0) {
                    callInfo->functionBase.valuePointer -=
                            callInfo->context.context.variableArgumentCount + variableArguments;
                }
                state->stackTop.valuePointer = BASE(resultSlot) + returnCount;
                ZrFunctionPostCall(state, callInfo, returnCount);
                trap = callInfo->context.context.trap;
                goto LZrReturn;
            }

        LZrReturn: {
            // return from vm
            if (callInfo->callStatus & ZR_CALL_STATUS_CREATE_FRAME) {
                return;
            } else {
                callInfo = callInfo->previous;
                goto LZrReturning;
            }
        }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GETUPVAL) {
                // GETUPVAL 指令格式：
                // operandExtra (E) = destination slot
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrStackGetValue(base - 1)->value.object);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrDebugRunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    // 如果闭包值为 NULL，尝试初始化（这可能是第一次访问）
                    // 注意：这不应该发生在正常执行中，但为了测试的兼容性，我们允许这种情况
                    ZrDebugRunError(state, "upvalue is null - closure values may not be initialized");
                }
                ZrValueCopy(state, destination, ZrClosureValueGetValue(closureValue));
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SETUPVAL) {
                // SETUPVAL 指令格式：
                // operandExtra (E) = source slot (destination)
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrStackGetValue(base - 1)->value.object);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrDebugRunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    ZrDebugRunError(state, "upvalue is null");
                }
                SZrTypeValue *target = ZrClosureValueGetValue(closureValue);
                ZrValueCopy(state, target, destination);
                ZrValueBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentClosure->closureValuesExtend[upvalueIndex]),
                               destination);
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_SUB_FUNCTION) {
                // GET_SUB_FUNCTION 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = childFunctionIndex (子函数在 childFunctionList 中的索引)
                // operand1[1] (B1) = 0 (未使用)
                // GET_SUB_FUNCTION 用于从父函数的 childFunctionList 中通过索引获取子函数并压入栈
                // 这是编译时确定的静态索引，运行时直接通过索引访问，无需名称查找
                // 注意：GET_SUB_FUNCTION 只操作函数类型（ZR_VALUE_TYPE_FUNCTION 或 ZR_VALUE_TYPE_CLOSURE）
                TZrSize childFunctionIndex = A1(instruction);
                
                // 获取父函数的 callInfo
                SZrCallInfo *parentCallInfo = callInfo->previous;
                TBool found = ZR_FALSE;
                
                if (parentCallInfo != ZR_NULL && ZR_CALL_INFO_IS_VM(parentCallInfo)) {
                    // 获取父函数的闭包和函数
                    SZrTypeValue *parentFunctionBaseValue = ZrStackGetValue(parentCallInfo->functionBase.valuePointer);
                    if (parentFunctionBaseValue != ZR_NULL) {
                        // 类型检查：确保父函数是函数类型或闭包类型
                        if (parentFunctionBaseValue->type == ZR_VALUE_TYPE_FUNCTION || 
                            parentFunctionBaseValue->type == ZR_VALUE_TYPE_CLOSURE) {
                            SZrClosure *parentClosure = ZR_CAST_VM_CLOSURE(state, parentFunctionBaseValue->value.object);
                            if (parentClosure != ZR_NULL && parentClosure->function != ZR_NULL) {
                                SZrFunction *parentFunction = parentClosure->function;
                                
                                // 通过索引直接访问 childFunctionList
                                if (childFunctionIndex < parentFunction->childFunctionLength) {
                                    SZrFunction *childFunction = &parentFunction->childFunctionList[childFunctionIndex];
                                    if (childFunction != ZR_NULL && 
                                        childFunction->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                                        // 创建闭包对象
                                        SZrClosure *childClosure = ZrClosureNew(state, 0);
                                        if (childClosure != ZR_NULL) {
                                            childClosure->function = childFunction;
                                            ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(childClosure));
                                            destination->type = ZR_VALUE_TYPE_CLOSURE;
                                            destination->isGarbageCollectable = ZR_TRUE;
                                            destination->isNative = ZR_FALSE;
                                            found = ZR_TRUE;
                                        }
                                    }
                                }
                            }
                        } else {
                            // 类型错误：父函数不是函数类型
                            ZrDebugRunError(state, "GET_SUB_FUNCTION: parent must be a function or closure");
                        }
                    }
                }
                
                // 如果没找到，返回 null
                if (!found) {
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);


            ZR_INSTRUCTION_LABEL(GET_GLOBAL) {
                // GET_GLOBAL 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = 0 (未使用)
                // operand1[1] (B1) = 0 (未使用)
                // GET_GLOBAL 用于获取全局 zr 对象到堆栈
                SZrGlobalState *global = state->global;
                if (global != ZR_NULL && global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
                    ZrValueCopy(state, destination, &global->zrObject);
                } else {
                    // 如果 zr 对象未初始化，返回 null
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GETTABLE) {
                // GETTABLE 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = tableSlot (对象在栈中的位置)
                // operand1[1] (B1) = keySlot (键在栈中的位置)
                // GETTABLE 用于从 object 的键值对（nodeMap）中获取值
                // 注意：GETTABLE 只操作对象类型（ZR_VALUE_TYPE_OBJECT 或 ZR_VALUE_TYPE_ARRAY）
                opA = &BASE(A1(instruction))->value; // table object
                opB = &BASE(B1(instruction))->value; // key
                
                // 类型检查：确保 table 是对象类型或数组类型
                if (opA->type == ZR_VALUE_TYPE_OBJECT || opA->type == ZR_VALUE_TYPE_ARRAY) {
                    const SZrTypeValue *result = ZrObjectGetValue(state, ZR_CAST_OBJECT(state, opA->value.object), opB);
                    if (result != ZR_NULL) {
                        ZrValueCopy(state, destination, result);
                    } else {
                        ZrValueResetAsNull(destination);
                    }
                } else {
                    // 类型错误：table 不是对象类型
                    ZrDebugRunError(state, "GETTABLE: table must be an object or array");
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SETTABLE) {
                opA = &BASE(A1(instruction))->value; // table object
                opB = &BASE(B1(instruction))->value; // key
                ZrObjectSetValue(state, ZR_CAST_OBJECT(state, opA->value.object), opB, destination);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP) { JUMP(callInfo, instruction, 0); }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP_IF) {
                if (destination->value.nativeObject.nativeBool) {
                    JUMP(callInfo, instruction, 0);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_CLOSURE) {
                // CREATE_CLOSURE 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = functionConstantIndex
                // operand1[1] (B1) = closureVarCount
                TZrSize functionConstantIndex = A1(instruction);
                TZrSize closureVarCount = B1(instruction);
                SZrTypeValue *functionConstant = CONST(functionConstantIndex);
                // 从常量池获取函数对象
                // 注意：编译器将SZrFunction*存储为ZR_VALUE_TYPE_CLOSURE类型，但value.object实际指向SZrFunction*
                SZrFunction *function = ZR_NULL;
                if (functionConstant->type == ZR_VALUE_TYPE_CLOSURE ||
                    functionConstant->type == ZR_VALUE_TYPE_FUNCTION) {
                    // 从raw object获取实际的函数对象
                    SZrRawObject *rawObject = functionConstant->value.object;
                    if (rawObject != ZR_NULL && rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                        function = ZR_CAST(SZrFunction *, rawObject);
                    }
                }
                if (function != ZR_NULL) {
                    // 创建闭包对象
                    SZrClosure *closure = ZrClosureNew(state, closureVarCount);
                    closure->function = function;
                    // 初始化闭包值
                    if (closureVarCount > 0) {
                        ZrClosureInitValue(state, closure);
                    }
                    ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
                } else {
                    // 类型错误或函数为NULL
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_OBJECT) {
                // 创建空对象
                SZrObject *object = ZrObjectNew(state, ZR_NULL);
                if (object != ZR_NULL) {
                    ZrObjectInit(state, object);
                    ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
                } else {
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_ARRAY) {
                // 创建空数组对象
                SZrObject *array = ZrObjectNewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
                if (array != ZR_NULL) {
                    ZrObjectInit(state, array);
                    ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
                } else {
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TRY) {
                // TRY 指令：设置异常恢复点
                // 注意：VM级别的异常处理使用setjmp/longjmp机制
                // 这个指令主要用于标记try块的开始，实际异常处理由底层机制处理
                // operand2[0] = catch块跳转偏移（如果有异常）
                // 对于VM级别，异常通过setjmp/longjmp处理，这个指令主要是占位
                // 实际的异常恢复点由ZrExceptionTryRun等函数设置
                // 这里不做任何操作，异常处理由底层机制自动处理
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(THROW) {
                // THROW 指令：抛出异常
                // operand2[0] = 异常值槽位（可选，如果为-1则使用栈顶值）
                // 异常值应该在栈上（通常在destination或指定槽位）
                SZrTypeValue *errorValue = destination;
                // 将异常值复制到栈顶（如果不在栈顶）
                if (A2(instruction) != (TUInt16) -1) {
                    errorValue = &BASE(A2(instruction))->value;
                }
                // 确保异常值在栈顶
                if (errorValue != &(state->stackTop.valuePointer - 1)->value) {
                    ZrStackCopyValue(state, state->stackTop.valuePointer, errorValue);
                    state->stackTop.valuePointer++;
                }
                // 抛出异常（使用运行时错误状态）
                ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CATCH) {
                // CATCH 指令：捕获异常
                // operand2[0] = 异常值目标槽位
                // 检查是否有异常（通过threadStatus）
                if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                    // 有异常，将异常值复制到目标槽位
                    if (state->stackTop.valuePointer > callInfo->functionBase.valuePointer) {
                        SZrTypeValue *errorValue = &(state->stackTop.valuePointer - 1)->value;
                        ZrValueCopy(state, destination, errorValue);
                        // 清除异常状态
                        state->threadStatus = ZR_THREAD_STATUS_FINE;
                        state->stackTop.valuePointer--;
                    } else {
                        // 没有异常值，设置为null
                        ZrValueResetAsNull(destination);
                        state->threadStatus = ZR_THREAD_STATUS_FINE;
                    }
                } else {
                    // 没有异常，设置为null并继续执行
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_DEFAULT() {
                // todo: error unreachable
                char message[256];
                sprintf(message, "Not implemented op code:%d at offset %d\n", instruction.instruction.operationCode,
                        (int) (instructionsEnd - programCounter));
                ZrDebugRunError(state, message);
                ZR_ABORT();
            }
            DONE(1);
        }
    }

#undef DONE
}
