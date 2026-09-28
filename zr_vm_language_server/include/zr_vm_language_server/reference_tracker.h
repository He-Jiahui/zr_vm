//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H
#define ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/symbol_table.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"

/** @brief 区分导航查询中的读取、写入、定义与调用位置，供语义分析记录引用用途。 */
enum EZrReferenceType {
    ZR_REFERENCE_READ,      // 读引用
    ZR_REFERENCE_WRITE,     // 写引用
    ZR_REFERENCE_DEFINITION, // 定义引用
    ZR_REFERENCE_CALL,      // 函数调用引用
};

/** @brief 引用用途枚举的公开类型别名，供追踪器 API 传递。 */
typedef enum EZrReferenceType EZrReferenceType;

/**
 * @brief 单次语义引用的展示层记录；符号指针与位置中的 source 均非本结构所有。
 * @note symbolId 保存规范语义身份；语法恢复路径可能没有有效 ID，消费者须检查。
 */
typedef struct SZrReference {
    SZrSymbol *symbol;                // 引用的符号
    TZrSymbolId symbolId;             // canonical symbol identity; invalid for syntax-only recovery
    SZrFileRange location;            // 引用位置
    EZrReferenceType type;            // 引用类型
} SZrReference;

/** @brief 按分析器生命周期持有引用记录；按位置查询期间关联符号须继续存活。 */
typedef struct SZrReferenceTracker {
    SZrArray allReferences;           // 所有引用（SZrReference*）
} SZrReferenceTracker;

/**
 * @brief 为一个符号表创建引用集合，供语义分析记录定义和使用位置。
 * @pre state、state->global 与 symbolTable 非空。
 * @return 参数或追踪器本体分配失败时返回 ZR_NULL；返回的对象由调用方用 ReferenceTracker_Free 释放。
 * BUG: 引用数组缓冲分配失败时 Array_Init 仍标记有效，构造函数未经检查就返回追踪器；
 * 随后的 AddReference 可在 Array_Push 中断言失败或写入空缓冲，见 reference_tracker.c 的构造分支。
 * TODO: 当前实现只检查 symbolTable 非空，并未保存其身份；需核查该参数是否应约束
 * 后续 AddReference 的符号归属，或应从公开签名中移除，见 reference_tracker.c 的构造入口。
 */
ZR_LANGUAGE_SERVER_API SZrReferenceTracker *ZrLanguageServer_ReferenceTracker_New(SZrState *state, 
                                                                     SZrSymbolTable *symbolTable);

/** @brief 释放追踪器持有的记录；不释放记录所借用的符号和位置 source。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_ReferenceTracker_Free(SZrState *state, SZrReferenceTracker *tracker);

/**
 * @brief 把一次引用同时登记到追踪器与符号的引用视图，服务后续定位与计数。
 * @pre symbol 在追踪器存续期间有效；location 的 source 在查询期间有效。
 * @return 参数无效或记录本体分配失败时返回假。
 * @note 底层 Array_Push 不返回扩容结果，本接口不能报告后续数组扩容失败。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_ReferenceTracker_AddReference(SZrState *state, 
                                                              SZrReferenceTracker *tracker,
                                                              SZrSymbol *symbol,
                                                              SZrFileRange location,
                                                              EZrReferenceType type);

/**
 * @brief 在同源位置中选最窄的引用，供按位置查询识别光标所在符号。
 * @note 返回借用指针；同跨度时非定义引用优先，追踪器释放后不得继续使用。
 */
ZR_LANGUAGE_SERVER_API SZrReference *ZrLanguageServer_ReferenceTracker_FindReferenceAt(SZrReferenceTracker *tracker,
                                                                        SZrFileRange position);

#endif //ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H
