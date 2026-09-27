//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_HASH_H
#define ZR_VM_CORE_HASH_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/value.h"

struct SZrGlobalState;
struct SZrState;

/**
 * @brief 预留的原始对象哈希比较回调类型。
 * @note TODO: 当前仓库只有此声明，没有注册或调用点；需核对是否仍属于哈希表的公开契约，
 *       以及同类型比较应由此回调还是 ZrCore_Value_CompareDirectly 承担。
 */
typedef TZrBool (*FZrHashCompare)(struct SZrState *state, const SZrRawObject *object1, const SZrRawObject *object2);

/**
 * @brief 哈希表桶链节点；key 参与查找，value 保存关联值，next 连接同桶节点。
 * @note 节点由所属 HashSet 的独立分配或 pair pool 管理；GC 沿 key/value 扫描引用。
 *       持有节点指针的对象缓存须遵守所属 HashSet 的节点生命周期。
 */
struct ZR_STRUCT_ALIGN SZrHashKeyValuePair {
    SZrTypeValue key;
    struct SZrHashKeyValuePair *next;
    SZrTypeValue value;
};

typedef struct SZrHashKeyValuePair SZrHashKeyValuePair;

/**
 * @brief 为一个 GlobalState 生成运行期字符串哈希盐。
 * @pre global 指向正在初始化的全局状态；须在创建参与字符串表查找的字符串前写入 hashSeed。
 * @note 混合时间、地址和 uniqueNumber，不保证唯一性或密码学强度，不能写入持久化身份。
 */
ZR_CORE_API TZrUInt64 ZrCore_HashSeed_Create(struct SZrGlobalState *global, TZrUInt64 uniqueNumber);

/**
 * @brief 按所属 GlobalState 的盐计算字符串对象和驻留表使用的运行期哈希。
 * @pre global 非空且 hashSeed 已初始化；length 非零时 string 指向至少 length 字节。
 * @note 结果用于同一个全局状态内的桶定位和字符串相等性快速拒绝，
 *       不用于跨进程或跨全局状态的签名及元数据身份。
 */
ZR_CORE_API TZrUInt64 ZrCore_Hash_Create(struct SZrGlobalState *global, TZrNativeString string, TZrSize length);

/**
 * @brief 对传入字节序列计算无盐、可重复的 64 位身份哈希。
 * @pre length 非零时 data 指向至少 length 字节。
 * @note 编译器、AOT 和运行时契约共用此算法；跨平台身份须由调用方规范化字节序列。
 *       它不提供防碰撞或真实性证明。
 */
ZR_CORE_API TZrUInt64 ZrCore_Hash_CreateStable64(const TZrByte *data, TZrSize length);

/**
 * @brief 对前缀与数据的串接字节计算稳定哈希，供版本化签名隔离不同契约域。
 * @pre 非零长度的 prefix/data 分别指向足够长的有效字节区间；零长度片段可为空指针。
 * @return 哈希值；内部状态分配或更新失败时也返回 0，调用方不能据此区分有效的零哈希。
 * @note 每次调用会创建并释放 XXH3 状态；两个片段直接串接，不自动编码边界。
 *       可变前缀的边界须由调用方约定；结果不是密码学签名。
 */
ZR_CORE_API TZrUInt64 ZrCore_Hash_CreateStable64WithPrefix(const TZrByte *prefix,
                                                           TZrSize prefixLength,
                                                           const TZrByte *data,
                                                           TZrSize length);

#endif // ZR_VM_CORE_HASH_H
