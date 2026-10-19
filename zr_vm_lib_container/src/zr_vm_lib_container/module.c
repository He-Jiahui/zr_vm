/** @file
 * @brief zr.container 的静态反射契约与原生容器回调。
 * @details Array/Map/Set 使用隐藏原生数组，LinkedList 使用 GC 节点；迭代器引用源而非快照。
 * 注册表按下方 method/meta 表创建闭包，直接下标派发另消费 Map 的两条内联回调。
 * 此文件不提供并发容器协议；process 静态缓存按 VM 身份重置，调用者仍须遵守域和派发约束。
 */
//
// Built-in zr.container module and runtime callbacks.
//

#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_iteration/module.h"
#include "contiguous_view.h"
#include "pooling.h"

#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

/* Array/Map/Set 的公开长度字段与隐藏 backing 分离；迭代器则通过隐藏 source
 * 维持 backing 的可达性。字段名也是热槽选择键，修改时须同步下方缓存映射。
 */
/**
 * ## Summary
 * Array包装对象与原生backing的私有连接名。
 *
 * ## Value model
 * __zr_items；与entries共用cachedHiddenItemsPair角色，使用仍须核验实际键；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:858;zr_vm_lib_container/src/zr_vm_lib_container/module.c:859;zr_vm_lib_container/src/zr_vm_lib_container/module.c:895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:896;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1509;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2242;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:44;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerItemsField = "__zr_items";
/**
 * ## Summary
 * Map/Set包装对象与entries的私有连接名。
 *
 * ## Value model
 * __zr_entries；与items同热角色不说明共享backing；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:861;zr_vm_lib_container/src/zr_vm_lib_container/module.c:862;zr_vm_lib_container/src/zr_vm_lib_container/module.c:898;zr_vm_lib_container/src/zr_vm_lib_container/module.c:899;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2036;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2242;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2275;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6770;zr_vm_lib_container/src/zr_vm_lib_container/module.c:58;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerEntriesField = "__zr_entries";
/**
 * ## Summary
 * Enumerator来源保活边的私有字段名。
 *
 * ## Value model
 * __zr_source；实例指向backing/list，缓存名不是该GC引用；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1584;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2256;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2288;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5330;zr_vm_lib_container/src/zr_vm_lib_container/module.c:72;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerSourceField = "__zr_source";
/**
 * ## Summary
 * Array Enumerator下一位置的私有字段名。
 *
 * ## Value model
 * __zr_index；整数游标不是长度/memberVersion快照；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1634;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2262;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:86;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerIndexField = "__zr_index";
/**
 * ## Summary
 * LinkedList Enumerator下一节点的私有字段名。
 *
 * ## Value model
 * __zr_nextNode；实例字段使节点可达，结束可为空；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1660;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2265;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2297;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5370;zr_vm_lib_container/src/zr_vm_lib_container/module.c:100;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerNextNodeField = "__zr_nextNode";
/**
 * ## Summary
 * Map/Set/LinkedList公开元素计数名。
 *
 * ## Value model
 * count；构造/增删/清空单独维护，不能当作backing实际长度；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:864;zr_vm_lib_container/src/zr_vm_lib_container/module.c:865;zr_vm_lib_container/src/zr_vm_lib_container/module.c:901;zr_vm_lib_container/src/zr_vm_lib_container/module.c:902;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2246;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2279;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6301;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6374;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6413;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6660;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6771;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6813;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6888;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6927;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7053;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7054;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7079;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7135;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7137;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7188;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7190;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7355;zr_vm_lib_container/src/zr_vm_lib_container/module.c:114;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerCountField = "count";
/**
 * ## Summary
 * Array公开逻辑长度名。
 *
 * ## Value model
 * length；与count/first共用cachedLengthPair角色，不等于宿主容量；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:867;zr_vm_lib_container/src/zr_vm_lib_container/module.c:868;zr_vm_lib_container/src/zr_vm_lib_container/module.c:904;zr_vm_lib_container/src/zr_vm_lib_container/module.c:905;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2245;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2278;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5757;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5924;zr_vm_lib_container/src/zr_vm_lib_container/module.c:128;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerLengthField = "length";
/**
 * ## Summary
 * Array公开增长策略容量名。
 *
 * ## Value model
 * capacity；与last/second共用cachedCapacityPair角色，不等于物理分配量；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:870;zr_vm_lib_container/src/zr_vm_lib_container/module.c:871;zr_vm_lib_container/src/zr_vm_lib_container/module.c:907;zr_vm_lib_container/src/zr_vm_lib_container/module.c:908;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2251;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2283;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5118;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5121;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5134;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:142;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerCapacityField = "capacity";
/**
 * ## Summary
 * Enumerator当前值协议名。
 *
 * ## Value model
 * current；对应prototype currentMemberName，键缓存不保活当前值；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1609;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2291;zr_vm_lib_container/src/zr_vm_lib_container/module.c:156;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerCurrentField = "current";
/**
 * ## Summary
 * LinkedNode元素载荷名。
 *
 * ## Value model
 * value；摘链后仍可保留，由节点/VM管理，没有专用字段Pair热槽；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:873;zr_vm_lib_container/src/zr_vm_lib_container/module.c:874;zr_vm_lib_container/src/zr_vm_lib_container/module.c:910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:911;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4749;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4753;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5511;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6995;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7000;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7228;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7305;zr_vm_lib_container/src/zr_vm_lib_container/module.c:170;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerValueField = "value";
/**
 * ## Summary
 * LinkedNode后继链接名。
 *
 * ## Value model
 * next；连接/遍历/摘链消费，没有专用字段Pair热槽；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:876;zr_vm_lib_container/src/zr_vm_lib_container/module.c:877;zr_vm_lib_container/src/zr_vm_lib_container/module.c:913;zr_vm_lib_container/src/zr_vm_lib_container/module.c:914;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2269;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4757;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5523;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7004;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7039;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7041;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7123;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7180;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7311;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7345;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7346;zr_vm_lib_container/src/zr_vm_lib_container/module.c:184;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerNextField = "next";
/**
 * ## Summary
 * LinkedNode前驱链接名。
 *
 * ## Value model
 * previous；双向连接及摘链消费，没有专用字段Pair热槽；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:879;zr_vm_lib_container/src/zr_vm_lib_container/module.c:880;zr_vm_lib_container/src/zr_vm_lib_container/module.c:916;zr_vm_lib_container/src/zr_vm_lib_container/module.c:917;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7038;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7046;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7127;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7176;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7347;zr_vm_lib_container/src/zr_vm_lib_container/module.c:198;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerPreviousField = "previous";
/**
 * ## Summary
 * LinkedList首节点名。
 *
 * ## Value model
 * first；与PairFirst等文本并共用键字符串，接收对象决定不同实例字段语义；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:885;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2247;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7080;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7122;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7136;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7184;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7303;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7343;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7353;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7391;zr_vm_lib_container/src/zr_vm_lib_container/module.c:212;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerFirstField = "first";
/**
 * ## Summary
 * LinkedList尾节点名。
 *
 * ## Value model
 * last；与capacity/second仅共用cachedCapacityPair角色，不表示容量语义；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:883;zr_vm_lib_container/src/zr_vm_lib_container/module.c:919;zr_vm_lib_container/src/zr_vm_lib_container/module.c:920;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2252;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2284;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7048;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7131;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7175;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7189;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7262;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7354;zr_vm_lib_container/src/zr_vm_lib_container/module.c:226;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerLastField = "last";
/**
 * ## Summary
 * Pair首分量及Map entry键分量名。
 *
 * ## Value model
 * first；与LinkedList首节点同文本，不据此认作同一实例字段或值；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:888;zr_vm_lib_container/src/zr_vm_lib_container/module.c:889;zr_vm_lib_container/src/zr_vm_lib_container/module.c:922;zr_vm_lib_container/src/zr_vm_lib_container/module.c:923;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1534;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2248;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2280;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3193;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4706;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5567;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5576;zr_vm_lib_container/src/zr_vm_lib_container/module.c:240;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerPairFirstField = "first";
/**
 * ## Summary
 * Pair次分量及Map entry值分量名。
 *
 * ## Value model
 * second；与capacity/last仅共用cachedCapacityPair角色，键匹配仍是前提；静态NUL字面量借用。
 *
 * ## Constraints and lifecycle
 * 名称不拥有对象、字段值或backing；指针快分支之外有文本匹配，C不保证等字面量地址合并或分离。更名须同步键映射、热槽分类及实际构造/访问链。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:891;zr_vm_lib_container/src/zr_vm_lib_container/module.c:892;zr_vm_lib_container/src/zr_vm_lib_container/module.c:925;zr_vm_lib_container/src/zr_vm_lib_container/module.c:926;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2253;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2285;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3225;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3280;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4715;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4719;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5577;zr_vm_lib_container/src/zr_vm_lib_container/module.c:254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static const TZrChar *kContainerPairSecondField = "second";

/**
 * ## Summary
 * 限定名字fallback及Map成功候选的宿主静态存储规模，供两类性能缓存协作。
 *
 * ## Value model
 * 匿名本地编译期常量组；不是容器元素数、Map桶数或guest协议容量。
 *
 * ## Constraints and lifecycle
 * 48表、4候选和独立MRU服务不同缓存；改变值须同步所有实际读写、显式四槽查询及替换游标宽度。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:645;zr_vm_lib_container/src/zr_vm_lib_container/module.c:929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1279;zr_vm_lib_container/src/zr_vm_lib_container/module.c:269;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1380;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1387
 */
enum {
    /**
     * ## Summary
     * 为固定名字以外的字段名保留有界fallback表。
     *
     * ## Value model
     * 48个ZrContainerFieldStringCacheEntry；固定11名字使用专槽，source/current/index/nextNode热键另走accessor。
     *
     * ## Constraints and lifecycle
     * 同一identity内无淘汰，表满走断言后NULL；借用的宿主fieldName指针及NUL文本内容须保持有效且不变至换域清表。TODO: 新动态名入口须沿make_field_key和fallback消费核对容量与文本有效期，不据固定字面量调用承诺任意名字数。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:645;zr_vm_lib_container/src/zr_vm_lib_container/module.c:929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:935;zr_vm_lib_container/src/zr_vm_lib_container/module.c:283;zr_vm_lib_container/src/zr_vm_lib_container/module.c:930;zr_vm_lib_container/src/zr_vm_lib_container/module.c:942;zr_vm_lib_container/src/zr_vm_lib_container/module.c:948;zr_vm_lib_container/src/zr_vm_lib_container/module.c:953
     */
    ZR_CONTAINER_FIELD_CACHE_CAPACITY = 48,
    /**
     * ## Summary
     * 为反复访问的字符串键保留四个成功查找候选，与独立MRU配合减少真实扫描。
     *
     * ## Value model
     * 4个候选槽；entries/backing变化清空，nextSlotIndex是TZrUInt8替换游标。
     *
     * ## Constraints and lifecycle
     * 不是通用可调循环上限：zr_container_try_hot_map_lookup_cache显式访问slots[0..3]，改值须同时改查询、数组、取模及游标表示；正结果缓存不记录缺键。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1279;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1287;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1288;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1309;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1310;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1313;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1441;zr_vm_lib_container/src/zr_vm_lib_container/module.c:297;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1380;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1387;zr_vm_lib_container/src/zr_vm_lib_container/module.c:425
     */
    ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT = 4
};

/**
 * ## Summary
 * 保存固定热名之外的一个宿主字段名与本 VM 的永久字符串，避免重复字段访问时重建键对象。
 * 实际使用者为 make_field_key、set_int/set_null、try_get 与字段 Pair 查找，不拥有容器实例。
 *
 * ## Fields
 * fieldName 借用 NUL 结尾的宿主文本，比较仍会读取内容；fieldString 借用创建后永久标记的本域字符串。
 * 两字段不作为一份可跨 VM 的键；宿主文本未复制，永久标记也不赋予宿主释放字符串的权利。
 *
 * ## Invariants and lifecycle
 * fallback 静态表最多48项，固定热名走独立槽。cacheIdentity 改变时全表清零，不析构原域字符串。
 * fieldName 必须保持有效到相应缓存清空；字符串只在当前 GlobalState 存活期间有效。
 * PERMANENT 区域及持久根保证已标记字符串不被 moving GC 搬迁；这不适用于下面 Map 的普通 Pair 借用。
 * TODO: 多 GlobalState 并行访问的进程静态缓存同步仍待核对，具体入口是本文件的缓存 accessor 与双域并行调用。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:645 是静态表；zr_vm_lib_container/src/zr_vm_lib_container/module.c:842、zr_vm_lib_container/src/zr_vm_lib_container/module.c:844 清空，zr_vm_lib_container/src/zr_vm_lib_container/module.c:930、zr_vm_lib_container/src/zr_vm_lib_container/module.c:931 匹配读出，zr_vm_lib_container/src/zr_vm_lib_container/module.c:948、zr_vm_lib_container/src/zr_vm_lib_container/module.c:949 发布。
 * 实际字段消费者 zr_vm_lib_container/src/zr_vm_lib_container/module.c:981、zr_vm_lib_container/src/zr_vm_lib_container/module.c:2082、zr_vm_lib_container/src/zr_vm_lib_container/module.c:2138、zr_vm_lib_container/src/zr_vm_lib_container/module.c:2439、zr_vm_lib_container/src/zr_vm_lib_container/module.c:3052；创建/永久标记 zr_vm_lib_container/src/zr_vm_lib_container/module.c:942、zr_vm_lib_container/src/zr_vm_lib_container/module.c:946。
 * zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210 与 zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217 是永久根/不可移动区域的依据。
 */
typedef struct ZrContainerFieldStringCacheEntry {
    /** @brief fieldName 借用 NUL 结尾宿主文本，缓存清空前必须有效；命中仍读取其内容，不复制或释放。 */
    const TZrChar *fieldName;
    /** @brief 本域永久字段字符串借用，永久根保活且不搬迁；identity 切换丢弃指针，不接管域生命周期。 */
    SZrString *fieldString;
} ZrContainerFieldStringCacheEntry;

/**
 * ## Summary
 * 给当前 VM 的八个热字段角色保存永久字符串，供 backing、Pair 和 Enumerator 的字段热路径复用。
 * 它只缓存字段名，不缓存该字段指向的容器、条目或迭代位置。
 *
 * ## Fields
 * cacheIdentity 区分 GlobalState 生命周期；items/entries 对应隐藏 backing 键，pairFirst/pairSecond 对应 Pair 分量键。
 * iteratorSource/Current/Index/NextNode 分别用于来源、当前值与两种迭代游标，均是延迟创建的字符串借用。
 *
 * ## Invariants and lifecycle
 * accessor 的进程静态实例按当前 global->cacheIdentity 清零；槽第一次使用时创建字符串并 MarkAsPermanent。
 * 已创建字符串在本域存续期间由永久根保活且不搬迁；缓存不形成容器实例或 backing 的 GC 根。
 * 切换 identity 只丢弃槽值，不释放原域对象；各字符串不能跨所属 GlobalState 销毁继续使用。
 * TODO: identity 切换不提供锁；多个 VM/mutator 的并发窗口需沿各字段 accessor 与主域调用契约核对。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1024 定义唯一静态实例；zr_vm_lib_container/src/zr_vm_lib_container/module.c:1030、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1032 比较/写入本类型身份。
 * 八字段 accessor 为 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1481、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1506、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1531、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1556、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1581、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1606、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1631、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1656；通用创建/永久标记为 zr_vm_lib_container/src/zr_vm_lib_container/module.c:582、zr_vm_lib_container/src/zr_vm_lib_container/module.c:586。
 * 真实消费者包括 backing zr_vm_lib_container/src/zr_vm_lib_container/module.c:4558、zr_vm_lib_container/src/zr_vm_lib_container/module.c:4657，Pair zr_vm_lib_container/src/zr_vm_lib_container/module.c:3181、zr_vm_lib_container/src/zr_vm_lib_container/module.c:3213，Enumerator zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218、zr_vm_lib_container/src/zr_vm_lib_container/module.c:5432。
 * zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210 与 zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217 说明永久字符串生命周期。
 */
typedef struct ZrContainerHotFieldStringCache {
    /** @brief 当前 GlobalState 生命周期标量，只由本类型 accessor 比较/写入；不是锁、对象版本或 GC 根。 */
    TZrUInt64 cacheIdentity;
    /** @brief items 隐藏 backing 的永久字符串键；仅缓存键名，不保活对应 Array。 */
    SZrString *itemsFieldString;
    /** @brief entries 隐藏 backing 的永久字符串键；仅缓存键名，不拥有 Map/Set 条目。 */
    SZrString *entriesFieldString;
    /** @brief Pair.first 的永久字符串键；字段值仍由 Pair/VM 管理。 */
    SZrString *pairFirstFieldString;
    /** @brief Pair.second 的永久字符串键；不复制或保活对应分量值。 */
    SZrString *pairSecondFieldString;
    /** @brief Enumerator.source 的永久键名；实例中的字段值负责保持来源可达。 */
    SZrString *iteratorSourceFieldString;
    /** @brief Enumerator.current 的永久键名；缓存不拥有迭代当前值。 */
    SZrString *iteratorCurrentFieldString;
    /** @brief Array Enumerator 隐藏 index 的永久键名；不是游标数值或版本快照。 */
    SZrString *iteratorIndexFieldString;
    /** @brief LinkedList Enumerator nextNode 的永久键名；节点由实例字段保持可达。 */
    SZrString *iteratorNextNodeFieldString;
} ZrContainerHotFieldStringCache;

/**
 * ## Summary
 * 保存一次成功字符串键查找的位置，供同一 Map backing 的 MRU 和四候选槽复用。
 * 同一键对象身份、同一 entries.memberVersion 且 Pair 指针非空时才能交付缓存位置；负查找不写槽。
 *
 * ## Fields
 * keyObject 是查询字符串对象的裸借用身份，不是内容哈希；entryObject 是对应 Pair 的裸借用。
 * entriesMemberVersion 及 index 是成功扫描时的普通版本/下标；没有独立的所有权、边界或内容证明。
 *
 * ## Invariants and lifecycle
 * 父缓存负责 entries/backing 与域切换失效；槽本身不保活 key、Pair 或 backing，不能跨分配/用户回调持有裸地址。
 * 现有验证不重新比较 Pair.first 的当前内容，也不单独重查 index 边界。
 * TODO: Pair.first 的合法写入与 entries 版本联动需沿 zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300 的写路径核对，不能把版本相同当成键未变的证明。
 * TODO: moving GC 后的 raw key/entry 重写仍待沿 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1685 和 gc_cycle 对象重写入口核对。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1214、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1441 是槽读取验证；zr_vm_lib_container/src/zr_vm_lib_container/module.c:1274 至 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1313 是已有候选覆盖及新槽发布/推进。
 * Map 真实扫描与缓存接入 zr_vm_lib_container/src/zr_vm_lib_container/module.c:4784、zr_vm_lib_container/src/zr_vm_lib_container/module.c:4927，公开消费者 zr_vm_lib_container/src/zr_vm_lib_container/module.c:6334、zr_vm_lib_container/src/zr_vm_lib_container/module.c:6366、zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495、zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575、zr_vm_lib_container/src/zr_vm_lib_container/module.c:6626、zr_vm_lib_container/src/zr_vm_lib_container/module.c:6736。
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1259 拒绝 NULL entry，zr_vm_lib_container/src/zr_vm_lib_container/module.c:4879 的负查找不发布候选；父容量为 zr_vm_lib_container/src/zr_vm_lib_container/module.c:298 的四槽常量。
 */
typedef struct ZrContainerHotMapLookupCacheSlot {
    /** @brief 查询字符串的裸对象身份借用；按地址匹配，不作为内容哈希或 GC 根。 */
    SZrRawObject *keyObject;
    /** @brief 成功扫描时的 backing memberVersion；须与当前版本相等，不独立证明 Pair.first 内容未变。 */
    TZrUInt32 entriesMemberVersion;
    /** @brief 成功位置下标；仅在 key/version/Pair 同时有效时使用，不独立做长度或所有权证明。 */
    TZrSize index;
    /** @brief 成功位置的 Pair 裸借用；缓存不保活，不跨分配/回调/moving GC 保留旧地址。 */
    SZrObject *entryObject;
} ZrContainerHotMapLookupCacheSlot;

/* Map 字符串键热路径只缓存 entries 中已有 Pair 的位置；同一 entries 的成员版本
 * 变化后，旧位置与 Pair 指针都不能再用于查找。
 */
/**
 * ## Summary
 * 进程静态字符串键 Map 热状态，将最近一次命中与四个候选槽用于减少重复扫描。
 * 所有位置都借用当前 backing；同一域、entries 身份、键对象身份及成员版本共同决定可否复用。
 *
 * ## Fields
 * cacheIdentity 是域生命周期标量；entries 是当前 backing 借用；lastKey/lastEntry 是 MRU 的对象借用。
 * lastEntriesMemberVersion 和 lastIndex 与 MRU 同时有效；slots 内嵌四个同契约候选，nextSlotIndex 只控制替换次序。
 *
 * ## Invariants and lifecycle
 * identity 改变时清零整份缓存；entries 改变时丢弃 MRU、候选和游标，不析构容器元素。
 * 已有同 key/version 槽被覆盖后直接返回，不推进游标；插入空槽或轮换满槽后均推进游标。
 * entries/key/Pair 裸指针没有永久标记或独立 GC 根；不能据此延长它们的生命周期或跨分配/回调保留。
 * TODO: moving GC 裸指针重写、Pair.first 与 backing 版本联动及多域并发同步分别沿 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1685、zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300、zr_vm_lib_container/src/zr_vm_lib_container/module.c:591 核对。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1075 是本类型静态实例；zr_vm_lib_container/src/zr_vm_lib_container/module.c:1081、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1083 是本类型 identity 访问。
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1106 至 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1119 切换 backing，zr_vm_lib_container/src/zr_vm_lib_container/module.c:1138、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1249 发布 MRU/候选，zr_vm_lib_container/src/zr_vm_lib_container/module.c:1280 至 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1313 覆盖/插入/推进游标。
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1334、zr_vm_lib_container/src/zr_vm_lib_container/module.c:1412 是两类查找；zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807、zr_vm_lib_container/src/zr_vm_lib_container/module.c:4943 是 Map 真实接入点，zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495、zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575 是公开读写消费者。
 */
typedef struct ZrContainerHotMapLookupCache {
    /** @brief 本类型静态缓存的 GlobalState 身份标量；只由 Map cache accessor 比较/写入，变化清零。 */
    TZrUInt64 cacheIdentity;
    /** @brief 当前 entries backing 裸借用；切换 backing 清除 MRU/全部候选，不保活 Array。 */
    SZrObject *entries;
    /** @brief 最近成功字符串键的裸身份借用；不缓存负查找，也不比较字符串内容。 */
    SZrRawObject *lastKeyObject;
    /** @brief 最近命中 backing 版本标量，与 lastKey/lastEntry 同时有效；不授 Pair 所有权。 */
    TZrUInt32 lastEntriesMemberVersion;
    /** @brief 最近成功位置的普通下标，与 MRU 其它字段同时有效，不独立证明 Pair 键/边界。 */
    TZrSize lastIndex;
    /** @brief 最近成功 Pair 裸借用；非 GC 根，不跨失效操作持有。 */
    SZrObject *lastEntryObject;
    /** @brief 新槽插入后推进的四槽替换游标；已有同 key/version 槽覆盖不推进。 */
    TZrUInt8 nextSlotIndex;
    /** @brief 四个内嵌候选槽；不额外拥有 slot 中 key/Pair，与 MRU 共用 backing/版本失效约定。 */
    ZrContainerHotMapLookupCacheSlot slots[ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT];
} ZrContainerHotMapLookupCache;

#if defined(ZR_DEBUG)
/**
 * ## Summary
 * DEBUG过程诊断窗口，让测试观察缓存成功命中提交。
 *
 * ## Value model
 * 进程静态整数结构；hotHitCount/memberVersionHitCount同点递增；entryValidationReadCount当前没有递增或非零更新，初始化和Reset将其置零。
 *
 * ## Constraints and lifecycle
 * Reset清数字，Get按值读取；无VM身份/容器引用，不可由零计数证明功能、GC或内容核验。
 * TODO: 清零、写入及读取没有同步，沿测试窗口与多域执行入口核对隔离及数据竞争。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:544;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1154;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1155;zr_vm_lib_container/src/zr_vm_lib_container/module.c:457;tests/container/test_container_runtime.c:1535;tests/container/test_container_runtime.c:1539;zr_vm_lib_container/include/zr_vm_lib_container/module.h:45
 */
static ZrVmLibContainerDebugHotMapLookupStats gZrContainerDebugHotMapLookupStats = {0};
#endif

static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state);
static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state);
static ZR_FORCE_INLINE TZrBool zr_container_array_raw_int_active(const SZrObject *array);
static ZR_FORCE_INLINE SZrObject *zr_container_array_get_object_fast(SZrState *state,
                                                                     SZrObject *array,
                                                                     TZrSize index);
static ZR_FORCE_INLINE SZrHashKeyValuePair **zr_container_hot_field_pair_slot(SZrObject *object,
                                                                               const TZrChar *fieldName);
static ZR_FORCE_INLINE TZrBool zr_container_try_set_existing_pair_value_plain_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   SZrHashKeyValuePair *pair,
                                                                                   const SZrTypeValue *value);
static ZR_FORCE_INLINE void zr_container_refresh_cached_field_slot(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldString,
                                                                   SZrHashKeyValuePair **hotPairSlot);
static ZR_FORCE_INLINE TZrBool zr_container_pair_matches_field_string(SZrState *state,
                                                                       SZrHashKeyValuePair *pair,
                                                                       SZrString *fieldString);
static ZR_FORCE_INLINE void zr_container_cache_string_lookup_pair_mru(SZrObject *object,
                                                                       SZrHashKeyValuePair *pair);

/**
 * ## Summary
 * 将固定名字和等文本宿主名字归为同一字段角色，供键缓存、隐藏字段识别及对象热槽选择。
 *
 * ## Parameters
 * 两个参数借用NUL文本，任一可为NULL；非NULL文本在调用期间有效。
 *
 * ## Returns
 * 相同指针（含两个NULL）或两个非NULL等文本为true，其余false。
 *
 * ## Constraints and lifecycle
 * 不访问VM、不复制/释放文本；指针不同会读取内容，不能传悬空或未终止文本。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:898;zr_vm_lib_container/src/zr_vm_lib_container/module.c:901;zr_vm_lib_container/src/zr_vm_lib_container/module.c:904;zr_vm_lib_container/src/zr_vm_lib_container/module.c:907;zr_vm_lib_container/src/zr_vm_lib_container/module.c:910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:913;zr_vm_lib_container/src/zr_vm_lib_container/module.c:916;zr_vm_lib_container/src/zr_vm_lib_container/module.c:919;zr_vm_lib_container/src/zr_vm_lib_container/module.c:922;zr_vm_lib_container/src/zr_vm_lib_container/module.c:925;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2036;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2275;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2278;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2279;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2280;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2283;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2284;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2285;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2288;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2291;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2297;zr_vm_lib_container/src/zr_vm_lib_container/module.c:498;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236
 */
static ZR_FORCE_INLINE TZrBool zr_container_field_name_equals(const TZrChar *fieldName, const TZrChar *expectedFieldName) {
    return fieldName == expectedFieldName ||
           (fieldName != ZR_NULL && expectedFieldName != ZR_NULL && strcmp(fieldName, expectedFieldName) == 0);
}

/**
 * ## Summary
 * 为脚本测试开始新的进程诊断计数窗口；不清真实Map缓存或修改容器。
 *
 * ## Parameters
 * 无；窗口不属于某个state或Map。
 *
 * ## Returns
 * void；DEBUG清计数，非DEBUG无动作，无失败返回。
 *
 * ## Constraints and lifecycle
 * TODO: Reset、Get和命中递增没有同步；沿测试窗口与多域执行入口核对隔离，不能当成暂停其它线程。
 *
 * ## References
 * tests/container/test_container_runtime.c:1535;zr_vm_lib_container/src/zr_vm_lib_container/module.c:519;zr_vm_lib_container/src/zr_vm_lib_container/module.c:457;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1154;zr_vm_lib_container/include/zr_vm_lib_container/module.h:146
 */
void ZrVmLibContainer_Debug_ResetHotMapLookupStats(void) {
#if defined(ZR_DEBUG)
    memset(&gZrContainerDebugHotMapLookupStats, 0, sizeof(gZrContainerDebugHotMapLookupStats));
#endif
}

/**
 * ## Summary
 * 在待测脚本后取诊断数字，供测试比较成功缓存命中的使用情况。
 *
 * ## Parameters
 * 无；不接收/保活VM。
 *
 * ## Returns
 * 结构副本；DEBUG读取计数，非DEBUG全零。
 *
 * ## Constraints and lifecycle
 * 两命中计数在同一提交点同时递增；entryValidationReadCount没有递增，零值不能证明执行过字段内容核验。真实结果仍须功能断言。
 * TODO: Get、Reset及写没有同步，沿实际并行入口核对快照与窗口隔离。
 *
 * ## References
 * tests/container/test_container_runtime.c:1539;zr_vm_lib_container/src/zr_vm_lib_container/module.c:542;zr_vm_lib_container/src/zr_vm_lib_container/module.c:457;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1154;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1155;zr_vm_lib_container/include/zr_vm_lib_container/module.h:163
 */
ZrVmLibContainerDebugHotMapLookupStats ZrVmLibContainer_Debug_GetHotMapLookupStats(void) {
#if defined(ZR_DEBUG)
    return gZrContainerDebugHotMapLookupStats;
#else
    ZrVmLibContainerDebugHotMapLookupStats stats = {0};
    return stats;
#endif
}

/**
 * ## Summary
 * 为字段访问延迟创建键字符串，永久标记完成后才发布到当前域缓存槽。
 *
 * ## Parameters
 * state为已构造当前域state；fieldName为有效NUL文本；slot为当前域零初始化或已有效槽。
 *
 * ## Returns
 * 本域永久字符串借用；state/name/slot为空或Create返回NULL时为NULL且不填空槽；已有槽直接借用。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 本函数不校验state->global或已有槽所属域，身份切换由上层负责；NULL分支不承诺所有底层OOM可恢复。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:859;zr_vm_lib_container/src/zr_vm_lib_container/module.c:862;zr_vm_lib_container/src/zr_vm_lib_container/module.c:865;zr_vm_lib_container/src/zr_vm_lib_container/module.c:868;zr_vm_lib_container/src/zr_vm_lib_container/module.c:871;zr_vm_lib_container/src/zr_vm_lib_container/module.c:874;zr_vm_lib_container/src/zr_vm_lib_container/module.c:877;zr_vm_lib_container/src/zr_vm_lib_container/module.c:880;zr_vm_lib_container/src/zr_vm_lib_container/module.c:883;zr_vm_lib_container/src/zr_vm_lib_container/module.c:886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:889;zr_vm_lib_container/src/zr_vm_lib_container/module.c:892;zr_vm_lib_container/src/zr_vm_lib_container/module.c:896;zr_vm_lib_container/src/zr_vm_lib_container/module.c:899;zr_vm_lib_container/src/zr_vm_lib_container/module.c:902;zr_vm_lib_container/src/zr_vm_lib_container/module.c:905;zr_vm_lib_container/src/zr_vm_lib_container/module.c:908;zr_vm_lib_container/src/zr_vm_lib_container/module.c:911;zr_vm_lib_container/src/zr_vm_lib_container/module.c:914;zr_vm_lib_container/src/zr_vm_lib_container/module.c:917;zr_vm_lib_container/src/zr_vm_lib_container/module.c:920;zr_vm_lib_container/src/zr_vm_lib_container/module.c:923;zr_vm_lib_container/src/zr_vm_lib_container/module.c:926;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1509;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1534;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1584;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1609;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1634;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1659;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static SZrString *zr_container_cache_field_string_once(SZrState *state,
                                                       const TZrChar *fieldName,
                                                       SZrString **slot) {
    SZrString *fieldString;

    if (state == ZR_NULL || fieldName == ZR_NULL || slot == ZR_NULL) {
        return ZR_NULL;
    }

    if (*slot != ZR_NULL) {
        return *slot;
    }

    fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    *slot = fieldString;
    return fieldString;
}

/* TODO: 核实多个 GlobalState 并行使用容器方法的支持范围。字段字符串、
 * Map 查找和迭代器原型均有进程级可写缓存，仅按 cacheIdentity 顺序失效；
 * 需并行双 GlobalState 测试确认隔离与同步契约。
 */
/**
 * ## Summary
 * 把普通字段读写需要的宿主名统一为永久String键；本函数识别的固定名用专槽，其余文本进入48项fallback；source/current/index/nextNode高热访问另走专用accessor。
 *
 * ## Parameters
 * state/global有效；fieldName为NUL文本，fallback借用宿主文本指针，须活到缓存清空。
 *
 * ## Returns
 * 成功返回本域永久键；无效state/global/name或Create返回NULL时为NULL；48个fallback槽全满断言后NULL。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * first两别名共用键字符串，不说明LinkedList和Pair共享实例字段。identity切换丢弃缓存值，不析构原域对象。
 * TODO: 新增动态名字调用时沿zr_vm_lib_container/src/zr_vm_lib_container/module.c:981;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2082;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2138;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3052核对宿主文本有效期和48槽容量；固定字面量调用不承诺任意名字数量。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:981;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2082;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2138;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:842;zr_vm_lib_container/src/zr_vm_lib_container/module.c:930;zr_vm_lib_container/src/zr_vm_lib_container/module.c:942;zr_vm_lib_container/src/zr_vm_lib_container/module.c:946;zr_vm_lib_container/src/zr_vm_lib_container/module.c:953;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static SZrString *zr_container_cached_field_string(SZrState *state, const TZrChar *fieldName) {
    /**
     * ## Summary
     * 标记普通名字缓存当前所属GlobalState生命周期，阻止顺序换域时复用旧域String。
     *
     * ## Value model
     * 本函数静态TZrUInt64，初值0；global创建分配非零身份，比较及更新只在本函数域切换块。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:842;zr_vm_lib_container/src/zr_vm_lib_container/module.c:843;zr_vm_lib_container/src/zr_vm_lib_container/module.c:629;zr_vm_core/src/zr_vm_core/runtime/global.c:36;zr_vm_core/src/zr_vm_core/runtime/global.c:376
     */
    static TZrUInt64 cachedGlobalCacheIdentity = 0;
    /**
     * ## Summary
     * 容纳没有命中固定11名的宿主文本到永久String映射，供普通Object字段Get/Set复用。
     *
     * ## Value model
     * 本函数独立48项fallback；fieldName是借用宿主NUL文本，fieldString是本域永久键；无中途淘汰。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * TODO: 新动态名字必须保证宿主指针及NUL文本内容到该槽换域清表前有效且不变；建String不会复制或拥有保存的fieldName指针。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:844;zr_vm_lib_container/src/zr_vm_lib_container/module.c:930;zr_vm_lib_container/src/zr_vm_lib_container/module.c:931;zr_vm_lib_container/src/zr_vm_lib_container/module.c:938;zr_vm_lib_container/src/zr_vm_lib_container/module.c:948;zr_vm_lib_container/src/zr_vm_lib_container/module.c:949;zr_vm_lib_container/src/zr_vm_lib_container/module.c:645;zr_vm_lib_container/src/zr_vm_lib_container/module.c:981;zr_vm_lib_container/src/zr_vm_lib_container/module.c:942;zr_vm_lib_container/src/zr_vm_lib_container/module.c:946;zr_vm_lib_container/src/zr_vm_lib_container/module.c:953
     */
    static ZrContainerFieldStringCacheEntry cache[ZR_CONTAINER_FIELD_CACHE_CAPACITY];
    /**
     * ## Summary
     * 跨本函数调用复用Array私有items backing键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:859;zr_vm_lib_container/src/zr_vm_lib_container/module.c:896;zr_vm_lib_container/src/zr_vm_lib_container/module.c:661;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedItemsFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用Map/Set私有entries backing键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:862;zr_vm_lib_container/src/zr_vm_lib_container/module.c:899;zr_vm_lib_container/src/zr_vm_lib_container/module.c:677;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedEntriesFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用Map/Set/LinkedList公开count键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:865;zr_vm_lib_container/src/zr_vm_lib_container/module.c:902;zr_vm_lib_container/src/zr_vm_lib_container/module.c:693;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedCountFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用Array公开length键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:848;zr_vm_lib_container/src/zr_vm_lib_container/module.c:868;zr_vm_lib_container/src/zr_vm_lib_container/module.c:905;zr_vm_lib_container/src/zr_vm_lib_container/module.c:709;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedLengthFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用Array公开capacity键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:849;zr_vm_lib_container/src/zr_vm_lib_container/module.c:871;zr_vm_lib_container/src/zr_vm_lib_container/module.c:908;zr_vm_lib_container/src/zr_vm_lib_container/module.c:725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedCapacityFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用LinkedNode载荷value键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:850;zr_vm_lib_container/src/zr_vm_lib_container/module.c:874;zr_vm_lib_container/src/zr_vm_lib_container/module.c:911;zr_vm_lib_container/src/zr_vm_lib_container/module.c:741;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedValueFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用LinkedNode后继next键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:851;zr_vm_lib_container/src/zr_vm_lib_container/module.c:877;zr_vm_lib_container/src/zr_vm_lib_container/module.c:914;zr_vm_lib_container/src/zr_vm_lib_container/module.c:757;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedNextFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用LinkedNode前驱previous键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:852;zr_vm_lib_container/src/zr_vm_lib_container/module.c:880;zr_vm_lib_container/src/zr_vm_lib_container/module.c:917;zr_vm_lib_container/src/zr_vm_lib_container/module.c:773;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedPreviousFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用LinkedList尾节点last键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:853;zr_vm_lib_container/src/zr_vm_lib_container/module.c:883;zr_vm_lib_container/src/zr_vm_lib_container/module.c:920;zr_vm_lib_container/src/zr_vm_lib_container/module.c:789;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedLastFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用LinkedList.first及Pair.first的同文本键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:854;zr_vm_lib_container/src/zr_vm_lib_container/module.c:886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:889;zr_vm_lib_container/src/zr_vm_lib_container/module.c:923;zr_vm_lib_container/src/zr_vm_lib_container/module.c:805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedPairFirstFieldString = ZR_NULL;
    /**
     * ## Summary
     * 跨本函数调用复用Pair第二分量及Map值second键，省去每次建字段键。
     *
     * ## Value model
     * 初值NULL、按域延迟发布永久String；指针及等文本分支通过once helper使用同槽。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 槽只保存名字，不保活目标容器/Pair、字段值或metadata；同文本first不意味着实例字段共享。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:855;zr_vm_lib_container/src/zr_vm_lib_container/module.c:892;zr_vm_lib_container/src/zr_vm_lib_container/module.c:926;zr_vm_lib_container/src/zr_vm_lib_container/module.c:821;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:578;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static SZrString *cachedPairSecondFieldString = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    /**
     * ## Summary
     * 在当前有效state进入名字查询时重新归属静态名字缓存，避免旧域指针穿入新域字段操作。
     *
     * ## Usage
     * 只在cacheIdentity不同的顺序切换时执行；调用前state/global/name已检查；所有13本函数状态属于同一切换边界。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 没有析构、回收或同步；并行更新这些槽的合法调度仍沿GlobalState/宿主入口核查。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:981;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2082;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2138;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:842;zr_vm_lib_container/src/zr_vm_lib_container/module.c:823;zr_vm_lib_container/src/zr_vm_lib_container/module.c:843;zr_vm_lib_container/src/zr_vm_lib_container/module.c:844;zr_vm_lib_container/src/zr_vm_lib_container/module.c:855;zr_vm_core/src/zr_vm_core/runtime/global.c:376
     */
    if (cachedGlobalCacheIdentity != state->global->cacheIdentity) {
        cachedGlobalCacheIdentity = state->global->cacheIdentity;
        memset(cache, 0, sizeof(cache));
        cachedItemsFieldString = ZR_NULL;
        cachedEntriesFieldString = ZR_NULL;
        cachedCountFieldString = ZR_NULL;
        cachedLengthFieldString = ZR_NULL;
        cachedCapacityFieldString = ZR_NULL;
        cachedValueFieldString = ZR_NULL;
        cachedNextFieldString = ZR_NULL;
        cachedPreviousFieldString = ZR_NULL;
        cachedLastFieldString = ZR_NULL;
        cachedPairFirstFieldString = ZR_NULL;
        cachedPairSecondFieldString = ZR_NULL;
    }

    if (fieldName == kContainerItemsField) {
        return zr_container_cache_field_string_once(state, kContainerItemsField, &cachedItemsFieldString);
    }
    if (fieldName == kContainerEntriesField) {
        return zr_container_cache_field_string_once(state, kContainerEntriesField, &cachedEntriesFieldString);
    }
    if (fieldName == kContainerCountField) {
        return zr_container_cache_field_string_once(state, kContainerCountField, &cachedCountFieldString);
    }
    if (fieldName == kContainerLengthField) {
        return zr_container_cache_field_string_once(state, kContainerLengthField, &cachedLengthFieldString);
    }
    if (fieldName == kContainerCapacityField) {
        return zr_container_cache_field_string_once(state, kContainerCapacityField, &cachedCapacityFieldString);
    }
    if (fieldName == kContainerValueField) {
        return zr_container_cache_field_string_once(state, kContainerValueField, &cachedValueFieldString);
    }
    if (fieldName == kContainerNextField) {
        return zr_container_cache_field_string_once(state, kContainerNextField, &cachedNextFieldString);
    }
    if (fieldName == kContainerPreviousField) {
        return zr_container_cache_field_string_once(state, kContainerPreviousField, &cachedPreviousFieldString);
    }
    if (fieldName == kContainerLastField) {
        return zr_container_cache_field_string_once(state, kContainerLastField, &cachedLastFieldString);
    }
    if (fieldName == kContainerFirstField) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (fieldName == kContainerPairFirstField) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (fieldName == kContainerPairSecondField) {
        return zr_container_cache_field_string_once(state, kContainerPairSecondField, &cachedPairSecondFieldString);
    }

    if (zr_container_field_name_equals(fieldName, kContainerItemsField)) {
        return zr_container_cache_field_string_once(state, kContainerItemsField, &cachedItemsFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerEntriesField)) {
        return zr_container_cache_field_string_once(state, kContainerEntriesField, &cachedEntriesFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerCountField)) {
        return zr_container_cache_field_string_once(state, kContainerCountField, &cachedCountFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerLengthField)) {
        return zr_container_cache_field_string_once(state, kContainerLengthField, &cachedLengthFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerCapacityField)) {
        return zr_container_cache_field_string_once(state, kContainerCapacityField, &cachedCapacityFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerValueField)) {
        return zr_container_cache_field_string_once(state, kContainerValueField, &cachedValueFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerNextField)) {
        return zr_container_cache_field_string_once(state, kContainerNextField, &cachedNextFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPreviousField)) {
        return zr_container_cache_field_string_once(state, kContainerPreviousField, &cachedPreviousFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerLastField)) {
        return zr_container_cache_field_string_once(state, kContainerLastField, &cachedLastFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPairFirstField)) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPairSecondField)) {
        return zr_container_cache_field_string_once(state, kContainerPairSecondField, &cachedPairSecondFieldString);
    }

    for (TZrSize index = 0; index < ZR_CONTAINER_FIELD_CACHE_CAPACITY; index++) {
        if (cache[index].fieldName != ZR_NULL && strcmp(cache[index].fieldName, fieldName) == 0) {
            return cache[index].fieldString;
        }
    }

    for (TZrSize index = 0; index < ZR_CONTAINER_FIELD_CACHE_CAPACITY; index++) {
        SZrString *fieldString;

        if (cache[index].fieldName != ZR_NULL) {
            continue;
        }

        fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
        if (fieldString == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));

        cache[index].fieldName = fieldName;
        cache[index].fieldString = fieldString;
        return fieldString;
    }

    ZR_ASSERT(ZR_FALSE);
    return ZR_NULL;
}

/**
 * ## Summary
 * 为通用Object字段Get/Set准备普通String值键，统一复用名字缓存。
 *
 * ## Parameters
 * state/NUL fieldName属于当前域；outKey为可初始化存储，不用此入口覆盖仍持有ownership的旧值。
 *
 * ## Returns
 * 成功写outKey并true；参数或缓存失败false且不写outKey，调用方只在true后读取。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * outKey借用字符串，不取得容器、字段值或域所有权。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2020;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:974;zr_vm_lib_container/src/zr_vm_lib_container/module.c:981
 */
static TZrBool zr_container_make_field_key(SZrState *state, const TZrChar *fieldName, SZrTypeValue *outKey) {
    SZrString *fieldString;

    if (state == ZR_NULL || fieldName == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 取得backing、Pair和Enumerator八热字段角色的共享键槽，供accessor延迟建键。
 *
 * ## Parameters
 * state/global须有效；NULL输入走NULL返回。
 *
 * ## Returns
 * 本进程静态缓存块借用或NULL；不分配。
 *
 * ## Constraints and lifecycle
 * identity变化清零同一缓存块，此缓存视图不能跨域切换保存使用；只缓存键，不保活目标容器或字段值。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1482;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1507;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1532;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1557;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1582;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1607;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1657;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1008;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1030;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1032
 */
static ZR_FORCE_INLINE ZrContainerHotFieldStringCache *zr_container_hot_field_string_cache(SZrState *state) {
    /**
     * ## Summary
     * 向八个字段accessor提供独立热键槽，给backing、Pair和Enumerator读写准备名字。
     *
     * ## Value model
     * 本函数ZrContainerHotFieldStringCache静态块；与普通fallback及Map位置缓存各自独立，返回地址只是可覆盖静态视图。
     *
     * ## Constraints and lifecycle
     * 永久字符串由原GlobalState保活且不搬迁；槽只借用，不析构或转移所有权。换域只丢弃引用，仍活旧域字符串不因此失效。
     * 返回视图不能跨域切换保存；只持名字不持容器或字段值，实际永久槽写经once helper。
     * TODO: 进程静态缓存没有锁；沿GlobalState创建、thread/task宿主入口与两条Map查询核实合法并行及串行协议。cacheIdentity失效不是同步。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1030;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1031;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1032;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1482;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1507;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1532;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1557;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1582;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1607;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1657;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1024;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1509;zr_vm_lib_container/src/zr_vm_lib_container/module.c:569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:587
     */
    static ZrContainerHotFieldStringCache cache = {0};

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache.cacheIdentity != state->global->cacheIdentity) {
        memset(&cache, 0, sizeof(cache));
        cache.cacheIdentity = state->global->cacheIdentity;
    }

    return &cache;
}

/**
 * ## Summary
 * 为两类字符串键Map查找取得MRU和四槽状态，先归属当前GlobalState生命周期。
 *
 * ## Parameters
 * state/global有效；无效时NULL。
 *
 * ## Returns
 * 静态缓存块借用或NULL，不转移对象所有权。
 *
 * ## Constraints and lifecycle
 * identity切换覆盖同一块，缓存视图不可跨切换保存；后续try依赖本accessor先核对域身份。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 * TODO: 沿zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300的Pair.first疑点及gc_cycle对象重写入口核对版本联动和裸地址重定位；相同memberVersion不是键内容或地址仍有效的证明。
 * BUG: 默认空线程锁钩子下，启用supportMultithread的AttachedDomain ThreadScheduler(workerCount=2)可让两个Job<int>在各自worker内构造独立Map并查询字符串键。队列锁在ExecutePreparedJob前释放，NativeEnter只登记状态即解锁；不同backing仍并发读写同一进程静态MRU/四槽/游标，形成C数据竞争。cacheIdentity和GC native模式不是缓存互斥。TODO: 后续明确缓存域/线程归属或建立完整同步；这里只是完整静态调用链证明，未运行并发复现。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4943;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1057;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1083;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1181;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1215;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1256;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1315;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:118;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:123;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:172;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:275;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:280;zr_vm_library/src/zr_vm_library/task/task_runtime.c:490;zr_vm_library/src/zr_vm_library/task/task_runtime.c:593;zr_vm_library/src/zr_vm_library/task/task_runtime.c:886;zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:518;zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:691;zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:350;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_common/include/zr_vm_common/zr_thread_conf.h:18;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6334
 */
static ZR_FORCE_INLINE ZrContainerHotMapLookupCache *zr_container_hot_map_lookup_cache(SZrState *state) {
    /**
     * ## Summary
     * 保存当前域及backing的成功Map查找位置，为字符串键contains、remove及下标读写共享MRU/候选。
     *
     * ## Value model
     * 本函数静态块：identity、entries、key/Pair裸借用、backing版本、MRU、4槽及游标；没有GC根或对象所有权。
     *
     * ## Constraints and lifecycle
     * 视图不能跨域切换保存；同backing/version并不证明Pair仍属于逻辑集合。
     * BUG: 同一backing的generic Map删除末项时，remove的find_index命中已有缓存，或删除前成功扫描并填槽；HashSet_Remove只减elementCount，self.count只改Map自身，entries.memberVersion不变。单域串行、同一字符串对象且删除至下次查询间无GC/其它Map查询替换缓存时，containsKey在真实length扫描前返回true；GET_ITEM可读取已脱链Pair.second。需后续修复backing版本联动或显式失效；bucket节点释放不等于Pair对象释放。
     * 当前gc_cycle的minor原地晋升及old compaction保持普通OBJECT/ARRAY地址；缓存仍不建根。TODO: 对象不可达后不能依靠缓存保活；若未来启用搬迁，须核对静态entries/key/Pair的根及地址重写协议。
     * TODO: Pair.first改变时backing版本联动另沿map_entry_set_first核对；不从同版本推出键内容未变。
     * BUG: 默认空线程锁钩子下，启用supportMultithread的AttachedDomain ThreadScheduler(workerCount=2)可让两个Job<int>在各自worker内构造独立Map并查询字符串键。队列锁在ExecutePreparedJob前释放，NativeEnter只登记状态即解锁；不同backing仍并发读写同一进程静态MRU/四槽/游标，形成C数据竞争。cacheIdentity和GC native模式不是缓存互斥。TODO: 后续明确缓存域/线程归属或建立完整同步；这里只是完整静态调用链证明，未运行并发复现。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1082;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1083;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1086;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4943;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1075;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6292;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4296;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4463;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4489;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6366;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6372;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4809;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4825;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4945;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_core/include/zr_vm_core/hash_set.h:584;zr_vm_core/src/zr_vm_core/object/object.c:1854;tests/container/test_container_runtime.c:1416;tests/container/test_container_runtime.c:1580;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2226;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2718;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2799;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1181;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1215;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1256;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1315;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:118;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:123;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:172;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:275;zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c:280;zr_vm_library/src/zr_vm_library/task/task_runtime.c:490;zr_vm_library/src/zr_vm_library/task/task_runtime.c:593;zr_vm_library/src/zr_vm_library/task/task_runtime.c:886;zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:518;zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:691;zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:350;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_common/include/zr_vm_common/zr_thread_conf.h:18;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6334
     */
    static ZrContainerHotMapLookupCache cache = {0};

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache.cacheIdentity != state->global->cacheIdentity) {
        memset(&cache, 0, sizeof(cache));
        cache.cacheIdentity = state->global->cacheIdentity;
    }

    return &cache;
}

/* 切换 backing 时旧 Pair/下标不可延续；该清零并不执行容器元素析构。 */
/**
 * ## Summary
 * 放弃旧backing成功位置，为新entries建立空缓存状态，避免跨容器复用下标和Pair。
 *
 * ## Parameters
 * cache可NULL（无动作）；entries可NULL，按给定身份记录。
 *
 * ## Returns
 * void；清MRU、候选、替换游标，无失败返回。
 *
 * ## Constraints and lifecycle
 * 保留cacheIdentity，不释放旧对象或修改容器版本；清槽不独立使仍可达对象失效。调用方负责串行使用状态。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1266;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1363;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1424;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1106;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1112;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1118
 */
static ZR_FORCE_INLINE void zr_container_reset_hot_map_lookup_cache_state(ZrContainerHotMapLookupCache *cache,
                                                                          SZrObject *entries) {
    if (cache == ZR_NULL) {
        return;
    }

    cache->entries = entries;
    cache->lastKeyObject = ZR_NULL;
    cache->lastEntriesMemberVersion = 0u;
    cache->lastIndex = 0u;
    cache->lastEntryObject = ZR_NULL;
    cache->nextSlotIndex = 0u;
    memset(cache->slots, 0, sizeof(cache->slots));
}

/**
 * ## Summary
 * 将上层已确认命中交付为MRU及可选结果，使下标和直接Pair查询共享提交口径。
 *
 * ## Parameters
 * cache/keyObject/entryObject须非NULL；version/index为已验证位置；两个输出可NULL。
 *
 * ## Returns
 * void；非NULL输出取得普通下标及Pair裸借用，无复制/所有权转移。
 *
 * ## Constraints and lifecycle
 * 只提交，不检查backing、键内容、版本或下标边界；DEBUG两命中计数同时增加，不增加entryValidationReadCount。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1187;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1431;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1138;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1149;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1154;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1155
 */
static ZR_FORCE_INLINE void zr_container_commit_hot_map_lookup_cache_entry_hit(ZrContainerHotMapLookupCache *cache,
                                                                               SZrRawObject *keyObject,
                                                                               TZrUInt32 entriesMemberVersion,
                                                                               TZrSize index,
                                                                               SZrObject *entryObject,
                                                                               TZrSize *outIndex,
                                                                               SZrObject **outEntryObject) {
    ZR_ASSERT(cache != ZR_NULL);
    ZR_ASSERT(keyObject != ZR_NULL);
    ZR_ASSERT(entryObject != ZR_NULL);

    cache->lastKeyObject = keyObject;
    cache->lastEntriesMemberVersion = entriesMemberVersion;
    cache->lastIndex = index;
    cache->lastEntryObject = entryObject;
#if defined(ZR_DEBUG)
    gZrContainerDebugHotMapLookupStats.hotHitCount++;
    gZrContainerDebugHotMapLookupStats.memberVersionHitCount++;
#endif
    if (outIndex != ZR_NULL) {
        *outIndex = index;
    }
    if (outEntryObject != ZR_NULL) {
        *outEntryObject = entryObject;
    }
}

/**
 * ## Summary
 * 将已验证候选槽转交命中提交口，供MRU重命中及四槽命中使用。
 *
 * ## Parameters
 * slot须非NULL，其cache/key/entry须符合下层断言；输出可NULL。
 *
 * ## Returns
 * void；借用槽下标/Pair，无独立失效返回。
 *
 * ## Constraints and lifecycle
 * 本入口不验证slot内容，也不证明Pair.first未变；调用方先在本次查找内完成候选检查。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1226;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1376;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1182;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1187
 */
static ZR_FORCE_INLINE void zr_container_commit_hot_map_lookup_cache_hit(ZrContainerHotMapLookupCache *cache,
                                                                         const ZrContainerHotMapLookupCacheSlot *slot,
                                                                         TZrSize *outIndex,
                                                                         SZrObject **outEntryObject) {
    ZR_ASSERT(slot != ZR_NULL);
    zr_container_commit_hot_map_lookup_cache_entry_hit(cache,
                                                       slot->keyObject,
                                                       slot->entriesMemberVersion,
                                                       slot->index,
                                                       slot->entryObject,
                                                       outIndex,
                                                       outEntryObject);
}

/**
 * ## Summary
 * 检验单个成功候选是否匹配当前字符串对象键及backing版本，再交付命中。
 *
 * ## Parameters
 * cache/slot属于本次backing；keyObject必须非NULL；version为当前backing版本；输出可NULL。
 *
 * ## Returns
 * cache/slot无效、地址/版本不同或entry为空时false且不改输出；通过检查则更新MRU/统计、写可选输出并true。
 *
 * ## Constraints and lifecycle
 * 不独立拒绝NULL key，高层try先拒绝NULL；违反前提可能进入下层断言。域/backing身份由上层保证，也不重读Pair.first或独立验证下标。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 * TODO: 沿zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300的Pair.first疑点及gc_cycle对象重写入口核对版本联动和裸地址重定位；相同memberVersion不是键内容或地址仍有效的证明。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1380;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1382;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1384;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1386;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1214;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1221;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1226;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1342;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300
 */
static ZR_FORCE_INLINE TZrBool zr_container_try_hot_map_lookup_cache_slot(
        ZrContainerHotMapLookupCache *cache,
        const ZrContainerHotMapLookupCacheSlot *slot,
        SZrRawObject *keyObject,
        TZrUInt32 entriesMemberVersion,
        TZrSize *outIndex,
        SZrObject **outEntryObject) {
    if (cache == ZR_NULL || slot == ZR_NULL || slot->keyObject != keyObject ||
        slot->entriesMemberVersion != entriesMemberVersion || slot->entryObject == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_commit_hot_map_lookup_cache_hit(cache, slot, outIndex, outEntryObject);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为真实字符串键扫描成功记录位置，减少重复扫描；未找到的键不存负结果。
 *
 * ## Parameters
 * state当前域，entries/key/entry为可达借用，index来自当前成功扫描。
 *
 * ## Returns
 * void；accessor失败或entries/key/entry为空不发布位置；accessor仍可在拒绝entry之前切换域清槽。
 *
 * ## Constraints and lifecycle
 * backing改变先清旧位置；已有同key/version槽覆盖不推进游标，空槽插入和满槽轮换均推进。无边界/内容复验或异常反馈。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 * BUG: 本更新器记录key对象身份、entries地址/memberVersion、index与Pair指针，未记录first内容或Pair版本；同域公开C API改原Pair.first后，entries版本和旧key身份不变可使后续命中器跳过 zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300的重读。限无GC/其它Map查询的静态链。TODO: guest STRUCT复制/赋值及moving GC裸地址重定位另待核。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4869;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4879;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4971;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4985;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1249;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1280;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1289;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1310;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1313
 */
static ZR_FORCE_INLINE void zr_container_update_hot_map_lookup_cache(SZrState *state,
                                                                     SZrObject *entries,
                                                                     SZrRawObject *keyObject,
                                                                     TZrSize index,
                                                                     SZrObject *entryObject) {
    ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);
    ZrContainerHotMapLookupCacheSlot slotValue = {0};
    TZrSize insertIndex;
    TZrUInt32 entriesMemberVersion;

    if (cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL || entryObject == ZR_NULL) {
        return;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
    }

    cache->lastKeyObject = keyObject;
    cache->lastEntriesMemberVersion = entriesMemberVersion;
    cache->lastIndex = index;
    cache->lastEntryObject = entryObject;

    slotValue.keyObject = keyObject;
    slotValue.entriesMemberVersion = entriesMemberVersion;
    slotValue.index = index;
    slotValue.entryObject = entryObject;

    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        if (cache->slots[slotIndex].keyObject == keyObject &&
            cache->slots[slotIndex].entriesMemberVersion == entriesMemberVersion) {
            cache->slots[slotIndex] = slotValue;
            return;
        }
    }

    insertIndex = ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT;
    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        if (cache->slots[slotIndex].keyObject == ZR_NULL ||
            cache->slots[slotIndex].entryObject == ZR_NULL) {
            insertIndex = slotIndex;
            break;
        }
    }

    /**
     * ## Summary
     * 用有限成功候选覆盖反复查询，保持热命中而不给任意键集合无限存储。
     *
     * ## Usage
     * 已有同key/version槽覆盖不推进游标；其余优先空槽，满槽才轮换；空槽插入和轮换均推进；update收到NULL entry不发布。
     *
     * ## Constraints and lifecycle
     * 4候选之外有独立MRU；不存负结果，无集合成员验证；accessor在参数拒绝前已可清域。改槽数须同步显式四槽读取与TZrUInt8游标。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4869;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4879;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4971;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4985;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1309;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1279;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1282;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1287;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1289;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1310;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1312;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1313;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1380;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1387
     */
    if (insertIndex == ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT) {
        insertIndex = cache->nextSlotIndex % ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT;
    }
    cache->slots[insertIndex] = slotValue;
    cache->nextSlotIndex = (TZrUInt8)((insertIndex + 1u) % ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT);
}

/**
 * ## Summary
 * 在map_find_index真实扫描前尝试交付已记录位置，miss交回字符串键扫描。
 *
 * ## Parameters
 * state/cache/entries/key非NULL；cache须刚由当前state accessor取得；输出可NULL。
 *
 * ## Returns
 * 无效输入、backing切换或无候选false且不改输出；命中更新MRU/统计、写可选输出并true。
 *
 * ## Constraints and lifecycle
 * 本函数只检查state非NULL，不自行核验global/cacheIdentity；域前提来自zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807调用链。backing改变清槽并miss，版本不同使旧候选不被选中。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 * BUG: 命中只比较keyObject身份与entries版本，不重验Pair版本或first；同域公开C API改原Pair.first后，仍用同一活key且无GC/其它Map查询可在真实扫描前返回旧Pair/index。完整静态链见 zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300。TODO: guest STRUCT复制/赋值及moving GC重定位另待核。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4809;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1342;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1362;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1376;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300
 */
static ZR_FORCE_INLINE TZrBool zr_container_try_hot_map_lookup_cache(SZrState *state,
                                                                     ZrContainerHotMapLookupCache *cache,
                                                                     SZrObject *entries,
                                                                     SZrRawObject *keyObject,
                                                                     TZrSize *outIndex,
                                                                     SZrObject **outEntryObject) {
    TZrUInt32 entriesMemberVersion;

    if (state == ZR_NULL || cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL) {
        return ZR_FALSE;
    }

    entriesMemberVersion = entries->memberVersion;

    /**
     * ## Summary
     * 首次查询不同entries时放弃旧集合成功位置，使上层回到本次真实扫描。
     *
     * ## Usage
     * cache须刚由当前域accessor取得；backing地址不同才清MRU、槽及游标并miss；直接Pair查询1132也有对应边界。
     *
     * ## Constraints and lifecycle
     * 清缓存不析构对象、不改backing版本，输出在miss时保留；同地址则依赖版本，而generic末项删除违反这项调用链契约。
     * BUG: 同一backing的generic Map删除末项时，remove的find_index命中已有缓存，或删除前成功扫描并填槽；HashSet_Remove只减elementCount，self.count只改Map自身，entries.memberVersion不变。单域串行、同一字符串对象且删除至下次查询间无GC/其它Map查询替换缓存时，containsKey在真实length扫描前返回true；GET_ITEM可读取已脱链Pair.second。需后续修复backing版本联动或显式失效；bucket节点释放不等于Pair对象释放。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4809;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1362;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1106;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1363;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1364;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1424;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4945;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6292;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4296;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4463;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4489;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6366;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6372;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4825;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_core/include/zr_vm_core/hash_set.h:584;zr_vm_core/src/zr_vm_core/object/object.c:1854;tests/container/test_container_runtime.c:1416;tests/container/test_container_runtime.c:1580
     */
    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
        return ZR_FALSE;
    }

    if (cache->lastKeyObject == keyObject &&
        cache->lastEntryObject != ZR_NULL &&
        cache->lastEntriesMemberVersion == entriesMemberVersion) {
        ZrContainerHotMapLookupCacheSlot lastSlot = {
                .keyObject = cache->lastKeyObject,
                .entriesMemberVersion = cache->lastEntriesMemberVersion,
                .index = cache->lastIndex,
                .entryObject = cache->lastEntryObject};

        zr_container_commit_hot_map_lookup_cache_hit(cache, &lastSlot, outIndex, outEntryObject);
        return ZR_TRUE;
    }

    if (zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[0], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[1], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[2], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[3], keyObject, entriesMemberVersion, outIndex, outEntryObject)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/**
 * ## Summary
 * 在Map直接取Pair的字符串快路径省去下标输出；NULL交给真实扫描，不说明Map必定缺键。
 *
 * ## Parameters
 * cache刚由当前域accessor取得；entries/key为当前非NULL借用。
 *
 * ## Returns
 * 成功返回MRU或候选Pair借用；无效输入/backing切换/无匹配时NULL，backing切换清旧状态。
 *
 * ## Constraints and lifecycle
 * 没有state或域核验，依赖zr_vm_lib_container/src/zr_vm_lib_container/module.c:4943 accessor；命中更新MRU/统计，不重比Pair.first。
 * entries/key/Pair为普通裸借用，缓存不建立GC根；调用者须保持对象可达，不跨分配、用户回调或moving GC保留旧地址。
 * BUG: 直接热命中只比较entries/key及backing版本，不重比Pair.first；同域公开C API改原first而entries版本未变时，同一活旧key且无GC/其它Map查询可返回旧Pair并跳过 zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300的重读。TODO: guest STRUCT复制/赋值及moving GC重定位另待核。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4945;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4943;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1431;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3300
 */
static ZR_FORCE_INLINE SZrObject *zr_container_try_hot_map_lookup_cache_entry(ZrContainerHotMapLookupCache *cache,
                                                                              SZrObject *entries,
                                                                              SZrRawObject *keyObject) {
    TZrUInt32 entriesMemberVersion;

    if (cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL) {
        return ZR_NULL;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
        return ZR_NULL;
    }

    if (cache->lastKeyObject == keyObject &&
        cache->lastEntryObject != ZR_NULL &&
        cache->lastEntriesMemberVersion == entriesMemberVersion) {
        zr_container_commit_hot_map_lookup_cache_entry_hit(cache,
                                                           keyObject,
                                                           entriesMemberVersion,
                                                           cache->lastIndex,
                                                           cache->lastEntryObject,
                                                           ZR_NULL,
                                                           ZR_NULL);
        return cache->lastEntryObject;
    }

    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        const ZrContainerHotMapLookupCacheSlot *slot = &cache->slots[slotIndex];

        if (slot->keyObject != keyObject ||
            slot->entriesMemberVersion != entriesMemberVersion ||
            slot->entryObject == ZR_NULL) {
            continue;
        }

        zr_container_commit_hot_map_lookup_cache_entry_hit(cache,
                                                           keyObject,
                                                           entriesMemberVersion,
                                                           slot->index,
                                                           slot->entryObject,
                                                           ZR_NULL,
                                                           ZR_NULL);
        return slot->entryObject;
    }

    return ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Map/Set隐藏entries，供backing读取和建立。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4658;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1481;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_entries_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerEntriesField, &cache->entriesFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Array隐藏items，供backing建立。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4679;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1506;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_items_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerItemsField, &cache->itemsFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Pair.first，供首分量读取；与LinkedList.first同文本但实例语义分别属于其对象。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3182;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1531;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_pair_first_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerPairFirstField, &cache->pairFirstFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Pair.second，供次分量读写和Map值更新。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3214;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3249;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1556;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_pair_second_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerPairSecondField, &cache->pairSecondFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Enumerator隐藏source，供来源保活边写入。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2821;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1581;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_source_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerSourceField, &cache->iteratorSourceFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Enumerator.current协议键，供当前值写入和结束清空。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2849;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1606;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_current_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerCurrentField, &cache->iteratorCurrentFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得Array Enumerator隐藏index整数游标键。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2981;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1631;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_index_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerIndexField, &cache->iteratorIndexFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 延迟取得LinkedList Enumerator隐藏nextNode节点游标键。
 *
 * ## Parameters
 * state属于当前已构造GlobalState；本入口不接收目标容器/Pair/迭代器。
 *
 * ## Returns
 * 本域永久String借用；state/global/cache无效或Create返回NULL时NULL，目标读写由调用方决定。
 *
 * ## Constraints and lifecycle
 * 字符串由原GlobalState永久根保活且在域内不搬迁；调用者只借用，不能释放或交给另一域。identity切换只清槽，原域仍存活的旧字符串不因此立即失效。
 * 只缓存名字，不能据此推出目标字段存在、值可达或迭代位置/容器内容不变。
 * TODO: 进程静态缓存没有锁；沿两条Map查询、字段accessor和多GlobalState/mutator入口核对合法并行与同步协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3016;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1656;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210;zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217
 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_next_node_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state,
                                                                  kContainerNextNodeField,
                                                                  &cache->iteratorNextNodeFieldString)
                            : ZR_NULL;
}

/**
 * ## Summary
 * 为容器及内建数组游标提供两种可复用Enumerator原型，把native推进函数与current字段接入核心迭代契约。
 *
 * ## Parameters
 * @param[in,out] state 属于已初始化GlobalState的有效执行状态。
 * @param[in] moveNextFunction 仅本模块数组backing与LinkedList两种native回调身份；不是任意可调用函数。
 *
 * ## Returns
 * @return 当前域的共享借用原型；显式参数拒绝或下游NULL结果返回NULL。分配器异常可能不经过这些分支，NULL不承诺回滚或可重试。
 *
 * ## Constraints and lifecycle
 * 原型、名字及native闭包由其GlobalState的permanent区保活；槽只借用，不拥有Enumerator实例、source或节点。清槽不释放仍活旧域对象；返回地址只在所属域存续期内有效。
 * 核心MoveNext读取iteratorContract；protocol位是能力元数据，不是该派发的必要门槛。数组快路可绕过native闭包。
 * TODO: 核对原型缓存填充的失败原子性：*slot先发布并永久标记，之后才创建闭包和安装Iterator contract/protocol。ClosureNative_New在分配后直接解引用，显式closure==NULL分支不能证明OOM可恢复，也不能证明后续缓存命中是完整装配；核查分配器异常退场及重试，未授故障注入信用。
 * TODO: 这组进程静态原型/名字槽没有跨mutator或跨域首填充同步；另核并行getIterator和换域清槽的发布边界，不从cacheIdentity或permanent标记推出互斥。Map缓存的已证竞争不替代本组原型发布的独立核查。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1685;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1763;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1765;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:4082;zr_vm_core/src/zr_vm_core/object/object.c:4107;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/src/zr_vm_core/object/object.c:4284;zr_vm_core/src/zr_vm_core/object/object.c:4394;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:877;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:879;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1648;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1650;zr_vm_core/src/zr_vm_core/closure/closure.c:239;zr_vm_core/src/zr_vm_core/closure/closure.c:247;tests/iterator/test_enumerator_protocol.c:131
 */
static SZrObjectPrototype *zr_container_iterator_runtime_prototype(SZrState *state,
                                                                   FZrNativeFunction moveNextFunction) {
    /**
     * ## Summary
     * 为本函数的两种原型和current名字提供一个顺序切域边界，防止旧域对象被当作新域缓存复用。
     *
     * ## Value model
     * 初值0的本函数TZrUInt64；只比较/记录当前global.cacheIdentity，不是内容版本或线程身份。
     *
     * ## Constraints and lifecycle
     * 原型、名字及native闭包由其GlobalState的permanent区保活；槽只借用，不拥有Enumerator实例、source或节点。清槽不释放仍活旧域对象；返回地址只在所属域存续期内有效。
     * TODO: 这组进程静态原型/名字槽没有跨mutator或跨域首填充同步；另核并行getIterator和换域清槽的发布边界，不从cacheIdentity或permanent标记推出互斥。Map缓存的已证竞争不替代本组原型发布的独立核查。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1757;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1701;zr_vm_core/src/zr_vm_core/runtime/global.c:36
     */
    static TZrUInt64 cachedGlobalCacheIdentity = 0;
    /**
     * ## Summary
     * 跨本域调用复用Array、内建array和Map/Set数组backing的游标原型，避免每个实例重复装配协议。
     *
     * ## Value model
     * NULL表示尚未发布；由对应moveNext函数指针身份选择slot，*slot承接间接创建、contract写入及返回。
     *
     * ## Constraints and lifecycle
     * 原型、名字及native闭包由其GlobalState的permanent区保活；槽只借用，不拥有Enumerator实例、source或节点。清槽不释放仍活旧域对象；返回地址只在所属域存续期内有效。
     * 非NULL槽不独立证明初始化已完成；TODO: 核对原型缓存填充的失败原子性：*slot先发布并永久标记，之后才创建闭包和安装Iterator contract/protocol。ClosureNative_New在分配后直接解引用，显式closure==NULL分支不能证明OOM可恢复，也不能证明后续缓存命中是完整装配；核查分配器异常退场及重试，未授故障注入信用。
     * TODO: 这组进程静态原型/名字槽没有跨mutator或跨域首填充同步；另核并行getIterator和换域清槽的发布边界，不从cacheIdentity或permanent标记推出互斥。Map缓存的已证竞争不替代本组原型发布的独立核查。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1764;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1771;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1806;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1823;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1826;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1717;zr_vm_core/src/zr_vm_core/object/object.c:4082;zr_vm_core/src/zr_vm_core/object/object.c:4107;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/src/zr_vm_core/object/object.c:4284;zr_vm_core/src/zr_vm_core/object/object.c:4394;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:877;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:879;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1648;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1650
     */
    static SZrObjectPrototype *arrayIteratorPrototype = ZR_NULL;
    /**
     * ## Summary
     * 跨本域调用复用按LinkedNode链接推进的列表游标原型，避免每个实例重复装配协议。
     *
     * ## Value model
     * NULL表示尚未发布；由对应moveNext函数指针身份选择slot，*slot承接间接创建、contract写入及返回。
     *
     * ## Constraints and lifecycle
     * 原型、名字及native闭包由其GlobalState的permanent区保活；槽只借用，不拥有Enumerator实例、source或节点。清槽不释放仍活旧域对象；返回地址只在所属域存续期内有效。
     * 非NULL槽不独立证明初始化已完成；TODO: 核对原型缓存填充的失败原子性：*slot先发布并永久标记，之后才创建闭包和安装Iterator contract/protocol。ClosureNative_New在分配后直接解引用，显式closure==NULL分支不能证明OOM可恢复，也不能证明后续缓存命中是完整装配；核查分配器异常退场及重试，未授故障注入信用。
     * TODO: 这组进程静态原型/名字槽没有跨mutator或跨域首填充同步；另核并行getIterator和换域清槽的发布边界，不从cacheIdentity或permanent标记推出互斥。Map缓存的已证竞争不替代本组原型发布的独立核查。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1759;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1766;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1771;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1806;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1807;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1823;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1826;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1733;zr_vm_core/src/zr_vm_core/object/object.c:4082;zr_vm_core/src/zr_vm_core/object/object.c:4107;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/src/zr_vm_core/object/object.c:4284;zr_vm_core/src/zr_vm_core/object/object.c:4394;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:877;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:879;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1648;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1650
     */
    static SZrObjectPrototype *linkedIteratorPrototype = ZR_NULL;
    /**
     * ## Summary
     * 让两种原型通过同一域内的current名字读取Enumerator已发布的当前值。
     *
     * ## Value model
     * 延迟创建并永久标记的String；是contract字段名字，不是current值，也不保活元素或实例。
     *
     * ## Constraints and lifecycle
     * 原型、名字及native闭包由其GlobalState的permanent区保活；槽只借用，不拥有Enumerator实例、source或节点。清槽不释放仍活旧域对象；返回地址只在所属域存续期内有效。
     * 名字装入contract时按值复制指针；栈local contract本身无需保留。
     * TODO: 这组进程静态原型/名字槽没有跨mutator或跨域首填充同步；另核并行getIterator和换域清槽的发布边界，不从cacheIdentity或permanent标记推出互斥。Map缓存的已证竞争不替代本组原型发布的独立核查。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1760;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1785;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1786;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1787;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1790;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1821;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1749;zr_vm_core/src/zr_vm_core/object/object.c:4394;zr_vm_core/src/zr_vm_core/object/object.c:4328;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:879
     */
    static SZrString *currentMemberName = ZR_NULL;
    SZrObjectPrototype **slot;

    if (state == ZR_NULL || state->global == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    if (cachedGlobalCacheIdentity != state->global->cacheIdentity) {
        cachedGlobalCacheIdentity = state->global->cacheIdentity;
        arrayIteratorPrototype = ZR_NULL;
        linkedIteratorPrototype = ZR_NULL;
        currentMemberName = ZR_NULL;
    }

    if (moveNextFunction == zr_container_array_iterator_move_next_native) {
        slot = &arrayIteratorPrototype;
    } else if (moveNextFunction == zr_container_linked_list_iterator_move_next_native) {
        slot = &linkedIteratorPrototype;
    } else {
        return ZR_NULL;
    }

    if (*slot == ZR_NULL) {
        SZrString *prototypeName;
        SZrClosureNative *closure;
        SZrIteratorContract contract;

        prototypeName = ZrCore_String_CreateFromNative(state,
                                                       moveNextFunction == zr_container_array_iterator_move_next_native
                                                               ? "__zr_container_array_iterator"
                                                               : "__zr_container_linked_iterator");
        if (prototypeName == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototypeName));

        if (currentMemberName == ZR_NULL) {
            currentMemberName = ZrCore_String_CreateFromNative(state, "current");
            if (currentMemberName == ZR_NULL) {
                return ZR_NULL;
            }
            ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentMemberName));
        }

    /**
     * ## Summary
     * 记录原型缓存的发布边界，避免将缓存非NULL与完整Iterator能力误作同一状态。
     *
     * ## Usage
     * 仅在所选slot为空的初始化分支；后续填充moveNext闭包与currentMemberName contract，再声明协议能力。
     *
     * ## Constraints and lifecycle
     * TODO: 核对原型缓存填充的失败原子性：*slot先发布并永久标记，之后才创建闭包和安装Iterator contract/protocol。ClosureNative_New在分配后直接解引用，显式closure==NULL分支不能证明OOM可恢复，也不能证明后续缓存命中是完整装配；核查分配器异常退场及重试，未授故障注入信用。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1806;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1771;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1812;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1823;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1826;zr_vm_core/src/zr_vm_core/closure/closure.c:239;zr_vm_core/src/zr_vm_core/closure/closure.c:247;zr_vm_core/src/zr_vm_core/object/object.c:4394
     */
        *slot = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        if (*slot == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(*slot));

        closure = ZrCore_ClosureNative_New(state, 0);
        if (closure == ZR_NULL) {
            return ZR_NULL;
        }
        closure->nativeFunction = moveNextFunction;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

        memset(&contract, 0, sizeof(contract));
        contract.moveNextFunction = ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
        contract.currentMemberName = currentMemberName;
        ZrCore_ObjectPrototype_SetIteratorContract(*slot, &contract);
        ZrCore_ObjectPrototype_AddProtocol(*slot, ZR_PROTOCOL_ID_ITERATOR);
    }

    return *slot;
}

/**
 * ## Summary
 * 为实例方法与构造目标复用提供活动native调用的接收对象视图，由业务回调继续验证自身契约。
 *
 * ## Parameters
 * @param[in,out] context 活动调用上下文；Self访问刷新栈布局，非空对象转换须有效state。
 *
 * ## Returns
 * @return Self为OBJECT/ARRAY时返回其对象借用指针；缺Self或其它标签为NULL。
 *
 * ## Constraints and lifecycle
 * 只筛值标签并作raw转换，不验证ownerPrototype、内部业务类型、泛型或迭代协议；正确非空payload与布局由调用链保证。借用不能越过调用frame寿命，不retain、不转移owner、不建立root。TODO: 这里不显式拒绝OBJECT/ARRAY标签但payload为空的值；公开ZrLib_CallValue只检查receiver槽指针并复制值，须明确API有效值不变量及未来空payload处理。未有合法坏输入证明，不标BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1900;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5603;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5654;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5699;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5782;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5831;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5873;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5911;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5949;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6028;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6093;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6187;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6235;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6325;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6357;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6400;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6438;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6795;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6839;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6871;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6914;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6952;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7108;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7161;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7214;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7336;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7380;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1850;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:718;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1814;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:472;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:499;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:503;zr_vm_library/include/zr_vm_library/native_binding.h:137;zr_vm_library/include/zr_vm_library/native_binding.h:186;zr_vm_core/include/zr_vm_core/conversion.h:65
 */
static SZrObject *zr_container_self_object(ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

/**
 * ## Summary
 * 为构造复用及Pair相等性检查确认声明owner的原型归属，允许沿派生原型链认领接收者。
 *
 * ## Parameters
 * @param[in] context 可为空，只提供绑定ownerPrototype。
 * @param[in] object 可为空的对象借用；非空时布局与原型链须有效。
 *
 * ## Returns
 * @return owner与object均非空且Core原型链匹配时true，否则false。
 *
 * ## Constraints and lifecycle
 * 只验证原型归属，不验证值标签、internalType、泛型或Pair字段形状；派生对象可以通过。借用不retain、不建立root。构造器仍须初始化字段，Pair.equals仍须按自身值相等性流程处理。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1903;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5621;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1870;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1872;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1901;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:836;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:837;zr_vm_core/src/zr_vm_core/object/object.c:4413;zr_vm_core/src/zr_vm_core/object/object.c:4420;zr_vm_core/src/zr_vm_core/object/object.c:4425
 */
static TZrBool zr_container_object_is_owner_instance(const ZrLibCallContext *context, SZrObject *object) {
    SZrObjectPrototype *ownerPrototype = ZrLib_CallContext_OwnerPrototype(context);
    return ownerPrototype != ZR_NULL && object != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(object, ownerPrototype);
}

/**
 * ## Summary
 * 统一六类容器构造的实例选择，优先复用兼容Self以保留实际派生原型，其余按构造目标或声明owner建立实例。
 *
 * ## Parameters
 * @param[in,out] context 活动native上下文；state须有效，target/Self访问可刷新栈布局，owner与目标由绑定链提供。
 *
 * ## Returns
 * @return 可复用Self或新GC实例的借用指针；context/state无效、最终原型缺失或显式分配失败为NULL，尚未发布Result。
 *
 * ## Constraints and lifecycle
 * Self符合实际target或声明owner任一原型链即可复用；target getter可保留派生Self实际原型。选择不证明业务字段或泛型已初始化；STRUCT原型创建internal STRUCT，值仍用raw OBJECT标签。调用方负责初始化及结果发布，借用不是独立C root；复用对象的中途字段/分配失败无事务回滚。此helper不负责arity或native返回数量。BUG: 已登记容器的公开C调用可取原型.__constructor闭包，并以CallValue(receiver=NULL,arguments=NULL,count=0)进入Array/Map/Set构造；合法零参使Self为空，本函数新分配的target只存于C局部，Result仍NULL且未pin/root。在默认incremental模式、非immediate状态下，随后backing GcMalloc宿主申请失败触发同步GcFull；root扫描不包含该局部，major重置/翻转generation后可sweep target。若retry成功，构造继续用已释放的target指针，产生悬空访问或复用对象误写。此为严格条件性公开C API静态链，不扩展为guest构造或所有GC模式的运行复现。TODO: 新实例在后续可能分配的初始化完成/结果发布前须遵守临时root生命周期；失败退出也须成对释放。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5733;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6293;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6763;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6988;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7075;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1899;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1900;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1901;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1903;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1907;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7493;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7526;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7585;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7608;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7646;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7658;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:225;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:233;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:240;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:245;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:841;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1679;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:472;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:499;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:503;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:873;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:891;zr_vm_core/src/zr_vm_core/object/object.c:4413;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:34;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:51;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:336;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:341;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:502;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1356;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1361;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1842;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1873;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1879;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1921;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1407;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1418;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1429;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1437;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:220;zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:229;zr_vm_library/include/zr_vm_library/native_binding.h:671;zr_vm_core/src/zr_vm_core/object/object.c:1482;zr_vm_core/src/zr_vm_core/object/object.c:1488;zr_vm_core/include/zr_vm_core/raw_object.h:53;zr_vm_core/include/zr_vm_core/raw_object.h:59;zr_vm_core/include/zr_vm_core/raw_object.h:70;zr_vm_core/include/zr_vm_core/raw_object.h:72;zr_vm_core/include/zr_vm_core/raw_object.h:84;zr_vm_core/src/zr_vm_core/base/memory.c:13;zr_vm_core/src/zr_vm_core/base/memory.c:16;zr_vm_core/src/zr_vm_core/base/memory.c:17;zr_vm_core/src/zr_vm_core/base/memory.c:18;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:42;zr_vm_core/src/zr_vm_core/base/memory.c:44;zr_vm_core/src/zr_vm_core/gc/gc.c:455;zr_vm_core/src/zr_vm_core/gc/gc.c:458;zr_vm_core/src/zr_vm_core/gc/gc.c:459;zr_vm_core/src/zr_vm_core/gc/gc.c:766;zr_vm_core/src/zr_vm_core/gc/gc.c:788;zr_vm_core/src/zr_vm_core/gc/gc.c:795;zr_vm_core/src/zr_vm_core/gc/gc.c:796;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1069;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1074;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1086;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1092;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1093;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1106;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:471;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:480;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:494;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:506;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1303;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1310;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1312;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2903;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2920;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2921;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2922;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3129;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3130;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3140;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3250;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3255;zr_vm_core/src/zr_vm_core/gc/gc_internal.h:42;zr_vm_core/src/zr_vm_core/gc/gc_internal.h:51;zr_vm_core/src/zr_vm_core/gc/gc_internal.h:54;zr_vm_core/src/zr_vm_core/gc/gc_internal.h:59;zr_vm_core/src/zr_vm_core/gc/gc_internal.h:60;zr_vm_core/src/zr_vm_core/gc/gc_sweep.c:6;zr_vm_core/src/zr_vm_core/gc/gc_sweep.c:27;zr_vm_core/src/zr_vm_core/gc/gc_sweep.c:34;zr_vm_core/src/zr_vm_core/function/function.c:2014;zr_vm_core/src/zr_vm_core/function/function.c:2029;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6764;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6770;zr_vm_library/include/zr_vm_library/native_binding.h:642;zr_vm_library/include/zr_vm_library/native_binding.h:644;zr_vm_library/include/zr_vm_library/native_binding.h:657;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1416;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1433;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1444;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1448;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1633;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1641;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1661;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1667;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1670;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7918;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7919;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7920;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7921;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7926;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1782;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1785;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1790;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1429;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1446;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7922;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7923
 */
static SZrObject *zr_container_resolve_construct_target(ZrLibCallContext *context) {
    SZrObject *self;
    SZrObjectPrototype *targetPrototype;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    targetPrototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    self = zr_container_self_object(context);
    if (self != ZR_NULL &&
        ((targetPrototype != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(self, targetPrototype)) ||
         zr_container_object_is_owner_instance(context, self))) {
        return self;
    }

    if (targetPrototype == ZR_NULL) {
        targetPrototype = ZrLib_CallContext_OwnerPrototype(context);
    }
    return ZrLib_Type_NewInstanceWithPrototype(context->state, targetPrototype);
}

/**
 * ## Summary
 * 为结果与临时字段值选择数组或普通对象的外部标签，使backing数组保留ARRAY身份。
 *
 * ## Parameters
 * @param[in] object 可为空的对象借用；非空时须有效SZrObject布局。
 *
 * ## Returns
 * @return 非空内部ARRAY为ARRAY，其余情况包括NULL与STRUCT为OBJECT。
 *
 * ## Constraints and lifecycle
 * 仅分类已有对象，不分配、验证raw标签/域/业务类型、复制owner或建立root。STRUCT没有独立SZrTypeValue标签；分类不能替代对象/标签一致性保证，NULL分类为OBJECT不构成发布空payload许可。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1987;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2185;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3622;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3701;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1930;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1960;zr_vm_core/src/zr_vm_core/object/object.c:1482;zr_vm_core/src/zr_vm_core/object/object.c:1485;zr_vm_core/src/zr_vm_core/object/object.c:1488;zr_vm_core/src/zr_vm_core/object/object.c:1496
 */
static EZrValueType zr_container_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY : ZR_VALUE_TYPE_OBJECT;
}

/**
 * ## Summary
 * 为native结果和待发布字段准备无owner的GC对象值，字段可达性由后续发布接口承担。
 *
 * ## Parameters
 * @param[in] state 当前有效执行域状态，object须符合其归属。
 * @param[out] value 可用新/空槽或已释放旧owner的目标，本函数不清理旧owner。
 * @param[in] object 非空、存活、布局和当前域归属有效的借用。
 * @param[in] type 外部SZrTypeValue标签：internal STRUCT仍用OBJECT，数组backing用ARRAY；由分类或固定角色保证对象与标签一致。
 *
 * ## Returns
 * @return 无返回值，目标成为给定标签的普通GC对象值。
 *
 * ## Constraints and lifecycle
 * 非空检查仅为断言；InitAsRawObject的alive断言不代替ValidateValueWrite或域资格检查，后续标签覆盖也不验证一致性。不retain、Value_Copy、释放旧owner或添加独立root；C指针/局部值本身不保活对象。当前调用用于新Result、局部包装或初始化完成的目标，持久字段由后续Core发布及屏障建立边。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1987;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2185;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2817;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3012;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3622;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3701;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6162;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6650;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1956;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1960;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2819;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3014;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_core/src/zr_vm_core/value/value.c:52;zr_vm_core/src/zr_vm_core/value/value.c:57;zr_vm_core/src/zr_vm_core/value/value.c:59;zr_vm_core/src/zr_vm_core/value/value.c:63;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1248;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1931
 */
static ZR_FORCE_INLINE void zr_container_value_set_object_fast(SZrState *state,
                                                               SZrTypeValue *value,
                                                               SZrObject *object,
                                                               EZrValueType type) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = type;
}

/**
 * ## Summary
 * 把上游已初始化的实例发布到native回调单一Result槽，使构造与getIterator共享结果约定。
 *
 * ## Parameters
 * @param[in] context 活动调用上下文，state须有效。
 * @param[out] result dispatcher准备的新空Result或已释放旧owner的可用槽。
 * @param[in] object 当前域中存活、已由上游初始化完成的借用。
 *
 * ## Returns
 * @return context/result/object任一为空为false；普通对象结果包装完成为true，bool不是VM native结果数量。
 *
 * ## Constraints and lifecycle
 * 只检查三个指针，未独立验证state、threadStatus、原型、泛型或初始化完整性。不释放旧Result owner、不retain或转移owner；新空槽前提由dispatcher初始化NULL支持。dispatcher后续Value_Copy搬到闭包栈槽并返回一个结果，此helper不负责那一步。上游初始化/链表插入的副作用不因false回滚。 上游新实例在初始化期间须保持root，C局部在后续分配前不自动保活；resolve_construct_target契约记录无Self公开构造的条件性GC缺陷，本发布函数不补回之前丢失的存活资格。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5582;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5761;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6108;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6304;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6453;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6774;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6967;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7008;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7084;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7140;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7193;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7393;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1983;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1987;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1952;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:671;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:673;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:674;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:675;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:471;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:480;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:494;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:506;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1303;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1310;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1312
 */
static TZrBool zr_container_finish_object(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object) {
    if (context == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_value_set_object_fast(context->state, result, object, zr_container_value_type_for_object(object));
    return ZR_TRUE;
}

/**
 * ## Summary
 * 统一Pair构造/更新、容器元数据及链表连接的字段发布责任，使调用方通过核心复制和写屏障维护热读缓存。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供字段键准备、Core字段发布及其后异常状态观察。
 * @param[in,out] object 字段发布的有效目标对象；写入可同步角色槽与隐藏backing缓存。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 * @param[in] value 只在本次调用读取的借用值；按核心Value复制/ownership协议存入字段，不消耗调用方槽。
 *
 * ## Returns
 * @return 参数/键准备拒绝或core setter后threadStatus非FINE时false；否则在刷新本地缓存后true。true须结合核心写入资格，不能证明底层void setter未静默拒绝。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效，目标对象及非空GC载荷符合该域id/generation归属；本接口的Core setter执行ValidateValueWrite，拒绝时可能静默返回。本批owner BUG静态证明使用有效同域对象和值。写入的是普通对象值槽，不派发字段描述符setter或在此验证声明类型；跨域写入沿核心GC域规则。false不表示已回滚此前字段/分配。items/entries写成功时同步借用backing缓存；其它字段不承担该同步职责。TODO: 底层Object_SetValue/SetExistingPairValueUnchecked返回void；GC域检查拒绝时可不改变threadStatus直接返回。这里的true只代表wrapper未观察到受检查的异常并完成本地路径，不能独立证明字段已接受写入；沿核心资格校验及上游失败传播核对可观察结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2107;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2149;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2186;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3280;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4706;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4715;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4749;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5565;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5569;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6993;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2010;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2026;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2027;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2032;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2037;zr_vm_core/src/zr_vm_core/object/object.c:1730;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static TZrBool zr_container_set_value_field_fast(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrString *fieldString;
    SZrHashKeyValuePair **hotPairSlot;
    TZrBool isHiddenItemsField;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL ||
        !zr_container_make_field_key(state, fieldName, &key)) {
        return ZR_FALSE;
    }

    fieldString = ZR_CAST_STRING(state, key.value.object);
    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    ZrCore_Object_SetValue(state, object, &key, value);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (hotPairSlot != ZR_NULL && fieldString != ZR_NULL) {
        zr_container_refresh_cached_field_slot(state, object, fieldString, hotPairSlot);
    }

    isHiddenItemsField = zr_container_field_name_equals(fieldName, kContainerItemsField) ||
                         zr_container_field_name_equals(fieldName, kContainerEntriesField);
    if (isHiddenItemsField) {
        object->cachedHiddenItemsObject = ZR_NULL;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
            SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

            if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                object->cachedHiddenItemsObject = hiddenItemsObject;
            }
        }
    }
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为容量、计数等容器元数据提供整数发布接口，保留字段角色缓存和成员版本的联动。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供字段名字准备、热槽键核验和Core回退；热槽路径信任已满足的域前提。
 * @param[in,out] object 整数元数据发布的有效目标对象；调用方维持热角色节点有效。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 * @param[in] value 本次发布的有符号64位整数；业务容量/计数范围由调用方约束。
 *
 * ## Returns
 * @return 参数拒绝或通用字段发布报告失败时false；整数热槽本地完成或通用路径报告完成时true。热槽不另查已有threadStatus，回退true仍受void核心setter静默拒绝边界约束。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效且目标对象属于该域；整数热槽直写信任此前提而不执行ValidateValueWrite，字段热槽未中才经set_value的Core setter执行域资格检查。本批owner BUG静态证明使用有效同域对象和值。热槽仅核键；整数新值不含GC引用，不代表被覆盖的旧值无需owner释放。BUG: 公开Core C API在有效state和同域Array上可用Ownership API构造Shared值，经可写Object_SetMember存入capacity；字段运行描述只含name/kind/isWritable，不检查typeName，Core复制Shared增加强引用。随后正常Array.add的容量读取将非整数视作0，并使capacity Pair缓存就绪，整数快写清掉owner元数据却没有释放这份强引用；其余Shared正常释放后仍不能到达最后强引用的drop/归回GC。只证明这条合法C API静态链，不主张guest静态赋值通过或运行复现。TODO: 后续校验旧槽owner并走核心覆盖释放协议。TODO: 底层Object_SetValue/SetExistingPairValueUnchecked返回void；GC域检查拒绝时可不改变threadStatus直接返回。这里的true只代表wrapper未观察到受检查的异常并完成本地路径，不能独立证明字段已接受写入；沿核心资格校验及上游失败传播核对可观察结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5121;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5134;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5757;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5924;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6301;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6372;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6413;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6658;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6771;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6811;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6927;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7054;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7079;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7137;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7190;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7355;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2069;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2083;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2100;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2360;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3644;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5118;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5798;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7458;zr_vm_library/include/zr_vm_library/native_binding.h:347;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1237;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1240;zr_vm_core/src/zr_vm_core/object/object.c:2520;zr_vm_core/src/zr_vm_core/object/object.c:2919;zr_vm_core/src/zr_vm_core/object/object.c:2956;zr_vm_core/src/zr_vm_core/object/object.c:2964;zr_vm_core/include/zr_vm_core/ownership.h:42;zr_vm_core/include/zr_vm_core/ownership.h:67;zr_vm_core/include/zr_vm_core/ownership.h:72;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static TZrBool zr_container_set_int_field_fast(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               TZrInt64 value) {
    SZrHashKeyValuePair **hotPairSlot;
    SZrString *fieldString;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    fieldString = zr_container_cached_field_string(state, fieldName);
    if (hotPairSlot != ZR_NULL &&
        fieldString != ZR_NULL &&
        zr_container_pair_matches_field_string(state, *hotPairSlot, fieldString)) {
    /**
     * ## Summary
     * 缓存命中的整数发布仍须维护成员版本；新值为标量不能替代旧值的owner清理。
     *
     * ## Usage
     * 所属set_int_field_fast在角色槽和字符串键核验成功后进入；不构成独立函数参数接口。
     * 本分支本地完成后返回true，未单独检查已有threadStatus。
     *
     * ## Constraints and lifecycle
     * BUG: 公开Core C API在有效state和同域Array上可用Ownership API构造Shared值，经可写Object_SetMember存入capacity；字段运行描述只含name/kind/isWritable，不检查typeName，Core复制Shared增加强引用。随后正常Array.add的容量读取将非整数视作0，并使capacity Pair缓存就绪，整数快写清掉owner元数据却没有释放这份强引用；其余Shared正常释放后仍不能到达最后强引用的drop/归回GC。只证明这条合法C API静态链，不主张guest静态赋值通过或运行复现。TODO: 后续校验旧槽owner并走核心覆盖释放协议。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2083;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2100;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2069;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/src/zr_vm_core/object/object.c:2964;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5121
     */
        ZR_VALUE_FAST_SET(&(*hotPairSlot)->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
        object->memberVersion++;
        zr_container_cache_string_lookup_pair_mru(object, *hotPairSlot);
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 供Pair初始化、链表连接及清空流程将字段发布为空值，并保持角色缓存与成员版本的可用性。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供字段名字准备、热槽键核验和Core回退；热槽路径信任已满足的域前提。
 * @param[in,out] object 空值发布的有效目标对象；调用方维持热角色节点有效。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 *
 * ## Returns
 * @return 参数拒绝或通用字段发布报告失败时false；空值热槽本地完成或通用路径报告完成时true。热槽不另查已有threadStatus，回退true仍受void核心setter静默拒绝边界约束。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效且目标对象属于该域；null热槽直写信任此前提而不执行ValidateValueWrite，字段热槽未中才经set_value的Core setter执行域资格检查。本批owner BUG静态证明使用有效同域对象和值。热槽仅核键；NULL新值不含GC引用，不代表旧值无需owner释放。BUG: 正常单节点LinkedList中的真实LinkedNode可由公开Ownership API取得Shared视图，再经同域可写Object_SetMember写回first；底层节点类型仍正确，Core复制增加强引用。removeFirst读取该节点并恢复first缓存后，unlink以NULL更新首尾，空值快写命中first热槽并清掉Shared元数据，没有释放该引用，造成最后强引用释放无法到达。此证明限定公开C API静态链；node的next/previous/value无对应热槽，不能扩为同一BUG，也不授guest赋值或动态复现信用。TODO: 旧槽owner须经核心覆盖协议释放。TODO: 底层Object_SetValue/SetExistingPairValueUnchecked返回void；GC域检查拒绝时可不改变threadStatus直接返回。这里的true只代表wrapper未观察到受检查的异常并完成本地路径，不能独立证明字段已接受写入；沿核心资格校验及上游失败传播核对可观察结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2182;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4719;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4753;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4757;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5576;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5577;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7000;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7004;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7080;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7346;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7347;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7353;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7354;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2128;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2139;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2142;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3151;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7048;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7616;zr_vm_library/include/zr_vm_library/native_binding.h:347;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1240;zr_vm_core/src/zr_vm_core/object/object.c:2520;zr_vm_core/src/zr_vm_core/object/object.c:2956;zr_vm_core/include/zr_vm_core/ownership.h:72;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_core/include/zr_vm_core/value.h:90;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static TZrBool zr_container_set_null_field_fast(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrHashKeyValuePair **hotPairSlot;
    SZrString *fieldString;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    fieldString = zr_container_cached_field_string(state, fieldName);
    if (hotPairSlot != ZR_NULL &&
        fieldString != ZR_NULL &&
        zr_container_pair_matches_field_string(state, *hotPairSlot, fieldString)) {
        ZrCore_Value_ResetAsNullNoProfile(&(*hotPairSlot)->value);
        object->memberVersion++;
        zr_container_cache_string_lookup_pair_mru(object, *hotPairSlot);
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 让backing和链表连接调用方按统一字段协议发布借用对象引用，空引用复用字段置空责任。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供对象值包装及委托字段发布。
 * @param[in,out] object 对象引用字段发布的有效目标对象；调用方维持其及载荷有效。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 * @param[in] valueObject 本次借用的有效对象；NULL表示发布guest null，不转移其owner。
 *
 * ## Returns
 * @return 参数或委托路径报告失败时false；委托报告本地完成时true。非空引用的true不单独证明core接受，NULL快路不额外检查已有threadStatus。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效，目标对象及非空valueObject属于该域。非空对象包装后委托set_value执行Core资格检查；NULL委托set_null，热槽分支信任域前提而不执行ValidateValueWrite，热槽未中才经Core检查。本批owner BUG静态证明使用有效同域对象和值。非空对象按实际对象类型包装后经核心字段复制与GC写屏障建立可达边，局部C指针不是root。NULL分支继承set_null已标记的旧owner覆盖BUG边界；借用对象需满足核心域写入及存活规则。TODO: 底层Object_SetValue/SetExistingPairValueUnchecked返回void；GC域检查拒绝时可不改变threadStatus直接返回。这里的true只代表wrapper未观察到受检查的异常并完成本地路径，不能独立证明字段已接受写入；沿核心资格校验及上游失败传播核对可观察结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6770;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7041;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7046;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7048;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7123;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7127;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7131;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7136;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7176;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7180;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7184;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7189;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2171;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2182;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2185;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2186;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2010;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/include/zr_vm_core/conversion.h:65;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static TZrBool zr_container_set_object_field_fast(SZrState *state,
                                                  SZrObject *object,
                                                  const TZrChar *fieldName,
                                                  SZrObject *valueObject) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (valueObject == ZR_NULL) {
        return zr_container_set_null_field_fast(state, object, fieldName);
    }

    zr_container_value_set_object_fast(state, &fieldValue, valueObject, zr_container_value_type_for_object(valueObject));
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 为自有字段快查的最终回退和动态方法查找提供普通对象读取契约，允许使用继承字段。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供字段键准备与普通Core对象值读取。
 * @param[in,out] object 有效接收对象；普通读取可从该对象或其原型取得借用值。 普通Core读取回退遇活跃RAW Array时还可能改变其存储表示。
 * @param[in] fieldName 有效NUL结尾字段名；当前调用使用稳定模块角色常量，动态文本须另核名字缓存寿命。
 *
 * ## Returns
 * @return 字段值槽的借用指针；参数/键准备拒绝或自有及原型读取未命中时NULL；guest null与缺槽不同。
 *
 * ## Constraints and lifecycle
 * 最终Core Object_GetValue先查自有再查原型，不能从此结果推导自有字段归属。返回值或节点只借用，不retain、不新增GC root或转移owner；持有对象和相应节点须持续有效。节点删除、清空或所属map销毁后地址失效；可能覆写字段或改变结构的调用之后重新取得所需值，不能靠缓存指针延长节点或载荷寿命。 普通Core读取回退若接收internal ARRAY、RAW_CANONICAL且raw-int活跃的对象，可先将raw整数存储物化为nodeMap、释放旧raw buffer、切换NODE_CANONICAL并推进storageGeneration，再查字段及原型；这条物化条件不要求dirty位为true。读取目的不保证表示不变，跨该回退须重新取得raw view/节点视图；正常Pair/Enumerator等非RAW Array接收者不触发此条件。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3130;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3696;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:974;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2216;zr_vm_core/src/zr_vm_core/object/object.c:858;zr_vm_core/src/zr_vm_core/object/object.c:1992;zr_vm_core/src/zr_vm_core/object/object.c:2011;zr_vm_core/src/zr_vm_core/object/object.c:2018;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:200;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:214;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507
 */
static const SZrTypeValue *zr_container_get_field_value(SZrState *state,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL ||
        !zr_container_make_field_key(state, fieldName, &key)) {
        return ZR_NULL;
    }
    return ZrCore_Object_GetValue(state, object, &key);
}

/**
 * ## Summary
 * 把容器的字段角色映射到对象缓存槽，供共享查找和发布入口保持相同的热字段约定。
 *
 * ## Parameters
 * @param[in] object 有效缓存持有对象；返回内嵌槽地址供上层管理，调用方维持对象有效。
 * @param[in] fieldName 有效NUL结尾字段名；当前调用使用稳定模块角色常量，动态文本须另核名字缓存寿命。
 *
 * ## Returns
 * @return 对象内角色Pair槽的借用地址；空参数或没有专用角色时NULL。
 *
 * ## Constraints and lifecycle
 * length/count/first共用一槽，capacity/last/second共用另一槽，items/entries也共享角色；迭代器source/current/index/nextNode各有独立专用槽；value/next/previous没有专用槽。槽非空不证明键、map成员关系或节点寿命；使用者须按实际字段核验。地址不建立root，依赖object仍有效。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2025;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2137;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2434;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3057;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2242;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2245;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2251;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2256;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2262;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2265
 */
static ZR_FORCE_INLINE SZrHashKeyValuePair **zr_container_hot_field_pair_slot(SZrObject *object,
                                                                               const TZrChar *fieldName) {
    if (object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (fieldName == kContainerItemsField || fieldName == kContainerEntriesField) {
        return &object->cachedHiddenItemsPair;
    }
    if (fieldName == kContainerLengthField ||
        fieldName == kContainerCountField ||
        fieldName == kContainerFirstField ||
        fieldName == kContainerPairFirstField) {
        return &object->cachedLengthPair;
    }
    if (fieldName == kContainerCapacityField ||
        fieldName == kContainerLastField ||
        fieldName == kContainerPairSecondField) {
        return &object->cachedCapacityPair;
    }
    if (fieldName == kContainerSourceField) {
        return &object->cachedIteratorSourcePair;
    }
    if (fieldName == kContainerCurrentField) {
        return &object->cachedIteratorCurrentPair;
    }
    if (fieldName == kContainerIndexField) {
        return &object->cachedIteratorIndexPair;
    }
    if (fieldName == kContainerNextNodeField) {
        return &object->cachedIteratorNextNodePair;
    }
    if (fieldName == kContainerValueField ||
        fieldName == kContainerNextField ||
        fieldName == kContainerPreviousField) {
        return ZR_NULL;
    }

    if (zr_container_field_name_equals(fieldName, kContainerItemsField) ||
        zr_container_field_name_equals(fieldName, kContainerEntriesField)) {
        return &object->cachedHiddenItemsPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerLengthField) ||
        zr_container_field_name_equals(fieldName, kContainerCountField) ||
        zr_container_field_name_equals(fieldName, kContainerPairFirstField)) {
        return &object->cachedLengthPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerCapacityField) ||
        zr_container_field_name_equals(fieldName, kContainerLastField) ||
        zr_container_field_name_equals(fieldName, kContainerPairSecondField)) {
        return &object->cachedCapacityPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerSourceField)) {
        return &object->cachedIteratorSourcePair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerCurrentField)) {
        return &object->cachedIteratorCurrentPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerIndexField)) {
        return &object->cachedIteratorIndexPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerNextNodeField)) {
        return &object->cachedIteratorNextNodePair;
    }

    return ZR_NULL;
}

/**
 * ## Summary
 * 给字段查找及写后缓存刷新提供无继承、无字符串内容回调的自有节点身份查找。
 *
 * ## Parameters
 * @param[in] object 有效字段持有对象；本函数不修改map。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 *
 * ## Returns
 * @return String键对象身份精确匹配的自有Pair；空参数、无有效桶或无该身份键时NULL。
 *
 * ## Constraints and lifecycle
 * 仅自有nodeMap，不沿原型；名字内容相同但String身份不同可未命中，内容回退由上层决定。哈希重排重链节点不使Pair本身搬迁；节点删除或map销毁会使借用失效。返回值或节点只借用，不retain、不新增GC root或转移owner；持有对象和相应节点须持续有效。节点删除、清空或所属map销毁后地址失效；可能覆写字段或改变结构的调用之后重新取得所需值，不能靠缓存指针延长节点或载荷寿命。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2591;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2669;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3073;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2321;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2331;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2333;zr_vm_core/include/zr_vm_core/hash_set.h:480;zr_vm_core/include/zr_vm_core/hash_set.h:566;zr_vm_core/include/zr_vm_core/hash_set.h:600
 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_string_pair_exact(SZrObject *object,
                                                                                     SZrString *fieldString) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (object == ZR_NULL || fieldString == ZR_NULL || !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL ||
        object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    hash = ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)->hash;
    for (pair = ZrCore_HashSet_GetBucket(&object->nodeMap, hash); pair != ZR_NULL; pair = pair->next) {
        if (pair->key.type == ZR_VALUE_TYPE_STRING &&
            pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
            return pair;
        }
    }

    return ZR_NULL;
}

/**
 * ## Summary
 * 为共享角色槽和两级MRU确认字段键，避免用缓存非空代替字段身份判断。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供候选String键的raw类型转换。
 * @param[in] pair 可为空的候选借用节点；非空候选必须仍有效。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 *
 * ## Returns
 * @return 候选含有效String键且身份或String内容匹配时true，否则false。
 *
 * ## Constraints and lifecycle
 * 只核验键，不证明候选仍属于当前map，更不能解引用已释放节点来验证寿命。String_Equal不派发guest equality；内容匹配不等于String对象身份相同。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2085;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2141;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2570;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2575;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2583;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2664;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2666;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3059;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2360;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2365;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2370;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2375
 */
static ZR_FORCE_INLINE TZrBool zr_container_pair_matches_field_string(SZrState *state,
                                                                      SZrHashKeyValuePair *pair,
                                                                      SZrString *fieldString) {
    SZrString *pairKeyString;

    if (state == ZR_NULL || pair == ZR_NULL || fieldString == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        return ZR_TRUE;
    }

    pairKeyString = ZR_CAST_STRING(state, pair->key.value.object);
    return pairKeyString != ZR_NULL && ZrCore_String_Equal(pairKeyString, fieldString);
}

/**
 * ## Summary
 * 为Pair及热字段读取提供严格的缓存身份门槛，身份不合即让上层选择更完整查找。
 *
 * ## Parameters
 * @param[in] pair 可为空的借用候选；非空节点须仍存活。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 *
 * ## Returns
 * @return String键对象身份精确匹配时返回原候选；否则NULL。
 *
 * ## Constraints and lifecycle
 * 不按名字内容相等回退，不证明map成员关系。返回值或节点只借用，不retain、不新增GC root或转移owner；持有对象和相应节点须持续有效。节点删除、清空或所属map销毁后地址失效；可能覆写字段或改变结构的调用之后重新取得所需值，不能靠缓存指针延长节点或载荷寿命。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3184;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3216;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3256;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2395;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2397;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2402
 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_try_match_cached_field_pair_exact(SZrHashKeyValuePair *pair,
                                                                                            SZrString *fieldString) {
    if (pair == ZR_NULL || fieldString == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        return ZR_NULL;
    }

    return pair;
}

/**
 * ## Summary
 * 为自有优先字段读取提供无结构查找的候选视图，未命中由共享读取层继续处理。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供字段名字准备及角色缓存核验。
 * @param[in] object 有效缓存持有对象；返回热槽所指字段值的借用视图。
 * @param[in] fieldName 有效NUL结尾字段名；当前调用使用稳定模块角色常量，动态文本须另核名字缓存寿命。
 *
 * ## Returns
 * @return 热角色槽键身份匹配时借用其value；参数、角色键或候选不合时NULL。
 *
 * ## Constraints and lifecycle
 * 不沿原型，不在这里刷新Pair/MRU；字段名字准备可更新模块名字缓存。命中只说明键身份，不说明节点成员关系。返回值或节点只借用，不retain、不新增GC root或转移owner；持有对象和相应节点须持续有效。节点删除、清空或所属map销毁后地址失效；可能覆写字段或改变结构的调用之后重新取得所需值，不能靠缓存指针延长节点或载荷寿命。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3120;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2236;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2395;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2434;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2441
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_try_get_hot_field_value_exact(SZrState *state,
                                                                                      SZrObject *object,
                                                                                      const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrHashKeyValuePair **hotPairSlot;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    if (hotPairSlot == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    pair = hotPairSlot != ZR_NULL ? zr_container_try_match_cached_field_pair_exact(*hotPairSlot, fieldString) : ZR_NULL;
    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

/**
 * ## Summary
 * 在字段查找与写入入口间复用最近自有节点，缓存次序不承担字段发布或版本职责。
 *
 * ## Parameters
 * @param[in,out] object 有效缓存持有对象；本次更新两个MRU槽，不承担字段值发布。
 * @param[in] pair 已核验的当前自有节点；NULL表示清空两级MRU。
 *
 * ## Returns
 * @return 无返回值；object为空时不执行更新。
 *
 * ## Constraints and lifecycle
 * 只调整对象内两个借用缓存，不核验节点成员关系，不建立root，也不改变字段值或memberVersion。调用方负责已有节点存活及结构变更时的失效；NULL清槽不释放字段或载荷。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2144;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2533;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2584;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2598;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2667;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2670;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2461;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2466;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2480;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2481
 */
static ZR_FORCE_INLINE void zr_container_cache_string_lookup_pair_mru(SZrObject *object, SZrHashKeyValuePair *pair) {
    if (object == ZR_NULL) {
        return;
    }

    if (pair == ZR_NULL) {
        object->cachedStringLookupPair = ZR_NULL;
        object->cachedStringLookupPair2 = ZR_NULL;
        return;
    }

    if (object->cachedStringLookupPair == pair) {
        return;
    }
    if (object->cachedStringLookupPair2 == pair) {
        object->cachedStringLookupPair2 = object->cachedStringLookupPair;
        object->cachedStringLookupPair = pair;
        return;
    }
    object->cachedStringLookupPair2 = object->cachedStringLookupPair;
    object->cachedStringLookupPair = pair;
}

/**
 * ## Summary
 * 为Iterator.current及Map/Pair已有字段更新提供有限的非拥有快写许可，把owner覆盖责任留给调用方的Core回退。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供成功plain快写的GC写屏障。
 * @param[in,out] object 已有字段的有效持有对象；快写更新该字段的GC写屏障和MRU。
 * @param[in,out] pair object当前有效的自有字段节点；调用方已证明成员关系和目标字段身份。
 * @param[in] value 本次读取的借用源槽；符合Core域写入资格，不因本调用消费调用方槽。
 *
 * ## Returns
 * @return 参数及两侧normalized/no-ownership满足时完成本地快写并true；否则false且本helper不写字段。true不检查已有threadStatus。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效，目标对象及非空GC载荷符合该域id/generation归属；热槽与plain快路信任这一前提，不执行ValidateValueWrite，只有Core setter路径执行资格检查。本批owner BUG静态证明使用有效同域对象和值。 helper维护GC写屏障和MRU，不改变memberVersion；调用方完成版本/隐藏backing职责。false是快路不适用，调用方仍可Core回退；不承诺整体调用回滚。 TODO: normalized/no-ownership仅核值槽owner元数据，不能证明非RefLike STRUCT可浅赋值。Value_Copy对此类STRUCT转入CloneStruct；容器plain helper和Core普通existing-Pair快路却都可直接复制槽位。沿已核实的Iterator.current与Map已有键更新链明确语言/API值复制和身份契约，再决定是否同时调整Core与容器；本批不将该疑点升级为BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2715;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3258;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3381;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2503;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2528;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2531;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4700;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5462;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5470;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6446;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7732;zr_vm_core/include/zr_vm_core/value.h:170;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193;zr_vm_core/src/zr_vm_core/object/object.c:1613;zr_vm_core/src/zr_vm_core/object/object.c:1884;zr_vm_core/src/zr_vm_core/object/object_internal.h:275;zr_vm_core/src/zr_vm_core/object/object_internal.h:292
 */
static ZR_FORCE_INLINE TZrBool zr_container_try_set_existing_pair_value_plain_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   SZrHashKeyValuePair *pair,
                                                                                   const SZrTypeValue *value) {
    /**
     * ## Summary
     * 守住已有值覆盖的owner边界：任一槽不能按无ownership处理时，外围调用者承担通用覆盖而非强制浅写。
     *
     * ## Usage
     * 由所属helper的state/object/pair/value局部上下文进入；不是独立函数参数接口。
     * 此条件拒绝时helper返回false且尚未写目标；三个直接调用方随后选择Core覆盖。
     *
     * ## Constraints and lifecycle
     * 调用方须保证当前state域仍有效，目标对象及非空GC载荷符合该域id/generation归属；热槽与plain快路信任这一前提，不执行ValidateValueWrite，只有Core setter路径执行资格检查。本批owner BUG静态证明使用有效同域对象和值。 任一输入无效或源/旧目标不是normalized/no-ownership都拒绝该快路；调用方随后可能释放旧owner并复制新值。本条件并未验证完整类型、Pair成员关系或STRUCT赋值语义。 TODO: normalized/no-ownership仅核值槽owner元数据，不能证明非RefLike STRUCT可浅赋值。Value_Copy对此类STRUCT转入CloneStruct；容器plain helper和Core普通existing-Pair快路却都可直接复制槽位。沿已核实的Iterator.current与Map已有键更新链明确语言/API值复制和身份契约，再决定是否同时调整Core与容器；本批不将该疑点升级为BUG。
     *
     * ## References
     * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2715;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3258;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3381;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2503;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2522;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2524;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2720;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3261;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3384;zr_vm_core/include/zr_vm_core/value.h:170;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/object/object.c:1884;zr_vm_core/src/zr_vm_core/object/object_internal.h:275;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193
     */
    if (state == ZR_NULL || object == ZR_NULL || pair == ZR_NULL || value == ZR_NULL ||
        !ZrCore_Value_HasNormalizedNoOwnership(&pair->value) ||
        !ZrCore_Value_HasNormalizedNoOwnership(value)) {
        return ZR_FALSE;
    }

    if (&pair->value != value) {
        pair->value = *value;
    }
    if (value->isGarbageCollectable) {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->value);
    }
    zr_container_cache_string_lookup_pair_mru(object, pair);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Pair字段、隐藏backing和现有字段写入共享自有节点契约，缓存未中仍以当前nodeMap为后备。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供String键核验及Core自有nodeMap后备查找。
 * @param[in,out] object 有效字段持有对象；查找可刷新专用Pair槽和MRU。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 * @param[in,out] hotPairSlot 可为空的对象专用Pair槽地址；非空时须属于object并符合字段角色。
 *
 * ## Returns
 * @return 键匹配的自有Pair借用指针；参数/map拒绝或字段未找到时NULL；查询可以刷新专用槽与MRU。
 *
 * ## Constraints and lifecycle
 * 不查原型；角色/MRU命中核身份或内容，精确桶查找后仍可Core HashSet内容回退。Pair与hotPairSlot须属于当前对象，不能以键匹配证明成员关系；本层依赖核心结构变更失效协议。返回值或节点只借用，不retain、不新增GC root或转移owner；持有对象和相应节点须持续有效。节点删除、清空或所属map销毁后地址失效；可能覆写字段或改变结构的调用之后重新取得所需值，不能靠缓存指针延长节点或载荷寿命。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2629;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4525;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2556;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2360;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2321;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2570;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2596;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2598;zr_vm_core/include/zr_vm_core/hash_set.h:600
 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_cached_field_pair_fast(
        SZrState *state,
        SZrObject *object,
        SZrString *fieldString,
        SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.buckets == ZR_NULL || object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    pair = (hotPairSlot != ZR_NULL) ? *hotPairSlot : ZR_NULL;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        return pair;
    }

    pair = object->cachedStringLookupPair;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = object->cachedStringLookupPair2;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        zr_container_cache_string_lookup_pair_mru(object, pair);
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = zr_container_find_own_string_pair_exact(object, fieldString);
    if (pair == ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
        key.type = ZR_VALUE_TYPE_STRING;
        pair = ZrCore_HashSet_Find(state, &object->nodeMap, &key);
    }

    zr_container_cache_string_lookup_pair_mru(object, pair);
    if (hotPairSlot != ZR_NULL) {
        *hotPairSlot = pair;
    }
    return pair;
}

/**
 * ## Summary
 * 为Pair first/second读接口提供严格自有值借用层，继承回退由上层读取入口决定。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供自有节点查找与缓存核验。
 * @param[in,out] object 有效字段持有对象；查找可刷新专用Pair槽和MRU。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 * @param[in,out] hotPairSlot 可为空的对象专用Pair槽地址；非空时须属于object并符合字段角色。
 *
 * ## Returns
 * @return 当前自有Pair.value借用地址；查找拒绝/未中时NULL，guest null仍是存在的值槽。
 *
 * ## Constraints and lifecycle
 * 不沿原型；查找可更新Pair缓存及MRU。返回值或节点只借用，不retain、不新增GC root或转移owner；持有对象和相应节点须持续有效。节点删除、清空或所属map销毁后地址失效；可能覆写字段或改变结构的调用之后重新取得所需值，不能靠缓存指针延长节点或载荷寿命。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3188;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3220;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2624;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2556;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2629
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_get_cached_field_value_fast(
        SZrState *state,
        SZrObject *object,
        SZrString *fieldString,
        SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, hotPairSlot);

    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

/**
 * ## Summary
 * 在字段发布后重新绑定角色槽与MRU，使上层缓存继续指向已存在的当前字段节点。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供String键核验及自有节点身份查找。
 * @param[in,out] object 有效缓存持有对象；本次刷新只更新其Pair/MRU缓存。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 * @param[in,out] hotPairSlot 必需的object角色槽地址；无效参数直接返回。
 *
 * ## Returns
 * @return 无返回值；有效参数下找到节点或NULL均写入目标槽。
 *
 * ## Constraints and lifecycle
 * 只刷新查找缓存，不递增memberVersion、不更新cachedHiddenItemsObject。MRU按键内容核验，桶后备只接受String身份相同，故不提供任意内容等值键查找的保证；调用方须使用与发布一致的字段String。借用节点不另建根，结构删除/销毁后失效。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2032;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2746;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2653;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2321;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2360;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2664;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2669;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2673
 */
static ZR_FORCE_INLINE void zr_container_refresh_cached_field_slot(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldString,
                                                                   SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || hotPairSlot == ZR_NULL) {
        return;
    }

    pair = object->cachedStringLookupPair;
    if (!zr_container_pair_matches_field_string(state, pair, fieldString)) {
        pair = object->cachedStringLookupPair2;
        if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
            zr_container_cache_string_lookup_pair_mru(object, pair);
        } else {
            pair = zr_container_find_own_string_pair_exact(object, fieldString);
            zr_container_cache_string_lookup_pair_mru(object, pair);
        }
    }
    *hotPairSlot = pair;
}

/**
 * ## Summary
 * 在Iterator、Pair和隐藏backing字段发布间共享更新/插入职责，并由调用方决定隐藏数组快指针同步。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供字段发布的写屏障、Core回退和状态观察。
 * @param[in,out] object 字段发布的有效目标对象；本接口同步版本及所要求的隐藏backing缓存。
 * @param[in] fieldString 有效且仍活的本域字段String；只借用，不转移名字所有权。
 * @param[in,out] hotPairSlot 可为空的对象专用Pair槽地址；非空时须属于object并符合字段角色。
 * @param[in] value 本次读取的借用源槽；符合Core域写入资格，不因本调用消费调用方槽。
 * @param[in] refreshHiddenItemsObject 是否在状态正常后同步隐藏Array backing快指针；true强制避开本地plain路径。
 *
 * ## Returns
 * @return 参数拒绝或Core路径观察到threadStatus非FINE时false；本地plain完成或Core路径未观察到该错误时true，不能单凭true证明void Core已接受写入。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效，目标对象及非空GC载荷符合该域id/generation归属；热槽与plain快路信任这一前提，不执行ValidateValueWrite，只有Core setter路径执行资格检查。本批owner BUG静态证明使用有效同域对象和值。 plain路径还要求关闭隐藏刷新且对象两种hidden缓存为空；本地成功不检查threadStatus。已有字段本函数推进版本，新增字段由Core推进；隐藏缓存只在要求刷新及状态正常后同步。false不回滚Core可能已改变的字段/owner，也不保证版本及隐藏指针已同步。TODO: void Core setter可能资格拒绝而不改threadStatus，须沿资格及上游失败传播核对可观察结果。 TODO: normalized/no-ownership仅核值槽owner元数据，不能证明非RefLike STRUCT可浅赋值。Value_Copy对此类STRUCT转入CloneStruct；容器plain helper和Core普通existing-Pair快路却都可直接复制槽位。沿已核实的Iterator.current与Map已有键更新链明确语言/API值复制和身份契约，再决定是否同时调整Core与容器；本批不将该疑点升级为BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2786;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4627;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2697;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2712;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2715;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2720;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2741;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2746;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/object/object.c:1884;zr_vm_core/src/zr_vm_core/object/object_internal.h:275;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_cached_field_value_fast(SZrState *state,
                                                                        SZrObject *object,
                                                                        SZrString *fieldString,
                                                                        SZrHashKeyValuePair **hotPairSlot,
                                                                        const SZrTypeValue *value,
                                                                        TZrBool refreshHiddenItemsObject) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, hotPairSlot);
    if (pair != ZR_NULL) {
        if (!refreshHiddenItemsObject &&
            object->cachedHiddenItemsPair == ZR_NULL &&
            object->cachedHiddenItemsObject == ZR_NULL &&
            zr_container_try_set_existing_pair_value_plain_fast(state, object, pair, value)) {
            object->memberVersion++;
            return ZR_TRUE;
        }

        ZrCore_Object_SetExistingPairValueUnchecked(state, object, pair, value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        object->memberVersion++;
        if (refreshHiddenItemsObject) {
            object->cachedHiddenItemsObject = ZR_NULL;
            if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
                SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

                if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                    object->cachedHiddenItemsObject = hiddenItemsObject;
                }
            }
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    zr_container_refresh_cached_field_slot(state, object, fieldString, hotPairSlot);
    if (refreshHiddenItemsObject) {
        object->cachedHiddenItemsObject = ZR_NULL;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
            SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

            if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                object->cachedHiddenItemsObject = hiddenItemsObject;
            }
        }
    }
    return ZR_TRUE;
}

/**
 * ## Summary
 * 承接source/current/index/nextNode四个迭代角色的值发布，复用字段缓存而不承担容器隐藏backing同步。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供字段发布与Core回退。
 * @param[in,out] iterator 有效同域迭代实例。
 * @param[in] fieldString 存活同域字段键，NULL拒绝。
 * @param[in,out] hotPairSlot 可为空的角色槽地址，非空节点须仍属于iterator对应字段。
 * @param[in] value 有效同域借用，发布不消耗调用方槽。
 *
 * ## Returns
 * @return 键为空或共用setter拒绝为false；本地/回退路径报告完成为true，受Core void边界约束。
 *
 * ## Constraints and lifecycle
 * 须保证state域有效，目标及非空GC载荷符合当前域id/generation。对象、值及角色Pair均为本次借用，缓存不建立独立root或owner。refreshHidden=false，source并非items/entries隐藏缓存同步。普通快路要求新旧槽normalized无owner，其它经Core ownership-aware覆盖；热键核验仍不证明成员身份。false无回滚，plain路径不另查既有threadStatus。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。TODO: normalized无owner不证明非RefLike STRUCT可以浅赋值；容器及Core existing-Pair普通快路都可能浅复制，而Value_Copy可转CloneStruct。须同时核语言/API值复制与身份契约，不新增已证实BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2819;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2979;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3014;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2780;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2785;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2697;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2720;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2503;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2521;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/value/value.c:36;zr_vm_core/src/zr_vm_core/value/value.c:40;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/include/zr_vm_core/value.h:170;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/include/zr_vm_core/value.h:218
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_cached_value_fast(SZrState *state,
                                                                           SZrObject *iterator,
                                                                           SZrString *fieldString,
                                                                           SZrHashKeyValuePair **hotPairSlot,
                                                                           const SZrTypeValue *value) {
    return fieldString != ZR_NULL &&
           zr_container_set_cached_field_value_fast(state, iterator, fieldString, hotPairSlot, value, ZR_FALSE);
}

/**
 * ## Summary
 * 在枚举器构造时连接实时backing或列表，使推进读取同一来源而非长度/内容快照。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，负责键和发布。
 * @param[in,out] iterator 已建原型的有效同域实例。
 * @param[in] source 可为空，非空须存活同域。
 * @param[in] sourceType 非空来源正确OBJECT/ARRAY标签，由业务调用角色保证。
 *
 * ## Returns
 * @return 字段发布结果；空source发布guest null，非空source实际成功发布使字段保持可达。
 *
 * ## Constraints and lifecycle
 * 须保证state域有效，目标及非空GC载荷符合当前域id/generation。对象、值及角色Pair均为本次借用，缓存不建立独立root或owner。局部包装不root或取owner，来源可达依赖实际字段写入与屏障。构造按current/source/index/nextNode顺序写入，后续失败不回滚实例或前序字段；不验证业务来源形状、不捕获版本/长度。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5221;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2814;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2817;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2819;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5223;zr_vm_core/src/zr_vm_core/value/value.c:52;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_source_object_fast(SZrState *state,
                                                                            SZrObject *iterator,
                                                                            SZrObject *source,
                                                                            EZrValueType sourceType) {
    SZrTypeValue fieldValue;

    if (source == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    } else {
        zr_container_value_set_object_fast(state, &fieldValue, source, sourceType);
    }
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_source_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorSourcePair : ZR_NULL,
                                                       &fieldValue);
}

/**
 * ## Summary
 * 为普通数组及链表推进发布当前元素，集中承担current字段的GC/owner复制边界。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，负责键和Core回退。
 * @param[in,out] iterator 有效同域实例，current角色节点须有效。
 * @param[in] value backing元素、节点value或局部标量/空值借用，只借到发布完成。
 *
 * ## Returns
 * @return current发布结果，true受Core写入资格约束，false不表示无副作用。
 *
 * ## Constraints and lifecycle
 * 须保证state域有效，目标及非空GC载荷符合当前域id/generation。对象、值及角色Pair均为本次借用，缓存不建立独立root或owner。normalized新旧无owner可走plain，带owner源/旧目标走Core覆盖，不消耗源槽。先current再index或nextNode，后续失败留下已写current；不能把整数直接覆盖BUG扩为所有current写入。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。TODO: normalized无owner不证明非RefLike STRUCT可以浅赋值；容器及Core existing-Pair普通快路都可能浅复制，而Value_Copy可转CloneStruct。须同时核语言/API值复制与身份契约，不新增已证实BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2922;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2946;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5470;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5513;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5471;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2697;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2720;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/include/zr_vm_core/value.h:170;zr_vm_core/include/zr_vm_core/value.h:218;zr_vm_core/src/zr_vm_core/value/value.c:36
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_value_fast(SZrState *state,
                                                                            SZrObject *iterator,
                                                                            const SZrTypeValue *value) {
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_current_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorCurrentPair : ZR_NULL,
                                                       value);
}

/**
 * ## Summary
 * 为raw数组推进的current/index状态提供不重新查字段的标量发布入口，维持实例版本与MRU。
 *
 * ## Parameters
 * @param[in,out] iterator 有效迭代实例，对象与Pair须存活且处于合法调用域。
 * @param[in] hotPairSlot 角色缓存地址，本函数不改指针；节点须属于iterator目标字段。
 * @param[in] value 待发布整数，元素/游标语义和范围由上游决定。
 *
 * ## Returns
 * @return 对象/槽/节点缺失或键不是非空字符串为false；本地发布并更新版本与MRU后true。
 *
 * ## Constraints and lifecycle
 * 无state参数，不执行域资格、旧owner释放或threadStatus检查；只核键为字符串，不核current/index内容与map成员身份。角色地址有效不保证旧槽无owner。BUG: 正常raw-int Array迭代器的current和__zr_index两个已有own字段，可通过公开Ownership_SharePlainValue与同域Object_SetMember同时写入Shared；Core复制增加强引用且保留Pair。非整数index使Core cached raw/generic快路失配，公开IterMoveNext回退本模块回调，index读取默认0；首元素合法时依次发布current和index，两次本整数快写均清旧owner元数据而未释放强引用。其它Shared正常释放后不能到达最后strong drop/归回GC。此为有效同域公开C API静态可达链，不声称guest赋值、运行复现或强制销毁VM后的永久泄漏。TODO: 整数发布须在覆盖前按核心协议释放旧owner，不能假定公开可写状态槽一直无owner。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:2915;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2972;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2872;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5391;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3648;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3659;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5443;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5444;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5455;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5456;zr_vm_core/src/zr_vm_core/object/object.c:2520;zr_vm_core/src/zr_vm_core/object/object.c:2919;zr_vm_core/src/zr_vm_core/object/object.c:2929;zr_vm_core/src/zr_vm_core/object/object.c:2981;zr_vm_core/src/zr_vm_core/object/object.c:888;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/object/object.c:1889;zr_vm_core/src/zr_vm_core/object/object.c:396;zr_vm_core/src/zr_vm_core/object/object.c:445;zr_vm_core/src/zr_vm_core/object/object.c:4107;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/src/zr_vm_core/object/object.c:4383;zr_vm_core/include/zr_vm_core/ownership.h:71;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:736;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/include/zr_vm_core/value.h:80;zr_vm_core/include/zr_vm_core/value.h:218;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_core/include/zr_vm_core/value.h:236;zr_vm_core/include/zr_vm_core/object.h:488;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4171;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4265;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6092;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6107;zr_vm_core/include/zr_vm_core/object.h:324;zr_vm_core/src/zr_vm_core/object/object.c:2922;zr_vm_core/src/zr_vm_core/object/object.c:904
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_cached_int_pair_fast(SZrObject *iterator,
                                                                              SZrHashKeyValuePair **hotPairSlot,
                                                                              TZrInt64 value) {
    SZrHashKeyValuePair *pair;

    if (iterator == ZR_NULL || hotPairSlot == ZR_NULL || *hotPairSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = *hotPairSlot;
    if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    iterator->memberVersion++;
    iterator->cachedStringLookupPair = pair;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 发布活动raw-int backing有效元素的current整数，缓存未就绪时沿通用current契约回退。
 *
 * ## Parameters
 * @param[in,out] state 有效域状态，热路不以它校验，冷路用于初始化和字段键。
 * @param[in,out] iterator 有效同域实例，current节点不得悬空。
 * @param[in] value raw backing有效下标的整数元素。
 *
 * ## Returns
 * @return 整数热槽或通用current报告完成为true，其它false；热槽不另查threadStatus。
 *
 * ## Constraints and lifecycle
 * 调用方已验证RAW_CANONICAL、buffer、length/capacity和下标，本接口不取得该资格。冷路包装Int再走current_value；先current后index，后步失败不回滚。BUG: 同时经同域公开Core C API将current与__zr_index置为Shared，可使Core快路回退到本模块，默认index0后整数热写丢失旧强引用；完整可达链见cached_int_pair契约。仅静态C API证据，无guest赋值或运行信用。TODO: 按核心覆盖协议处理旧owner。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5455;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2915;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2921;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2922;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2872;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5444;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5449;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5456;zr_vm_core/src/zr_vm_core/object/object.c:396;zr_vm_core/src/zr_vm_core/object/object.c:445;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4171;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4265;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6092;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6107;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5391;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5443;zr_vm_core/include/zr_vm_core/object.h:324;zr_vm_core/include/zr_vm_core/object.h:488;zr_vm_core/include/zr_vm_core/ownership.h:71;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:736;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/src/zr_vm_core/object/object.c:2919;zr_vm_core/src/zr_vm_core/object/object.c:2922;zr_vm_core/src/zr_vm_core/object/object.c:2981;zr_vm_core/src/zr_vm_core/object/object.c:888;zr_vm_core/src/zr_vm_core/object/object.c:904;zr_vm_core/src/zr_vm_core/object/object.c:4383;zr_vm_core/include/zr_vm_core/value.h:80
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_int_fast(SZrState *state,
                                                                          SZrObject *iterator,
                                                                          TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (zr_container_set_iterator_cached_int_pair_fast(iterator,
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorCurrentPair : ZR_NULL,
                                                       value)) {
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_iterator_current_value_fast(state, iterator, &fieldValue);
}

/**
 * ## Summary
 * 在构造和耗尽分支撤去current元素视图，使迭代协议状态同步为空。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供通用current发布。
 * @param[in,out] iterator 有效同域实例，current节点须有效。
 *
 * ## Returns
 * @return 通用current发布的bool结果，耗尽bool由上游finish_move_next另写。
 *
 * ## Constraints and lifecycle
 * 须保证state域有效，目标及非空GC载荷符合当前域id/generation。对象、值及角色Pair均为本次借用，缓存不建立独立root或owner。ResetAsNull只初始化局部fieldValue，目标覆盖由current_value/cached setter负责；旧owner可经Core释放，区别于整数直接清旧owner。false无回滚，不释放source/nextNode或使整个迭代器失效。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5464;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5505;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5517;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2945;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2946;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2697;zr_vm_core/include/zr_vm_core/value.h:90;zr_vm_core/include/zr_vm_core/value.h:218;zr_vm_core/src/zr_vm_core/object/object.c:1867
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_null_fast(SZrState *state, SZrObject *iterator) {
    SZrTypeValue fieldValue;

    ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    return zr_container_set_iterator_current_value_fast(state, iterator, &fieldValue);
}

/**
 * ## Summary
 * 发布数组迭代器下一次访问的实例游标，维持实时backing遍历而不建立修改快照。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域，热路信任上游资格，冷路用于键/值和Core发布。
 * @param[in,out] iterator 有效同域实例，index节点不得悬空。
 * @param[in] value 初始或推进后Int64游标，本函数不检查业务范围/长度。
 *
 * ## Returns
 * @return 热槽或通用发布报告完成为true，其它显式失败false。
 *
 * ## Constraints and lifecycle
 * 只发布状态，不保证对下一次source长度有效，不捕获版本。__zr_index内部命名仍可经公开Core API写已有own字段，非整数读可默认0。先写current后本步失败无回滚。BUG: 同时经同域公开Core C API将current与__zr_index置为Shared，可使Core快路回退到本模块，默认index0后整数热写丢失旧强引用；完整可达链见cached_int_pair契约。仅静态C API证据，无guest赋值或运行信用。TODO: 按核心覆盖协议处理旧owner。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5456;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5471;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2972;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2978;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2979;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5391;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2872;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2886;zr_vm_core/src/zr_vm_core/object/object.c:396;zr_vm_core/src/zr_vm_core/object/object.c:445;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4171;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4265;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6092;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6107;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1822;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5443;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5455;zr_vm_core/include/zr_vm_core/object.h:324;zr_vm_core/include/zr_vm_core/object.h:488;zr_vm_core/include/zr_vm_core/ownership.h:71;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:736;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/src/zr_vm_core/object/object.c:2919;zr_vm_core/src/zr_vm_core/object/object.c:2922;zr_vm_core/src/zr_vm_core/object/object.c:2981;zr_vm_core/src/zr_vm_core/object/object.c:888;zr_vm_core/src/zr_vm_core/object/object.c:904;zr_vm_core/src/zr_vm_core/object/object.c:4383;zr_vm_core/include/zr_vm_core/value.h:80
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_index_fast(SZrState *state,
                                                                    SZrObject *iterator,
                                                                    TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (zr_container_set_iterator_cached_int_pair_fast(iterator,
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorIndexPair : ZR_NULL,
                                                       value)) {
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_index_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorIndexPair : ZR_NULL,
                                                       &fieldValue);
}

/**
 * ## Summary
 * 连接链表枚举器下一节点游标，使推进顺序由当前链接决定，在结束时发布空游标。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供包装与发布。
 * @param[in,out] iterator 有效同域实例，nextNode节点缓存须有效。
 * @param[in] nextNode 可为空，非空须存活同域且适用OBJECT标签的正确链表节点。
 *
 * ## Returns
 * @return 字段发布结果，NULL发布guest null作结束游标。
 *
 * ## Constraints and lifecycle
 * 须保证state域有效，目标及非空GC载荷符合当前域id/generation。对象、值及角色Pair均为本次借用，缓存不建立独立root或owner。局部包装不root或retain，实际字段与屏障建立可达边，不释放节点value或冻结列表。先current后nextNode，本步失败留下current；节点形状和标签由调用链保证。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5223;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3009;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3012;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3014;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5513;zr_vm_core/src/zr_vm_core/value/value.c:52;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_next_node_fast(SZrState *state,
                                                                        SZrObject *iterator,
                                                                        SZrObject *nextNode) {
    SZrTypeValue fieldValue;

    if (nextNode == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    } else {
        zr_container_value_set_object_fast(state, &fieldValue, nextNode, ZR_VALUE_TYPE_OBJECT);
    }
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_next_node_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorNextNodePair : ZR_NULL,
                                                       &fieldValue);
}

/**
 * ## Summary
 * 给共享字段读取层提供自有nodeMap节点视图，复用角色槽和字符串查找缓存而不把继承字段当作自有节点。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供字段名字准备、字符串键核验和自有nodeMap查找。
 * @param[in,out] object 有效字段持有对象；自有字段查找可刷新其Pair/MRU缓存，调用方维持对象和节点有效。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 *
 * ## Returns
 * @return 自有字段查找所借用的C HashKeyValuePair；参数/nodeMap无效或未命中时NULL。
 *
 * ## Constraints and lifecycle
 * 不沿原型链；热槽键核验只确认字段角色，不重新证明该Pair仍属于当前map。缓存有效性依赖核心结构变更时的失效规则。返回值或字段节点只借用，不retain、不新增GC root或转移owner；须保持字段持有者可达。删除/清空节点、覆写值或可能改变字段的回调之后重新取得所需槽，不能靠cached Pair指针延长节点或载荷寿命。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3039;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3059;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3073;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3084;zr_vm_core/include/zr_vm_core/hash_set.h:566;zr_vm_core/include/zr_vm_core/hash_set.h:600
 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_field_pair_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrHashKeyValuePair *pair;
    SZrHashKeyValuePair **hotPairSlot;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.buckets == ZR_NULL || object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    pair = hotPairSlot != ZR_NULL ? *hotPairSlot : ZR_NULL;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        return pair;
    }

    pair = object->cachedStringLookupPair;
    if (pair != ZR_NULL &&
        pair->key.type == ZR_VALUE_TYPE_STRING &&
        pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = zr_container_find_own_string_pair_exact(object, fieldString);
    if (pair != ZR_NULL) {
        object->cachedStringLookupPair = pair;
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    pair = ZrCore_HashSet_Find(state, &object->nodeMap, &key);
    object->cachedStringLookupPair = pair;
    if (hotPairSlot != ZR_NULL) {
        *hotPairSlot = pair;
    }
    return pair;
}

/**
 * ## Summary
 * 为节点载荷、Pair及整数读取共享自有字段优先的借用接口，并把普通对象读取作为最终回退。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供字段名字准备、自有优先查找及普通Core读取回退。
 * @param[in,out] object 有效接收对象；自有优先查找可刷新字段缓存，最终普通读取也可借用原型字段。 普通Core读取回退遇活跃RAW Array时还可能改变其存储表示。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 *
 * ## Returns
 * @return 借用字段值槽；参数/键失败或自有及普通读取均未命中时NULL。guest null仍是存在的非NULL值槽。
 *
 * ## Constraints and lifecycle
 * 名字中的own不保证所有结果都为自有：最终Core Object_GetValue可返回原型继承值；调用方依赖字段归属时须区分该回退。返回值或字段节点只借用，不retain、不新增GC root或转移owner；须保持字段持有者可达。删除/清空节点、覆写值或可能改变字段的回调之后重新取得所需槽，不能靠cached Pair指针延长节点或载荷寿命。 普通Core读取回退若接收internal ARRAY、RAW_CANONICAL且raw-int活跃的对象，可先将raw整数存储物化为nodeMap、释放旧raw buffer、切换NODE_CANONICAL并推进storageGeneration，再查字段及原型；这条物化条件不要求dirty位为true。读取目的不保证表示不变，跨该回退须重新取得raw view/节点视图；正常Pair/Enumerator等非RAW Array接收者不触发此条件。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3154;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3193;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3225;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3648;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5511;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7228;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7305;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3110;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3120;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3130;zr_vm_core/src/zr_vm_core/object/object.c:858;zr_vm_core/src/zr_vm_core/object/object.c:1992;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:200;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:214;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_get_own_field_value_fast(SZrState *state,
                                                                                  SZrObject *object,
                                                                                  const TZrChar *fieldName) {
    SZrHashKeyValuePair *pair;
    const SZrTypeValue *hotValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    hotValue = zr_container_try_get_hot_field_value_exact(state, object, fieldName);
    if (hotValue != ZR_NULL) {
        return hotValue;
    }

    pair = zr_container_find_own_field_pair_fast(state, object, fieldName);
    if (pair != ZR_NULL) {
        return &pair->value;
    }

    return zr_container_get_field_value(state, object, fieldName);
}

/**
 * ## Summary
 * 为迭代来源和链表连接读取提供对象视图，把字段缺失/非引用与有效借用对象区分。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供字段值读取与Debug raw种类转换。
 * @param[in,out] object 有效接收对象；字段读取可刷新缓存，返回载荷由该字段或原型持有。 普通Core读取回退遇活跃RAW Array时还可能改变其存储表示。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 *
 * ## Returns
 * @return 非空OBJECT/ARRAY值所指借用对象；缺字段、guest null、其它标签或空引用时NULL。
 *
 * ## Constraints and lifecycle
 * 字段仍是自有优先并可继承回退。转换宏在Debug检查raw OBJECT/ARRAY种类，不验证Enumerator/LinkedNode等业务形状或声明泛型。调用方维持载荷和持有者有效。返回值或字段节点只借用，不retain、不新增GC root或转移owner；须保持字段持有者可达。删除/清空节点、覆写值或可能改变字段的回调之后重新取得所需槽，不能靠cached Pair指针延长节点或载荷寿命。 普通Core读取回退若接收internal ARRAY、RAW_CANONICAL且raw-int活跃的对象，可先将raw整数存储物化为nodeMap、释放旧raw buffer、切换NODE_CANONICAL并推进storageGeneration，再查字段及原型；这条物化条件不要求dirty位为true。读取目的不保证表示不变，跨该回退须重新取得raw view/节点视图；正常Pair/Enumerator等非RAW Array接收者不触发此条件。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5330;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5370;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5523;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7038;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7039;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7122;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7175;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7262;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7303;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7311;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7343;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7345;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7391;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3151;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3154;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3156;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3161;zr_vm_core/include/zr_vm_core/conversion.h:65;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1206;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:1951;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:200;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:214;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507
 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_object_field_fast(SZrState *state,
                                                                     SZrObject *object,
                                                                     const TZrChar *fieldName) {
    const SZrTypeValue *value = zr_container_get_own_field_value_fast(state, object, fieldName);

    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

/**
 * ## Summary
 * 为Map entry包装及Pair值语义运算提供first的借用字段视图，不复制键或元素。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供first名字准备、缓存核验及字段读取。
 * @param[in,out] pair 持有first字段的有效对象；读取可刷新其字段缓存，NULL由查找失败处理。 普通Core读取回退遇活跃RAW Array时还可能改变其存储表示。
 *
 * ## Returns
 * @return first值槽的借用指针；缺字段或查找失败时NULL，guest null与缺槽不同。
 *
 * ## Constraints and lifecycle
 * 对应角色槽按精确字符串身份核验；不单凭键证明当前map成员关系。最终字段读取可能返回继承值，不验证Pair业务内部形状或K/V类型。返回值或字段节点只借用，不retain、不新增GC root或转移owner；须保持字段持有者可达。删除/清空节点、覆写值或可能改变字段的回调之后重新取得所需槽，不能靠cached Pair指针延长节点或载荷寿命。 普通Core读取回退若接收internal ARRAY、RAW_CANONICAL且raw-int活跃的对象，可先将raw整数存储物化为nodeMap、释放旧raw buffer、切换NODE_CANONICAL并推进storageGeneration，再查字段及原型；这条物化条件不要求dirty位为true。读取目的不保证表示不变，跨该回退须重新取得raw view/节点视图；正常Pair/Enumerator等非RAW Array接收者不触发此条件。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3313;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5626;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5670;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5671;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5707;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3181;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2249;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2395;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3110;zr_vm_core/include/zr_vm_core/hash_set.h:600;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:200;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:214;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_pair_get_first_fast(SZrState *state, SZrObject *pair) {
    SZrString *fieldString = zr_container_pair_first_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair =
            zr_container_try_match_cached_field_pair_exact(pair != ZR_NULL ? pair->cachedLengthPair : ZR_NULL, fieldString);
    const SZrTypeValue *value =
            cachedPair != ZR_NULL
                    ? &cachedPair->value
                    : zr_container_get_cached_field_value_fast(state,
                                                               pair,
                                                               fieldString,
                                                               pair != ZR_NULL ? &pair->cachedLengthPair : ZR_NULL);

    return value != ZR_NULL ? value : zr_container_get_own_field_value_fast(state, pair, kContainerPairFirstField);
}

/**
 * ## Summary
 * 为Map entry包装及Pair值语义运算提供second的借用字段视图，不复制键或元素。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供second名字准备、缓存核验及字段读取。
 * @param[in,out] pair 持有second字段的有效对象；读取可刷新其字段缓存，NULL由查找失败处理。 普通Core读取回退遇活跃RAW Array时还可能改变其存储表示。
 *
 * ## Returns
 * @return second值槽的借用指针；缺字段或查找失败时NULL，guest null与缺槽不同。
 *
 * ## Constraints and lifecycle
 * 对应角色槽按精确字符串身份核验；不单凭键证明当前map成员关系。最终字段读取可能返回继承值，不验证Pair业务内部形状或K/V类型。返回值或字段节点只借用，不retain、不新增GC root或转移owner；须保持字段持有者可达。删除/清空节点、覆写值或可能改变字段的回调之后重新取得所需槽，不能靠cached Pair指针延长节点或载荷寿命。 普通Core读取回退若接收internal ARRAY、RAW_CANONICAL且raw-int活跃的对象，可先将raw整数存储物化为nodeMap、释放旧raw buffer、切换NODE_CANONICAL并推进storageGeneration，再查字段及原型；这条物化条件不要求dirty位为true。读取目的不保证表示不变，跨该回退须重新取得raw view/节点视图；正常Pair/Enumerator等非RAW Array接收者不触发此条件。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3346;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5627;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5629;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5674;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5675;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5708;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2254;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2395;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3110;zr_vm_core/include/zr_vm_core/hash_set.h:600;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:200;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:214;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_pair_get_second_fast(SZrState *state, SZrObject *pair) {
    SZrString *fieldString = zr_container_pair_second_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair =
            zr_container_try_match_cached_field_pair_exact(pair != ZR_NULL ? pair->cachedCapacityPair : ZR_NULL, fieldString);
    const SZrTypeValue *value =
            cachedPair != ZR_NULL
                    ? &cachedPair->value
                    : zr_container_get_cached_field_value_fast(state,
                                                               pair,
                                                               fieldString,
                                                               pair != ZR_NULL ? &pair->cachedCapacityPair : ZR_NULL);

    return value != ZR_NULL ? value : zr_container_get_own_field_value_fast(state, pair, kContainerPairSecondField);
}

/**
 * ## Summary
 * 供Map更新值包装器的缓存缺失回退发布Pair.second，统一普通值与核心ownership覆盖责任。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行域状态；供second名字准备、plain写屏障及Core回退状态观察。
 * @param[in,out] pair 持有second字段的有效对象；成功发布会更新其成员版本和缓存。
 * @param[in] value 本次借用的输入；核心分支按复制协议取得字段值，不消耗调用方槽。
 *
 * ## Returns
 * @return 参数拒绝或受检查的core/通用路径报告失败时false；所选路径报告本地完成时true。两种plain快路不检查已有threadStatus；core回退的true也不独立证明void setter未静默拒绝。
 *
 * ## Constraints and lifecycle
 * 调用方须保证当前state域仍有效，目标对象及非空GC载荷符合该域id/generation归属；热槽与plain快路信任这一前提，不执行ValidateValueWrite，只有Core setter路径执行资格检查。本批owner BUG静态证明使用有效同域对象和值。旧/新槽都normalized no-ownership才进入标量/GC普通值快路；其它已存在字段交核心覆盖并检查threadStatus。缺缓存的set_cached仍可能命中plain字段快路，不能把整个委托结果写成线程正常保证。Map entry wrapper的非空缓存路径绕过本helper，不由此推出所有Map更新均经过second键核验。TODO: 底层Object_SetValue/SetExistingPairValueUnchecked返回void；GC域检查拒绝时可不改变threadStatus直接返回。这里的true只代表wrapper未观察到受检查的异常并完成本地路径，不能独立证明字段已接受写入；沿核心资格校验及上游失败传播核对可观察结果。 TODO: normalized/no-ownership仅核值槽owner元数据，不能证明非RefLike STRUCT可浅赋值。Value_Copy对此类STRUCT转入CloneStruct；容器plain helper和Core普通existing-Pair快路却都可直接复制槽位。沿已核实的Iterator.current与Map已有键更新链明确语言/API值复制和身份契约，再决定是否同时调整Core与容器；本批不将该疑点升级为BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3378;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3246;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3256;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3258;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3280;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3381;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2715;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2503;zr_vm_core/include/zr_vm_core/value.h:170;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193;zr_vm_core/src/zr_vm_core/object/object.c:1613;zr_vm_core/src/zr_vm_core/object/object.c:1884;zr_vm_core/src/zr_vm_core/object/object_internal.h:275;zr_vm_core/src/zr_vm_core/object/object_internal.h:292
 */
static ZR_FORCE_INLINE TZrBool zr_container_pair_set_second_fast(SZrState *state,
                                                                 SZrObject *pair,
                                                                 const SZrTypeValue *value) {
    SZrString *fieldString = zr_container_pair_second_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || pair == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    cachedPair = zr_container_try_match_cached_field_pair_exact(pair->cachedCapacityPair, fieldString);
    if (cachedPair != ZR_NULL) {
        if (!zr_container_try_set_existing_pair_value_plain_fast(state, pair, cachedPair, value)) {
            ZrCore_Object_SetExistingPairValueUnchecked(state, pair, cachedPair, value);
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }

            pair->cachedStringLookupPair = cachedPair;
        }

        pair->memberVersion++;
        return ZR_TRUE;
    }

    if (fieldString != ZR_NULL &&
        zr_container_set_cached_field_value_fast(state,
                                                 pair,
                                                 fieldString,
                                                 pair != ZR_NULL ? &pair->cachedCapacityPair : ZR_NULL,
                                                 value,
                                                 ZR_FALSE)) {
        return ZR_TRUE;
    }
    return zr_container_set_value_field_fast(state, pair, kContainerPairSecondField, value);
}

/**
 * ## Summary
 * 为Map缓存未命中后的键扫描与hash/equality比较提供entry键借用，字段缓存减少重复Pair查找。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域，供冷路字段读取。
 * @param[in,out] entryObject entries中有效同域Pair，冷查可刷新角色缓存。
 *
 * ## Returns
 * @return first槽借用；参数缺失或字段读取失败为NULL，guest null槽与NULL指针不同。
 *
 * ## Constraints and lifecycle
 * cachedLengthPair复用为Pair.first，字段缓存命中不核键内容、成员身份、Pair原型或泛型。冷路pair_get_first可沿Core读取原型字段。值指针不root、retain或复制为guest值；改字段、删节点或用户hash/equality回调后重取，不跨回调保存。BUG: 合法同域公开C API可GetMember取得Map.__zr_entries的同一ARRAY backing，再以Array_Get借用实际Pair，InitAsRawObject包装同一指针并SetMember改可写first为另一个合法字符串。这只推进Pair.memberVersion，不推进entries版本；已用字符串对象A暖缓存、无GC且无其它Map查询时，再以同一活A查询仍可命中旧Pair，跳过本getter的first重读，GET得到旧second或contains误报。完整静态C API链未依赖guest STRUCT别名，不授运行复现信用。TODO: 缓存须联动可写entry键变化或在命中时重验first；guest STRUCT复制/成员赋值路径及moving GC裸地址重定位另待核。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4834;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4893;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4966;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5004;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3308;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3310;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3313;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3181;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3184;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3193;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4894;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5006;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7732;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/include/zr_vm_core/value.h:218;zr_vm_core/src/zr_vm_core/object/object.c:1613;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:58;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6650;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7589;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7590;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7733;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1346;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1367;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1376;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1428;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1437;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4806;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4809;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4815;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4943;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4945;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4947;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6501;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6502;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6581;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6582;zr_vm_core/include/zr_vm_core/object.h:313;zr_vm_core/include/zr_vm_core/object.h:324;zr_vm_library/include/zr_vm_library/native_binding.h:347;zr_vm_library/include/zr_vm_library/native_binding.h:650;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1542;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1548;zr_vm_core/include/zr_vm_core/value.h:53;zr_vm_core/src/zr_vm_core/value/value.c:52;zr_vm_core/src/zr_vm_core/value/value.c:59;zr_vm_core/src/zr_vm_core/value/value.c:63;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1237;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1241;zr_vm_core/src/zr_vm_core/object/object.c:2655;zr_vm_core/src/zr_vm_core/object/object.c:2673;zr_vm_core/src/zr_vm_core/object/object.c:2369;zr_vm_core/src/zr_vm_core/object/object.c:2377;zr_vm_core/src/zr_vm_core/object/object.c:527;zr_vm_core/src/zr_vm_core/object/object.c:531;zr_vm_core/src/zr_vm_core/object/object.c:2520;zr_vm_core/src/zr_vm_core/object/object.c:2536;zr_vm_core/src/zr_vm_core/object/object.c:2919;zr_vm_core/src/zr_vm_core/object/object.c:2922;zr_vm_core/src/zr_vm_core/object/object.c:2956;zr_vm_core/src/zr_vm_core/object/object.c:2964;zr_vm_core/src/zr_vm_core/object/object.c:2966;zr_vm_core/src/zr_vm_core/object/object.c:2976;zr_vm_core/src/zr_vm_core/object/object.c:888;zr_vm_core/src/zr_vm_core/object/object.c:904;zr_vm_core/src/zr_vm_core/object/object.c:50;zr_vm_core/src/zr_vm_core/object/object.c:55
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_map_entry_get_first_value_fast(SZrState *state,
                                                                                        SZrObject *entryObject) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL) {
        return ZR_NULL;
    }

    cachedPair = entryObject->cachedLengthPair;
    if (cachedPair != ZR_NULL) {
        return &cachedPair->value;
    }

    return zr_container_pair_get_first_fast(state, entryObject);
}

/**
 * ## Summary
 * 供Map普通与只读内联get取得已找到entry的映射值，把结果复制责任留在上层。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域，供冷路字段读取。
 * @param[in,out] entryObject 已定位有效同域Pair，冷路可刷新角色缓存。
 *
 * ## Returns
 * @return second槽借用；缺参数或读取失败为NULL，此处不复制native结果。
 *
 * ## Constraints and lifecycle
 * cachedCapacityPair复用为Pair.second，命中不核键内容或map成员身份；冷路pair_get_second可沿Core查原型。借用不root、retain或转移owner，改/删节点或可能执行用户代码后重取。上层result_copy_no_profile决定值复制及owner语义，不构成容器快照；字段查找回退可能刷新Pair字段缓存。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6501;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6581;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3341;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3343;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3346;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3216;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3225;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6502;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6582;zr_vm_core/include/zr_vm_core/value.h:218
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_map_entry_get_second_value_fast(SZrState *state,
                                                                                         SZrObject *entryObject) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL) {
        return ZR_NULL;
    }

    cachedPair = entryObject->cachedCapacityPair;
    if (cachedPair != ZR_NULL) {
        return &cachedPair->value;
    }

    return zr_container_pair_get_second_fast(state, entryObject);
}

/**
 * ## Summary
 * 更新Map已有entry的映射值，保持键、位置与数量，由上游区分更新和插入。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域，供屏障、Core覆盖和状态观察。
 * @param[in,out] entryObject 已定位同域Pair，second缓存须仍属于正确成员。
 * @param[in] value 有效同域借用，可以携owner，不消耗调用方槽。
 *
 * ## Returns
 * @return 参数拒绝、Pair冷路失败或Core回退后threadStatus非FINE为false；其它本地路径完成true，受void资格拒绝边界约束。
 *
 * ## Constraints and lifecycle
 * 须保证state域有效，目标及非空GC载荷符合当前域id/generation。对象、值及角色Pair均为本次借用，缓存不建立独立root或owner。命中不核键或成员身份；新旧normalized无owner走plain，其它Core覆盖并查threadStatus。plain先赋值再调用Barrier，其中ValidateWrite可静默拒绝，不能撤销先前赋值；plain不调用ValidateValueWrite。Core回退则在赋值前ValidateValueWrite，静默拒绝后包装器仍可能更新MRU与entry版本并返回true。更新entry对象版本和MRU，不推进entries/map版本、不改first、插入位置或数量。false可有字段/owner副作用，plain true不另查已有错误。TODO: Core字段setter返回void，赋值前ValidateValueWrite拒绝可不改变threadStatus；true只说明本地完成或未观察受检查错误，须核上游可观察写入结果。TODO: normalized无owner不证明非RefLike STRUCT可以浅赋值；容器及Core existing-Pair普通快路都可能浅复制，而Value_Copy可转CloneStruct。须同时核语言/API值复制与身份契约，不新增已证实BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6739;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3376;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3378;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3381;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3382;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3383;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3387;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3390;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3246;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3258;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2503;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2528;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2531;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6626;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6738;zr_vm_core/src/zr_vm_core/value/value.c:36;zr_vm_core/src/zr_vm_core/value/value.c:40;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/object/object_internal.h:275;zr_vm_core/include/zr_vm_core/value.h:170;zr_vm_core/include/zr_vm_core/value.h:183;zr_vm_core/include/zr_vm_core/value.h:186;zr_vm_core/include/zr_vm_core/value.h:218;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327
 */
static ZR_FORCE_INLINE TZrBool zr_container_map_entry_set_second_value_fast(SZrState *state,
                                                                            SZrObject *entryObject,
                                                                            const SZrTypeValue *value) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    cachedPair = entryObject->cachedCapacityPair;
    if (cachedPair == ZR_NULL) {
        return zr_container_pair_set_second_fast(state, entryObject, value);
    }

    if (!zr_container_try_set_existing_pair_value_plain_fast(state, entryObject, cachedPair, value)) {
        ZrCore_Object_SetExistingPairValueUnchecked(state, entryObject, cachedPair, value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        entryObject->cachedStringLookupPair = cachedPair;
    }

    entryObject->memberVersion++;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为存储移位、Array/Set扫描及Map entry读取提供统一借用槽入口；读取以实际backing为依据，允许Core改变其存储表示，不交付元素快照。
 *
 * ## Parameters
 * @param[in,out] state 有效执行状态，用于raw-int物化和后备读取；物化可能分配及设置错误。
 * @param[in,out] array 已初始化的internal ARRAY backing；读取可能把raw表示物化为节点表示，须保持对象可达。
 * @param[in] index backing位置/整数键，不是经本接口验证的公开length范围；业务调用方先给定扫描或移位界限。
 *
 * ## Returns
 * @return backing内借用值槽；NULL代表输入拒绝、物化失败或查无值，值为guest null时仍可返回非空槽。不能仅凭NULL判断线程状态。
 *
 * ## Constraints and lifecycle
 * 返回槽不retain、不建立独立root；数组写入、raw物化/表示切换、GC或可执行回调之后不能默认旧借用仍有效。后备Array_Get保留Core own/prototype查找语义，不是严格稠密自有元素读取；热路要求桶位、键及范围吻合。TODO: 沿storage_insert/remove_at取得槽后继续写backing的分配路径，以及hash/equals扫描回调，核临时槽保护和重取时机；不能把本接口的借用描述成跨修改快照。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3588;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4436;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4483;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5066;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5080;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5979;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5988;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6053;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6062;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3417;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3422;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3426;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3429;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3432;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3436;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1542;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1544;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1547;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1548;zr_vm_core/src/zr_vm_core/object/object.c:1992;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object.c:2011;zr_vm_core/src/zr_vm_core/object/object.c:2022
 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_array_get_value_fast(SZrState *state,
                                                                             SZrObject *array,
                                                                             TZrSize index) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    if (zr_container_array_raw_int_active(array) &&
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return ZR_NULL;
    }

    if (array->nodeMap.isValid && array->nodeMap.buckets != ZR_NULL && index < array->nodeMap.elementCount &&
        index < array->nodeMap.capacity) {
        pair = array->nodeMap.buckets[index];
        if (pair != ZR_NULL &&
            ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) &&
            pair->key.value.nativeObject.nativeInt64 == (TZrInt64)index) {
            return &pair->value;
        }
    }

    return ZrLib_Array_Get(state, array, index);
}

/**
 * ## Summary
 * 为存储操作和容器扫描提供backing实际计数，并供上层同步公开length/count；不依赖可独立写入的包装对象属性。
 *
 * ## Parameters
 * @param[in] array 有效backing候选；NULL或非internal ARRAY按空计数处理，正常业务入口应传已初始化backing。
 *
 * ## Returns
 * @return 活跃RAW_CANONICAL的raw长度，或nodeMap.elementCount；错误类型/NULL为0。0不报告存储或线程错误。
 *
 * ## Constraints and lifecycle
 * 不验证nodeMap有效性、桶存在、raw dirty位或整数键连续性；调用者不能据此断言公开length/count已同步或某个index可读。TODO: 核公开Core对隐藏backing写稀疏/非整数键后的容器不变量与扫描界限，明确计数能否仍当连续长度；目前不把此计数器称为公开属性getter。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4282;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4417;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4470;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4825;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4884;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4957;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4995;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5047;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5797;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5841;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5960;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6250;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6375;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6661;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6814;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6889;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3456;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3460;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3462;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3463;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3464;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7453;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7469;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7476;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7727
 */
static ZR_FORCE_INLINE TZrSize zr_container_array_length_fast(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0u;
    }

    return (array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
            array->superArrayRawIntData != ZR_NULL &&
            array->superArrayRawIntLength <= array->superArrayRawIntCapacity)
                   ? array->superArrayRawIntLength
                   : array->nodeMap.elementCount;
}

/**
 * ## Summary
 * 为raw存储增删、整数检索与通用读取选择可用表示；以当前RAW_CANONICAL资格为依据，避免把遗留buffer误当现行元素来源。
 *
 * ## Parameters
 * @param[in] array 只读对象候选，可为NULL；只查看表示资格，不改变对象或建立根。
 *
 * ## Returns
 * @return internal ARRAY、RAW_CANONICAL、非空raw数据及长度不超容量同时成立为true，其余为false；不返回物化或同步结果。
 *
 * ## Constraints and lifecycle
 * 该谓词不检查dirty位、nodeMap状态或逐值内容，不证明raw/node数据一致，也不保证跨后续调用仍活跃。通过它取得raw view后遇可能物化、写入或重入操作须重新核资格；不得把true当作root/锁或业务索引验证。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4157;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4321;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4367;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4422;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4475;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5049;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5962;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6036;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6204;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3485;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3486;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3487;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3488;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3422
 */
static ZR_FORCE_INLINE TZrBool zr_container_array_raw_int_active(const SZrObject *array) {
    return array != ZR_NULL &&
           array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
           array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
           array->superArrayRawIntData != ZR_NULL &&
           array->superArrayRawIntLength <= array->superArrayRawIntCapacity;
}

/**
 * ## Summary
 * 给容器clear提供保留backing分配的复用边界；丢弃当前索引关系并让pool从头使用，不能将它当逐值析构接口。
 *
 * ## Parameters
 * @param[in,out] set 待清空的已初始化hash set；NULL、无效、无桶或零容量直接不操作，调用者仍负责宿主和节点值寿命。
 *
 * ## Returns
 * 不返回成功状态；有效集合的桶内容和使用计数归空，桶/pool分配仍保留。无效输入不操作，旧owner及独立Pair不会在此释放。
 *
 * ## Constraints and lifecycle
 * 这里只清空桶链并重置pool使用位置，不逐值ReleaseValue，也不释放独立分配的非pool Pair；保留桶、pool、有效状态供后续写入。BUG: 合法同域公开Core API可将SharePlainValue取得的Shared写入正常Array的__zr_items backing，Object_SetValue/Value_Copy会保留一份strong。Array.clear清桶后正常后续清理不再访问该owner槽；pool重用先ResetAsNull、最终HashSet_Deconstruct只释放宿主块而无Value release，其余Shared正常释放后仍有未配对的strong。另有纯普通值入口：新建零初始容量Array，公开insert(0,value)经storage_set/Object_SetValue/HashSet_Add分配非pool宿主Pair；clear丢其桶链指针，最终析构只访问当前桶链及pool，无法回收该Pair。限正常Array静态链，未授guest Shared构造、Map/Set owner语义、运行复现或VM销毁后的永久泄漏信用。TODO: 明确clear先释放节点key/value的owner及非pool分配、再复用pool的成对清理协议；现有Array<int>测试只核内容不可见与capacity保留，需另核这两类清理边界。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3544;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3510;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3514;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3515;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3516;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3517;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3518;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3519;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4197;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4229;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4230;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5155;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5161;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5169;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5752;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5830;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5918;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5923;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5924;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7469;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7727;zr_vm_core/src/zr_vm_core/object/object.c:1509;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:1529;zr_vm_core/src/zr_vm_core/object/object.c:1545;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1754;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1940;zr_vm_core/include/zr_vm_core/hash_set.h:488;zr_vm_core/include/zr_vm_core/hash_set.h:507;zr_vm_core/include/zr_vm_core/hash_set.h:509;zr_vm_core/include/zr_vm_core/hash_set.h:513;zr_vm_core/include/zr_vm_core/hash_set.h:514;zr_vm_core/include/zr_vm_core/hash_set.h:515;zr_vm_core/include/zr_vm_core/hash_set.h:518;zr_vm_core/src/zr_vm_core/base/hash_set.c:39;zr_vm_core/src/zr_vm_core/base/hash_set.c:46;zr_vm_core/src/zr_vm_core/base/hash_set.c:49;zr_vm_core/src/zr_vm_core/base/hash_set.c:64;zr_vm_core/src/zr_vm_core/base/hash_set.c:73;zr_vm_core/src/zr_vm_core/base/hash_set.c:75;zr_vm_core/src/zr_vm_core/base/hash_set.c:79;zr_vm_core/src/zr_vm_core/base/hash_set.c:80;zr_vm_core/src/zr_vm_core/base/hash_set.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:481;zr_vm_core/src/zr_vm_core/ownership/ownership.c:488;zr_vm_core/src/zr_vm_core/ownership/ownership.c:494;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:736;zr_vm_core/src/zr_vm_core/ownership/ownership.c:738;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:126;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:133;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:134;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:137;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:138;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_core/include/zr_vm_core/value.h:236;zr_vm_core/src/zr_vm_core/value/value.c:238;zr_vm_core/src/zr_vm_core/value/value.c:239;tests/container/test_container_runtime.c:994;tests/container/test_container_runtime.c:1004;tests/container/test_container_runtime.c:1024;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1819;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1416;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1450;zr_vm_library/include/zr_vm_library/native_binding.h:642;zr_vm_core/include/zr_vm_core/ownership.h:73;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1446;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:42;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:81;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:92;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:139;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:152;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:164;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:331;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:339;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:702;zr_vm_core/src/zr_vm_core/gc/gc.c:706;zr_vm_core/src/zr_vm_core/gc/gc.c:711;zr_vm_lib_container/src/zr_vm_lib_container/module.c:44;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4126;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4231;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4410;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4436;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4442;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5162;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5841;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7476;zr_vm_core/src/zr_vm_core/base/hash_set.c:90
 */
static void zr_container_hash_set_clear_reuse_storage(SZrHashSet *set) {
    SZrHashPairPoolBlock *block;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return;
    }

    memset(set->buckets, 0, set->capacity * sizeof(set->buckets[0]));
    set->elementCount = 0;
    set->pairPoolUsed = 0;
    set->pairPoolActive = set->pairPoolHead;
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        block->used = 0;
    }
}

/**
 * ## Summary
 * 作为Array/Map/Set clear的backing层边界，使当前内容、热槽和表示版本失效并保留可复用buffer；公开length/count的发布继续由包装对象层负责。
 *
 * ## Parameters
 * @param[in,out] items 已初始化的internal ARRAY backing，须保持有效；NULL或内部类型不匹配不操作，本接口不接收包装对象。
 *
 * ## Returns
 * 不返回成功状态；backing内容/热槽/表示版本由本层处理，包装对象length/count尚未更新。上层随后字段写失败不会恢复已清空内容。
 *
 * ## Constraints and lifecycle
 * 清空会改变memberVersion和storageGeneration，raw长度归零且可保留buffer；调用方先前的桶槽、枚举缓存及raw view不能因buffer仍分配而继续当作有效元素。没有独立root、互斥或回滚；Array.clear之后另写length，Map/Set之后另写count并有各自缓存处理。这里只清空桶链并重置pool使用位置，不逐值ReleaseValue，也不释放独立分配的非pool Pair；保留桶、pool、有效状态供后续写入。BUG: 合法同域公开Core API可将SharePlainValue取得的Shared写入正常Array的__zr_items backing，Object_SetValue/Value_Copy会保留一份strong。Array.clear清桶后正常后续清理不再访问该owner槽；pool重用先ResetAsNull、最终HashSet_Deconstruct只释放宿主块而无Value release，其余Shared正常释放后仍有未配对的strong。另有纯普通值入口：新建零初始容量Array，公开insert(0,value)经storage_set/Object_SetValue/HashSet_Add分配非pool宿主Pair；clear丢其桶链指针，最终析构只访问当前桶链及pool，无法回收该Pair。限正常Array静态链，未授guest Shared构造、Map/Set owner语义、运行复现或VM销毁后的永久泄漏信用。TODO: 明确clear先释放节点key/value的owner及非pool分配、再复用pool的成对清理协议；现有Array<int>测试只核内容不可见与capacity保留，需另核这两类清理边界。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5923;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6926;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3510;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3514;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3515;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3516;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3517;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3518;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3519;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3544;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4197;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4229;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4230;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5155;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5161;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5169;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5752;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5830;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5918;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5924;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7469;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7727;zr_vm_core/src/zr_vm_core/object/object.c:1509;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:1529;zr_vm_core/src/zr_vm_core/object/object.c:1545;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1754;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1940;zr_vm_core/include/zr_vm_core/hash_set.h:488;zr_vm_core/include/zr_vm_core/hash_set.h:507;zr_vm_core/include/zr_vm_core/hash_set.h:509;zr_vm_core/include/zr_vm_core/hash_set.h:513;zr_vm_core/include/zr_vm_core/hash_set.h:514;zr_vm_core/include/zr_vm_core/hash_set.h:515;zr_vm_core/include/zr_vm_core/hash_set.h:518;zr_vm_core/src/zr_vm_core/base/hash_set.c:39;zr_vm_core/src/zr_vm_core/base/hash_set.c:46;zr_vm_core/src/zr_vm_core/base/hash_set.c:49;zr_vm_core/src/zr_vm_core/base/hash_set.c:64;zr_vm_core/src/zr_vm_core/base/hash_set.c:73;zr_vm_core/src/zr_vm_core/base/hash_set.c:75;zr_vm_core/src/zr_vm_core/base/hash_set.c:79;zr_vm_core/src/zr_vm_core/base/hash_set.c:80;zr_vm_core/src/zr_vm_core/base/hash_set.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:481;zr_vm_core/src/zr_vm_core/ownership/ownership.c:488;zr_vm_core/src/zr_vm_core/ownership/ownership.c:494;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:736;zr_vm_core/src/zr_vm_core/ownership/ownership.c:738;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:126;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:133;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:134;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:137;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:138;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:152;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_core/include/zr_vm_core/value.h:236;zr_vm_core/src/zr_vm_core/value/value.c:238;zr_vm_core/src/zr_vm_core/value/value.c:239;tests/container/test_container_runtime.c:994;tests/container/test_container_runtime.c:1004;tests/container/test_container_runtime.c:1024;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3540;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3545;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3555;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3556;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3558;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3560;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3561;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3563;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3564;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1819;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1416;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1450;zr_vm_library/include/zr_vm_library/native_binding.h:642;zr_vm_core/include/zr_vm_core/ownership.h:73;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1446;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:42;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:81;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:92;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:139;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:152;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:164;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:331;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:339;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:702;zr_vm_core/src/zr_vm_core/gc/gc.c:706;zr_vm_core/src/zr_vm_core/gc/gc.c:711;zr_vm_lib_container/src/zr_vm_lib_container/module.c:44;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4126;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4231;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4410;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4436;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4442;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5162;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5841;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7476;zr_vm_core/src/zr_vm_core/base/hash_set.c:90
 */
static void zr_container_array_clear_items_reuse_storage(SZrObject *items) {
    if (items == ZR_NULL || items->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return;
    }

    zr_container_hash_set_clear_reuse_storage(&items->nodeMap);
    items->cachedHiddenItemsPair = ZR_NULL;
    items->cachedHiddenItemsObject = ZR_NULL;
    items->cachedLengthPair = ZR_NULL;
    items->cachedCapacityPair = ZR_NULL;
    items->cachedStringLookupPair = ZR_NULL;
    items->cachedStringLookupPair2 = ZR_NULL;
    items->cachedIteratorSourcePair = ZR_NULL;
    items->cachedIteratorCurrentPair = ZR_NULL;
    items->cachedIteratorIndexPair = ZR_NULL;
    items->cachedIteratorNextNodePair = ZR_NULL;
    items->superArrayRawIntLength = 0;
    if (items->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
        items->superArrayRawIntData != ZR_NULL) {
        items->superArrayStorageGeneration++;
    } else {
        items->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_NONE;
        items->superArrayStorageGeneration++;
    }
    items->superArrayRawIntDirty = ZR_FALSE;
    items->memberVersion++;
}

/**
 * ## Summary
 * 为Map entry扫描取得backing中的对象借用视图；它只筛OBJECT标签，Pair原型/键值契约继续由上层负责。
 *
 * ## Parameters
 * @param[in,out] state 当前有效执行状态，传给底层读取/物化；错误状态由调用链消费。
 * @param[in,out] array 有效且可达的internal ARRAY backing，底层读取可能改变表示。
 * @param[in] index 由扫描调用方给定的backing位置，需遵守其连续索引不变量。
 *
 * ## Returns
 * @return 非空OBJECT元素的借用对象指针；缺值、精确标签不是OBJECT或payload为空返回NULL，ARRAY标签也被拒绝。
 *
 * ## Constraints and lifecycle
 * 不确认Pair类型、ownerPrototype或泛型，也不retain/root。底层借用在GC、表示切换和hash/equals用户回调后的有效期不能默认保持。TODO: 沿Map扫描使用entry指针跨hash/equals调用的实际路径，核重取/保护与重入修改边界；暂不声称这些扫描有版本快照或并发锁。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4828;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4887;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4998;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3588;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3590;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3594;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4894;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5006
 */
static ZR_FORCE_INLINE SZrObject *zr_container_array_get_object_fast(SZrState *state,
                                                                     SZrObject *array,
                                                                     TZrSize index) {
    const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, array, index);

    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

/**
 * ## Summary
 * 为Array赋值的Core SuperArraySetInt回退准备包装对象receiver，让Core解析其hidden backing；raw-int直接写分支无需该临时接收值。
 *
 * ## Parameters
 * @param[in,out] state 有效当前VM状态，用于raw对象包装与资格断言。
 * @param[in] self 非空且有效的Array包装对象，调用方维持其当前域可达性。
 * @param[out] outReceiver 非空临时输出槽，须新/空或旧owner已释放；不是要覆盖的持owner字段。
 *
 * ## Returns
 * 通过outReceiver交付普通receiver值，不返回Core赋值成功状态、不转移self的owner。
 *
 * ## Constraints and lifecycle
 * 对三个指针的断言不替代业务原型、域或生命周期检查；包装不Value_Copy/retain、不建立独立root。唯一调用位于Array setter非raw-int分支，调用方在随后Core setter完成前维持self有效；不能仅凭该临时C值保证后续分配的可达性。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6267;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3618;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3619;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3620;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3622;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6263;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1967;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2148;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2151
 */
static ZR_FORCE_INLINE void zr_container_array_make_receiver_value(SZrState *state,
                                                                   SZrObject *self,
                                                                   SZrTypeValue *outReceiver) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(self != ZR_NULL);
    ZR_ASSERT(outReceiver != ZR_NULL);

    zr_container_value_set_object_fast(state, outReceiver, self, zr_container_value_type_for_object(self));
}

/**
 * ## Summary
 * 为容量、计数和迭代位置提供带默认策略的整数读取接口，避免把缺字段或非整数载荷解释成nativeInt64。
 *
 * ## Parameters
 * @param[in,out] state 当前域执行上下文；供自有优先字段读取，查找失败由defaultValue承接。
 * @param[in,out] object 有效接收对象；字段读取可刷新缓存，整数结果按值返回。 普通Core读取回退遇活跃RAW Array时还可能改变其存储表示。
 * @param[in] fieldName 有效的NUL结尾字段名；当前调用均为模块稳定角色常量，新增动态名字须另核键缓存的文本寿命。
 * @param[in] defaultValue 未取得有符号/无符号整数字段时交付的默认值；不代表读错误恢复完成。
 *
 * ## Returns
 * @return 有符号整数内容、无符号内容转换到TZrInt64的结果，或defaultValue；不验证后续业务范围。
 *
 * ## Constraints and lifecycle
 * 读取仍是自有优先/原型回退；无符号转TZrInt64没有范围拒绝。TODO: 沿公开Core成员写入及整数构造入口，核对超INT64值进入capacity/count/index后各调用方算术与恢复边界，建立完整影响证据；未授guest静态赋值或动态边界用例信用。 普通Core读取回退若接收internal ARRAY、RAW_CANONICAL且raw-int活跃的对象，可先将raw整数存储物化为nodeMap、释放旧raw buffer、切换NODE_CANONICAL并推进storageGeneration，再查字段及原型；这条物化条件不要求dirty位为true。读取目的不保证表示不变，跨该回退须重新取得raw view/节点视图；正常Pair/Enumerator等非RAW Array接收者不触发此条件。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5118;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7053;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7135;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7188;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3644;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3648;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3653;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3656;zr_vm_core/src/zr_vm_core/object/object.c:2520;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:200;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:214;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507
 */
static TZrInt64 zr_container_get_int_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrInt64 defaultValue) {
    const SZrTypeValue *value = zr_container_get_own_field_value_fast(state, object, fieldName);

    if (value == ZR_NULL) {
        return defaultValue;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

/**
 * ## Summary
 * 为equals、compareTo和hashCode共用对象协议调用入口；属性查找与同步方法调用由这一层协作，调用方继续解释各协议结果及线程错误。
 *
 * ## Parameters
 * @param[in,out] state 当前有效VM状态，用于属性解析和调用；receiver、callable及参数保持当前域合法性，非空检查不代替域验证。
 * @param[in,out] receiver 有效且可达的接收对象，当前调用从OBJECT/ARRAY取得；查询或回调可能刷新缓存/表示及修改对象。
 * @param[in] methodName 有效NUL结尾方法名，当前为稳定的equals/compareTo/hashCode文本。
 * @param[in] arguments 借用参数数组，argumentCount>0须非空并具有足够元素，0参可空；不由本接口转移owner。
 * @param[in] argumentCount 显式参数个数，receiver另传；实际调用为equals/compareTo的1或hashCode的0。
 * @param[out] result 非空调用结果槽，须新/空或旧owner已释放；只在成功后依赖其内容，失败可能尚未初始化或已改变。
 *
 * ## Returns
 * @return true表示CallValue成功完成；false也包括输入拒绝、属性未找到和调用失败，须另核threadStatus。找不到属性不会写result；CallValue通过自身参数检查/必要临时分配后才将result置NULL。
 *
 * ## Constraints and lifecycle
 * 查到属性不证明它可调用；Core own/prototype查询在internal ARRAY的活跃raw表示上可能物化并改变backing。receiver包装不独立retain/root，其余借用来源也不由本层保护。CallValue将callable/receiver/参数复制入临时VM调用栈并禁止yield，回调仍能分配、重入和改变容器，失败不清线程异常。TODO: 沿Pair/Array/Set/LinkedNode借用槽跨协议回调的调用链核来源保活、地址和遍历位置重取；不能将同步调用解释为快照、互斥或禁止重入。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4085;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3692;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3696;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3697;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3701;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3702;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1929;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1952;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2216;zr_vm_core/src/zr_vm_core/object/object.c:1992;zr_vm_core/src/zr_vm_core/object/object.c:2005;zr_vm_core/src/zr_vm_core/object/object.c:2007;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:446;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:454;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:493;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:506;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:507;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1813;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1814;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1817;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1818;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1826;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1835;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1839;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1921;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1931;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1933;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrBool zr_container_call_method(SZrState *state,
                                        SZrObject *receiver,
                                        const TZrChar *methodName,
                                        const SZrTypeValue *arguments,
                                        TZrSize argumentCount,
                                        SZrTypeValue *result) {
    const SZrTypeValue *callable;
    SZrTypeValue receiverValue;

    if (state == ZR_NULL || receiver == ZR_NULL || methodName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = zr_container_get_field_value(state, receiver, methodName);
    if (callable == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_value_set_object_fast(state, &receiverValue, receiver, zr_container_value_type_for_object(receiver));
    return ZrLib_CallValue(state, callable, &receiverValue, arguments, argumentCount, result);
}

static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value);

/**
 * ## Summary
 * 为Pair、Map/Set、Array和LinkedList的通用比较/搜索提供方向明确的相等协议；Core直接值规则先判，随后仅尝试左值对象equals(rhs)。
 *
 * ## Parameters
 * @param[in,out] state 当前有效状态，NULL时false；自定义回调可改变线程状态及容器。
 * @param[in] lhs 有效借用左值，OBJECT/ARRAY是equals的receiver；保持值/来源有效。
 * @param[in] rhs 有效借用右值，作为唯一显式参数，不被本层转移owner。
 *
 * ## Returns
 * @return Core相等或成功BOOL true为true；缺输入、不等、缺方法、非BOOL结果和回调失败均可为false。false不区分业务不等与线程错误，异常状态留在state。
 *
 * ## Constraints and lifecycle
 * 初始Core检查使用值的浅副本，不retain；同标签Core相等/直接比较命中可以在用户协议前短路。只调用lhs.equals(rhs)，不尝试反向rhs.equals(lhs)；不能据此保证自定义相等对称。Map/Set通用查找先比较hash，自定义equals的键/元素需与hashCode保持一致。TODO: 联核上层在equals后继续使用借用slot、entry、链节点及游标的保活/地址与重入修改边界，本函数没有独立root、版本快照或互斥。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4012;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5006;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5082;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5631;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5990;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6064;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7306;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3734;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3735;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3736;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3740;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3743;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3747;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4894;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5081;zr_vm_core/src/zr_vm_core/value/value.c:127;zr_vm_core/src/zr_vm_core/value/value.c:130;zr_vm_core/src/zr_vm_core/value/value.c:132;zr_vm_core/src/zr_vm_core/value/value.c:139;zr_vm_core/src/zr_vm_core/value/value.c:151;zr_vm_core/src/zr_vm_core/value/value.c:155;zr_vm_core/src/zr_vm_core/value/value.c:275;zr_vm_core/src/zr_vm_core/value/value.c:280;zr_vm_core/src/zr_vm_core/value/value.c:312;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1813;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1839;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1921;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrBool zr_container_values_equal(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue lhsCopy;
    SZrTypeValue rhsCopy;
    SZrTypeValue result;

    if (state == ZR_NULL || lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    lhsCopy = *lhs;
    rhsCopy = *rhs;
    if (ZrCore_Value_Equal(state, &lhsCopy, &rhsCopy) || ZrCore_Value_CompareDirectly(state, &lhsCopy, &rhsCopy)) {
        return ZR_TRUE;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "equals", rhs, 1, &result) && result.type == ZR_VALUE_TYPE_BOOL) {
            return (TZrBool)(result.value.nativeObject.nativeBool != 0);
        }
    }

    return ZR_FALSE;
}

/**
 * ## Summary
 * 为Set及Array整数needle搜索提供不执行用户协议的精确数值比较；混合符号只把非负signed与同值unsigned视为相等。
 *
 * ## Parameters
 * @param[in] lhs 有效整数候选借用值，NULL或非整数标签拒绝；调用方保证标签与对应64位载荷一致。
 * @param[in] rhs 有效整数needle借用值，NULL或非整数拒绝；不发生所有权转移。
 *
 * ## Returns
 * @return 两个整数按signed/unsigned规则相等为true；NULL、非整数和数值不同均为false，本接口不会自行回通用equals。
 *
 * ## Constraints and lifecycle
 * 纯标量比较，不分配、不执行guest回调，也不验证域。三处实际int needle分支把false作为未命中；存储侧非整数对象不会再调用通用values_equal。TODO: 核Set/Array快路是否有意排除对象equals(int)跨标签等价；通用比较允许lhs对象配任意rhs，但尚无容器合同要求跨标签等价的证据，因此不标BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5068;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5981;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6055;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3771;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3775;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3776;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3777;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3780;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3783;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3784;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3787;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3740;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5065;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5074;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5978;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5986;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6061;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7477;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7567;zr_vm_common/include/zr_vm_common/zr_type_conf.h:93;zr_vm_common/include/zr_vm_common/zr_type_conf.h:94;zr_vm_common/include/zr_vm_common/zr_type_conf.h:95;zr_vm_common/include/zr_vm_common/zr_type_conf.h:96;zr_vm_common/include/zr_vm_common/zr_type_conf.h:97
 */
static ZR_FORCE_INLINE TZrBool zr_container_int_values_equal_fast(const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    TZrBool lhsSigned;
    TZrBool rhsSigned;

    if (lhs == ZR_NULL || rhs == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(lhs->type) || !ZR_VALUE_IS_TYPE_INT(rhs->type)) {
        return ZR_FALSE;
    }

    lhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(lhs->type);
    rhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(rhs->type);
    if (lhsSigned && rhsSigned) {
        return (TZrBool)(lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeInt64);
    }
    if (!lhsSigned && !rhsSigned) {
        return (TZrBool)(lhs->value.nativeObject.nativeUInt64 == rhs->value.nativeObject.nativeUInt64);
    }
    if (lhsSigned) {
        return lhs->value.nativeObject.nativeInt64 >= 0 &&
               (TZrUInt64)lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeUInt64;
    }
    return rhs->value.nativeObject.nativeInt64 >= 0 &&
           lhs->value.nativeObject.nativeUInt64 == (TZrUInt64)rhs->value.nativeObject.nativeInt64;
}

/**
 * ## Summary
 * 为Set/Array的raw-int搜索限定可由signed int64表示的needle；拒绝的值仍可是合法值，只是不适合该raw表示搜索。
 *
 * ## Parameters
 * @param[in] value 有效整数候选借用值，NULL或非整数拒绝；不修改输入或转移owner。
 * @param[out] outValue 非空int64临时输出，仅成功时写入；失败后不得把旧内容当转换结果。
 *
 * ## Returns
 * @return signed整数或不超INT64_MAX的unsigned成功写入并true；其它输入false且保持outValue原内容。
 *
 * ## Constraints and lifecycle
 * 纯标量范围资格检查，不分配、调用guest或验证域/owner。三个调用只在true后读取临时值；raw-int存储本身为signed int64。返回false不会由本函数触发通用equality，调用方的raw搜索分支按未匹配处理，不代表源值非法。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5965;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6039;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3809;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3812;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3813;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3816;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3819;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5062;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5974;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6047;zr_vm_common/include/zr_vm_common/zr_type_conf.h:93;zr_vm_common/include/zr_vm_common/zr_type_conf.h:94;zr_vm_common/include/zr_vm_common/zr_type_conf.h:95;zr_vm_common/include/zr_vm_common/zr_type_conf.h:96;zr_vm_common/include/zr_vm_common/zr_type_conf.h:97
 */
static ZR_FORCE_INLINE TZrBool zr_container_try_read_int64_needle_fast(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeUInt64 > (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }
    *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Pair排序/哈希、Array整数读取与写入、下标查找交付本次int64返回值；与通用返回复制共用新结果槽前提，不承担容器字段或owner覆盖。
 *
 * ## Parameters
 * @param[out] result 可为空的输出槽；非空时须为新/空结果或旧owner已按Core覆盖协议释放的槽。
 * @param[in] value 业务层已求得的有符号整数；本接口不判断它代表下标、比较顺序还是哈希，也不额外验证这些业务范围。
 *
 * ## Returns
 * 通过非空result交付普通int值；result为NULL不写入，不返回成功状态或处理threadStatus。
 *
 * ## Constraints and lifecycle
 * 此接口只初始化返回值，不能覆盖仍持有Shared/Weak/Unique等owner的旧槽；不执行ReleaseValue，也不提供线程错误恢复。普通容器绑定回调以dispatcher新置NULL的局部result调用，回调失败与线程异常的结果发布由外层处理；不得将此前提套用到复用callable槽的raw native适配。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3974;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5664;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5677;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5709;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5968;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5974;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5982;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5991;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5997;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6209;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6263;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:73;zr_vm_core/include/zr_vm_core/value.h:78;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/include/zr_vm_core/value.h:80;zr_vm_core/include/zr_vm_core/value.h:81;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:623;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:673;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:675
 */
static ZR_FORCE_INLINE void zr_container_result_set_int_fast(SZrTypeValue *result, TZrInt64 value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    }
}

/**
 * ## Summary
 * 为相等性、存在性及增删判断交付普通bool结果；LinkedList.remove的未命中也通过同一接口返回false，业务成功与布尔结果不能混为一谈。
 *
 * ## Parameters
 * @param[out] result 可为空的输出槽；非空时须为新/空结果或旧owner已按Core覆盖协议释放的槽。
 * @param[in] value 业务布尔判断；自定义equals的返回标签检查由其调用链先完成，此处不执行用户协议。
 *
 * ## Returns
 * 通过非空result交付普通bool值；result为NULL不写入，不返回成功状态或处理threadStatus。
 *
 * ## Constraints and lifecycle
 * 此接口只初始化返回值，不能覆盖仍持有Shared/Weak/Unique等owner的旧槽；不执行ReleaseValue，也不提供线程错误恢复。普通容器绑定回调以dispatcher新置NULL的局部result调用，回调失败与线程异常的结果发布由外层处理；不得将此前提套用到复用callable槽的raw native适配。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3971;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5616;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5622;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5630;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6042;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6047;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6056;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6065;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6071;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6335;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6368;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6378;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6806;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6817;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6847;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6892;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7308;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7314;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:73;zr_vm_core/include/zr_vm_core/value.h:78;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/include/zr_vm_core/value.h:80;zr_vm_core/include/zr_vm_core/value.h:81;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:623;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:673;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:675
 */
static ZR_FORCE_INLINE void zr_container_result_set_bool_fast(SZrTypeValue *result, TZrBool value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeBool, value, ZR_VALUE_TYPE_BOOL);
    }
}

/**
 * ## Summary
 * 为通用返回复制的UINT64分支交付完整无符号结果，使标量返回遵守同一结果槽初始化契约；本函数没有独立业务回调入口。
 *
 * ## Parameters
 * @param[out] result 可为空的输出槽；非空时须为新/空结果或旧owner已按Core覆盖协议释放的槽。
 * @param[in] value 源UINT64的数值；不转为有符号范围或容器容量。
 *
 * ## Returns
 * 通过非空result交付普通uint值；result为NULL不写入，不返回成功状态或处理threadStatus。
 *
 * ## Constraints and lifecycle
 * 此接口只初始化返回值，不能覆盖仍持有Shared/Weak/Unique等owner的旧槽；不执行ReleaseValue，也不提供线程错误恢复。普通容器绑定回调以dispatcher新置NULL的局部result调用，回调失败与线程异常的结果发布由外层处理；不得将此前提套用到复用callable槽的raw native适配。 当前唯一直接调用位于result_copy_no_profile，其上层为绑定回调；新增独立入口须维持此结果槽契约。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3977;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:73;zr_vm_core/include/zr_vm_core/value.h:78;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/include/zr_vm_core/value.h:80;zr_vm_core/include/zr_vm_core/value.h:81;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:623;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:673;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:675
 */
static ZR_FORCE_INLINE void zr_container_result_set_uint_fast(SZrTypeValue *result, TZrUInt64 value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeUInt64, value, ZR_VALUE_TYPE_UINT64);
    }
}

/**
 * ## Summary
 * 为通用返回复制的DOUBLE分支交付普通浮点结果，与整数/布尔返回采用同一结果槽前提；本函数没有独立业务回调入口。
 *
 * ## Parameters
 * @param[out] result 可为空的输出槽；非空时须为新/空结果或旧owner已按Core覆盖协议释放的槽。
 * @param[in] value 源DOUBLE值；不执行排序、相等性或数值范围验证。
 *
 * ## Returns
 * 通过非空result交付普通double值；result为NULL不写入，不返回成功状态或处理threadStatus。
 *
 * ## Constraints and lifecycle
 * 此接口只初始化返回值，不能覆盖仍持有Shared/Weak/Unique等owner的旧槽；不执行ReleaseValue，也不提供线程错误恢复。普通容器绑定回调以dispatcher新置NULL的局部result调用，回调失败与线程异常的结果发布由外层处理；不得将此前提套用到复用callable槽的raw native适配。 当前唯一直接调用位于result_copy_no_profile，其上层为绑定回调；新增独立入口须维持此结果槽契约。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3980;zr_vm_core/include/zr_vm_core/value.h:72;zr_vm_core/include/zr_vm_core/value.h:73;zr_vm_core/include/zr_vm_core/value.h:78;zr_vm_core/include/zr_vm_core/value.h:79;zr_vm_core/include/zr_vm_core/value.h:80;zr_vm_core/include/zr_vm_core/value.h:81;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:623;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:673;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:675
 */
static ZR_FORCE_INLINE void zr_container_result_set_double_fast(SZrTypeValue *result, TZrDouble value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeDouble, value, ZR_VALUE_TYPE_DOUBLE);
    }
}

/**
 * ## Summary
 * 为越界读取、空容器、无值更新和内建Array迭代适配交付空结果；调用者必须区分绑定回调的局部返回槽与raw native的callable复用槽。
 *
 * ## Parameters
 * @param[out] result 可为空的输出槽；非空时须为新/空结果或旧owner已释放的槽，本函数不承担旧owner释放。
 *
 * ## Returns
 * 通过非空result交付普通NULL；空指针不写入，不返回成功状态，也不改变线程错误。
 *
 * ## Constraints and lifecycle
 * 此接口只初始化返回值，不能覆盖仍持有Shared/Weak/Unique等owner的旧槽；不执行ReleaseValue，也不提供线程错误恢复。普通容器绑定回调以dispatcher新置NULL的局部result调用，回调失败与线程异常的结果发布由外层处理；不得将此前提套用到复用callable槽的raw native适配。 raw Array getIterator桥接以functionBase的callable槽作为resultValue，并非dispatcher局部NULL结果。BUG: 同域公开API可取基本ARRAY原型的permanent raw native getIterator闭包，用SharePlainValue建立Shared callable，并经CallValue传非ARRAY receiver。CallValue把callable复制到scratch functionBase时增加strong；raw桥在错误receiver成功返回分支以本接口将该槽清NULL而未Release，top=base+1且返回1。Core native返回source==target跳过覆盖复制，CallValue只复制NULL至外部result并恢复top；宿主正常释放原Shared后，scratch那份strong/control仍未配对。只证明此错误receiver分支的引用计数遗漏；闭包原本permanent，不能说它因本次错误才不可回收，也不推广到其它setter或所有桥接路径。TODO: raw native复用callable槽发布结果前沿Core覆盖协议释放旧owner，并继续独立核同桥的成功对象发布/分配失败分支；此处未授guest或运行复现信用。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:3962;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3968;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5809;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5851;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5889;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5927;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6146;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6160;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6201;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6206;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6416;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6497;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6930;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7224;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7264;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7358;zr_vm_core/include/zr_vm_core/value.h:90;zr_vm_core/include/zr_vm_core/value.h:92;zr_vm_core/include/zr_vm_core/value.h:93;zr_vm_core/include/zr_vm_core/value.h:94;zr_vm_core/include/zr_vm_core/value.h:95;zr_vm_core/include/zr_vm_core/value.h:96;zr_vm_core/include/zr_vm_core/value.h:97;zr_vm_core/include/zr_vm_core/value.h:98;zr_vm_core/include/zr_vm_core/value.h:99;zr_vm_core/include/zr_vm_core/value.h:147;zr_vm_core/include/zr_vm_core/value.h:156;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6127;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6140;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6141;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6143;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6144;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6148;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6149;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7826;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7853;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7854;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7855;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7856;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7857;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7918;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7926;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1813;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1814;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1815;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1839;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1921;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1931;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1933;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1934;zr_vm_core/src/zr_vm_core/runtime/stack.c:248;zr_vm_core/src/zr_vm_core/runtime/stack.c:251;zr_vm_core/src/zr_vm_core/function/function.c:3781;zr_vm_core/src/zr_vm_core/function/function.c:3782;zr_vm_core/src/zr_vm_core/function/function.c:3803;zr_vm_core/src/zr_vm_core/function/function.c:3818;zr_vm_core/src/zr_vm_core/ownership/ownership.c:20;zr_vm_core/src/zr_vm_core/ownership/ownership.c:22;zr_vm_core/src/zr_vm_core/ownership/ownership.c:41;zr_vm_core/src/zr_vm_core/ownership/ownership.c:49;zr_vm_core/src/zr_vm_core/ownership/ownership.c:52;zr_vm_core/src/zr_vm_core/ownership/ownership.c:137;zr_vm_core/src/zr_vm_core/ownership/ownership.c:148;zr_vm_core/src/zr_vm_core/ownership/ownership.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:481;zr_vm_core/src/zr_vm_core/ownership/ownership.c:488;zr_vm_core/src/zr_vm_core/ownership/ownership.c:494;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:738;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/closure/closure.c:239;zr_vm_core/src/zr_vm_core/closure/closure.c:243;zr_vm_core/src/zr_vm_core/closure/closure.c:245;zr_vm_core/src/zr_vm_core/closure/closure.c:247;zr_vm_core/src/zr_vm_core/closure/closure.c:248;zr_vm_core/src/zr_vm_core/closure/closure.c:263;zr_vm_core/src/zr_vm_core/closure/closure.c:885;zr_vm_core/src/zr_vm_core/closure/closure.c:912;zr_vm_core/src/zr_vm_core/closure/closure.c:919;zr_vm_core/src/zr_vm_core/closure/closure.c:922;zr_vm_core/src/zr_vm_core/function/function.c:3295;zr_vm_core/src/zr_vm_core/function/function.c:3322;zr_vm_core/src/zr_vm_core/function/function.c:3787;zr_vm_library/include/zr_vm_library/native_binding.h:671;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1945;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:81;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:92;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:139;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:152;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:164;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:702;zr_vm_core/src/zr_vm_core/gc/gc.c:706;zr_vm_core/src/zr_vm_core/gc/gc.c:711;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1429;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1446;zr_vm_library/include/zr_vm_library/native_binding.h:642;zr_vm_core/include/zr_vm_core/ownership.h:72
 */
static ZR_FORCE_INLINE void zr_container_result_set_null_fast(SZrTypeValue *result) {
    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(result);
    }
}

/**
 * ## Summary
 * 为Array/Map读写及LinkedList摘链返回交付可独立使用的值：标量走结果初始化契约，其他值沿Core复制/ownership协议；返回发布不构成容器修改事务。
 *
 * ## Parameters
 * @param[in,out] state 当前有效VM状态；非标量复制可分配、设置错误或执行Core ownership动作，实际七个调用均传有效状态。
 * @param[in,out] result 输出槽；NULL返回false；NULL/标量快路要求目标新/空或旧owner已释放，其余类型沿Core覆盖协议。
 * @param[in] value 借用源值，可为NULL指针而表示空结果；保持有效至复制完成，不在本接口直接转移源owner。
 *
 * ## Returns
 * @return 缺result为false；源NULL指针或NULL/BOOL/INT64/UINT64/DOUBLE写入后为true。其余类型完成Core copy后按threadStatus返回；代码中的state==NULL返回true不能解释为无状态对象复制安全契约，实际入口要求有效state。
 *
 * ## Constraints and lifecycle
 * 七个直接调用属于普通绑定回调，dispatcher预置局部result为NULL；这只证明当前标量快路目标可初始化，不许可新增owned旧槽调用。其他类型的复制、域资格及STRUCT克隆由Core Value_CopyNoProfile/Ownership_AssignValue决定；不能把此接口描述为总是浅拷贝或容器快照。复制异常可留下局部结果与已经发生的容器修改，false不提供回滚；外层dispatcher按错误状态不发布结果。LinkedList remove-first/last先复制成功再摘链，Array/Map set可能已写backing。TODO: 继续联核Core普通OBJECT内STRUCT的复制分支和容器plain快路，明确哪些入口必须保持STRUCT值语义；本返回接口不自行建立新的复制模式。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6214;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6502;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6582;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6664;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7229;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7269;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3958;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3966;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3986;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3987;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7272;zr_vm_core/include/zr_vm_core/value.h:147;zr_vm_core/include/zr_vm_core/value.h:156;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_core/include/zr_vm_core/value.h:232;zr_vm_core/include/zr_vm_core/value.h:236;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:673;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1839;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1931;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1933;zr_vm_core/src/zr_vm_core/value/value.c:238;zr_vm_core/src/zr_vm_core/value/value.c:239;zr_vm_core/src/zr_vm_core/ownership/ownership.c:774;zr_vm_core/src/zr_vm_core/ownership/ownership.c:782;zr_vm_core/src/zr_vm_core/ownership/ownership.c:786;zr_vm_core/src/zr_vm_core/ownership/ownership.c:792;zr_vm_core/src/zr_vm_core/ownership/ownership.c:801;zr_vm_core/src/zr_vm_core/ownership/ownership.c:802;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808
 */
static ZR_FORCE_INLINE TZrBool zr_container_result_copy_no_profile(SZrState *state,
                                                                    SZrTypeValue *result,
                                                                    const SZrTypeValue *value) {
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            zr_container_result_set_null_fast(result);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_BOOL:
            zr_container_result_set_bool_fast(result, value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT64:
            zr_container_result_set_int_fast(result, value->value.nativeObject.nativeInt64);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT64:
            zr_container_result_set_uint_fast(result, value->value.nativeObject.nativeUInt64);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_DOUBLE:
            zr_container_result_set_double_fast(result, value->value.nativeObject.nativeDouble);
            return ZR_TRUE;
        default:
            break;
    }

    ZrCore_Value_CopyNoProfile(state, result, value);
    return state == ZR_NULL || state->threadStatus == ZR_THREAD_STATUS_FINE;
}

/* Pair.compareTo 先比较可解释的字符串、数值及对象协议；无协议时的哈希/类型
 * 顺序只是容器的兜底排序，调用方不能把它当成跨运行实例的稳定序列化顺序。
 */
/** @brief 先判相等，再处理字符串和数字、对象 compareTo，最后以哈希/类型兜底；数字分支存在下方已记录的精度和 unsigned 排序缺陷。 */
static TZrInt64 zr_container_values_compare(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue result;
    const TZrChar *lhsText;
    const TZrChar *rhsText;
    TZrDouble lhsNumber;
    TZrDouble rhsNumber;
    TZrUInt64 lhsHash;
    TZrUInt64 rhsHash;

    if (lhs == ZR_NULL && rhs == ZR_NULL) {
        return 0;
    }
    if (lhs == ZR_NULL) {
        return -1;
    }
    if (rhs == ZR_NULL) {
        return 1;
    }
    if (zr_container_values_equal(state, lhs, rhs)) {
        return 0;
    }

    if (lhs->type == ZR_VALUE_TYPE_STRING && rhs->type == ZR_VALUE_TYPE_STRING) {
        lhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, lhs->value.object));
        rhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, rhs->value.object));
        if (lhsText == ZR_NULL) {
            return rhsText == ZR_NULL ? 0 : -1;
        }
        if (rhsText == ZR_NULL) {
            return 1;
        }
        return strcmp(lhsText, rhsText);
    }

    /* BUG: UInt64 经 nativeInt64 解释后大于 INT64_MAX 的值会变成负数；
     * 64 位整数再转 double 还会使 2^53 与 2^53+1 失去顺序，甚至双向比较均返回 1。
     * Pair.compareTo 通过 kPairMetaMethods 可达；需补齐有符号/无符号边界测试。
     */
    if ((ZR_VALUE_IS_TYPE_INT(lhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(lhs->type) || ZR_VALUE_IS_TYPE_FLOAT(lhs->type)) &&
        (ZR_VALUE_IS_TYPE_INT(rhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rhs->type) || ZR_VALUE_IS_TYPE_FLOAT(rhs->type))) {
        lhsNumber = ZR_VALUE_IS_TYPE_FLOAT(lhs->type) ? lhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)lhs->value.nativeObject.nativeInt64;
        rhsNumber = ZR_VALUE_IS_TYPE_FLOAT(rhs->type) ? rhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)rhs->value.nativeObject.nativeInt64;
        return lhsNumber < rhsNumber ? -1 : 1;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "compareTo", rhs, 1, &result) &&
            (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type))) {
            return ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)
                           ? result.value.nativeObject.nativeInt64
                           : (TZrInt64)result.value.nativeObject.nativeUInt64;
        }
    }

    lhsHash = zr_container_value_hash(state, lhs);
    rhsHash = zr_container_value_hash(state, rhs);
    if (lhsHash == rhsHash) {
        return (TZrInt64)lhs->type - (TZrInt64)rhs->type;
    }
    return lhsHash < rhsHash ? -1 : 1;
}

/**
 * ## Summary
 * 统一Map/Set键筛选、Pair.hashCode及比较兜底的哈希协议；对象整数hashCode可以覆盖Core值哈希，结果不保证跨运行稳定。
 *
 * ## Parameters
 * @param[in,out] state 当前有效状态，NULL时返回0；用户hashCode可分配、重入或设置错误。
 * @param[in] value 当前域合法借用值，NULL返回0；非空来源槽在整个协议/后备哈希期间须保持有效。
 *
 * ## Returns
 * @return OBJECT/ARRAY的hashCode成功返回signed/unsigned整数时转为UInt64；否则用Core值哈希。0不是成功或失败标识，方法失败后仍可给后备哈希但不清threadStatus。
 *
 * ## Constraints and lifecycle
 * 仅OBJECT/ARRAY非空payload尝试用户协议；signed结果按C无符号转换映射。Map/Set通用路径先hash后equals，调用者需保持自定义hashCode与相等关系兼容、键在查找期间稳定；Core fallback不再次执行对象方法。TODO: hashCode后非整数或失败回退会再次读取原借用value，联核Pair/backing/节点槽在回调修改或GC后的保活及地址稳定；本函数未给它独立root、retain、快照或锁。缺完整合法释放/移动证明，不标新BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4883;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4894;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4994;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5005;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5078;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5707;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5708;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4079;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4083;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4085;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4086;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4087;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4089;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4090;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4095;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5006;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5082;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7601;zr_vm_core/src/zr_vm_core/value/value.c:242;zr_vm_core/src/zr_vm_core/value/value.c:246;zr_vm_core/src/zr_vm_core/value/value.c:249;zr_vm_core/src/zr_vm_core/value/value.c:252;zr_vm_core/src/zr_vm_core/value/value.c:254;zr_vm_core/src/zr_vm_core/value/value.c:256;zr_vm_core/src/zr_vm_core/value/value.c:263;zr_vm_core/src/zr_vm_core/value/value.c:269;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1839;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1921;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value) {
    SZrTypeValue result;

    if (state == ZR_NULL || value == ZR_NULL) {
        return 0;
    }

    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, value->value.object);
        if (zr_container_call_method(state, receiver, "hashCode", ZR_NULL, 0, &result)) {
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
                return (TZrUInt64)result.value.nativeObject.nativeInt64;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
                return result.value.nativeObject.nativeUInt64;
            }
        }
    }

    return ZrCore_Value_GetHash(state, value);
}

/**
 * ## Summary
 * 为追加和移位提供按整数键写入的Core边界；存储复制由Core负责，公开容器元数据由外层维护。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] index 可表示为int64的backing整数键；本层不验证公开长度或连续性。
 * @param[in] value 调用期间有效且与backing写入域兼容的非NULL借用值。
 *
 * ## Returns
 * @return 空参数时false；调用setter后仅以threadStatus是否FINE为结果。true不单独证明字段更新，false不证明未产生副作用。
 *
 * ## Constraints and lifecycle
 * Core setter为void，可物化raw表示并执行值复制、owner处理和屏障；本入口不建立值副本或事务，不更新包装对象length/count。TODO: 沿Object_SetValue拒绝写入而未设置线程错误的路径核实调用方是否需要独立更新结果；不能把线程状态当作事务提交证明。 调用方须保证backing长度、整数键及length+1可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4438;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4442;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4485;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4124;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881;zr_vm_core/src/zr_vm_core/object/object.c:1730;zr_vm_core/src/zr_vm_core/object/object.c:1810;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1829;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/object/object.c:1925;zr_vm_core/src/zr_vm_core/object/object.c:1950
 */
static TZrBool zr_container_storage_set(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

/**
 * ## Summary
 * 为标量追加保留raw-int表示，避免为整数建立Pair节点；供通用追加路径选择可用快路。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] value 已解码的int64标量，无GC载荷或owner。
 *
 * ## Returns
 * @return 表示不适用、已有非空节点阻止raw初始化或raw容量申请失败时false；完成追加时true，不显式检查线程状态。
 *
 * ## Constraints and lifecycle
 * 调用方必须保证array地址与raw缓冲区在调用期间有效。容量由RawMalloc处理，不执行本路径的GC重试；成功把backing切换/维持为RAW_CANONICAL，并更新raw长度、generation和dirty；不更新memberVersion或包装length。storage_push在false后仍尝试Core setter，故本结果只描述快路命中，不能说明整个追加已失败。 调用方须保证backing长度、整数键及length+1可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4157;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4166;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4176;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:238;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:269;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:290;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:306;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2107;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2115;zr_vm_core/include/zr_vm_core/memory.h:30;zr_vm_core/include/zr_vm_core/memory.h:36
 */
static TZrBool zr_container_storage_push_raw_int_fast(SZrState *state, SZrObject *array, TZrInt64 value) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    if (array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL) {
        return ZR_FALSE;
    }
    if (zr_container_array_raw_int_active(array)) {
        length = array->superArrayRawIntLength;
    } else {
        length = 0;
        if (array->nodeMap.elementCount != 0) {
            return ZR_FALSE;
        }
    }

    if (!ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, array, length + 1) ||
        array->superArrayRawIntData == ZR_NULL) {
        return ZR_FALSE;
    }

    array->superArrayRawIntData[length] = value;
    array->superArrayRawIntLength = length + 1;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
    array->superArrayStorageGeneration++;
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Array.add提供GC值的稠密节点追加边界，以预留Pair pool承接整数索引存储。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] value 非NULL、GC-collectable且满足本域写入契约的借用值；raw-canonical backing不适用。
 *
 * ## Returns
 * @return 输入或存储形状不适用、容量准备失败、目标桶非空或无预留Pair时false；发布值与屏障后true，不显式读取线程状态。内存异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 仅Array.add尝试本快路；正常native派发使收到的self/参数保持为GC可见值；本helper未为从self读取的backing/nodeMap裸指针单独建立root或pin，也不保证其在可能GC的pool扩容期间地址稳定；直接C调用亦无此保证。复制遵守Core ownership；backing与Pair借用不得跨域、重入或GC后盲用。普通false可能已经扩过桶/池，外层仍尝试storage_push；true之后包装length写失败也不回滚已追加项。TODO: 核对void copy/barrier错误、表示发布与外层长度更新的统一失败契约，以及pool扩容时backing地址稳定性和容量上界。 调用方须保证backing长度、整数键及length+1可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4197;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4204;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4211;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4216;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4217;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4224;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4231;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4233;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4234;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4235;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4236;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4237;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4238;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4239;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7467;zr_vm_core/include/zr_vm_core/hash_set.h:136;zr_vm_core/include/zr_vm_core/hash_set.h:151;zr_vm_core/include/zr_vm_core/hash_set.h:152;zr_vm_core/include/zr_vm_core/hash_set.h:153;zr_vm_core/include/zr_vm_core/hash_set.h:362;zr_vm_core/src/zr_vm_core/base/hash_set.c:102;zr_vm_core/src/zr_vm_core/base/hash_set.c:114;zr_vm_core/src/zr_vm_core/base/hash_set.c:116;zr_vm_core/src/zr_vm_core/base/hash_set.c:119;zr_vm_core/src/zr_vm_core/base/hash_set.c:124;zr_vm_core/src/zr_vm_core/base/hash_set.c:133;zr_vm_core/src/zr_vm_core/base/hash_set.c:141;zr_vm_core/include/zr_vm_core/value.h:200;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_core/include/zr_vm_core/value.h:232;zr_vm_core/include/zr_vm_core/value.h:236;zr_vm_core/src/zr_vm_core/value/value.c:36;zr_vm_core/src/zr_vm_core/value/value.c:40;zr_vm_core/src/zr_vm_core/value/value.c:43;zr_vm_core/src/zr_vm_core/value/value.c:238;zr_vm_core/src/zr_vm_core/value/value.c:239;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:167;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:194;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:202;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:211;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:225;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:231;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:234;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:239;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_storage_push_gc_value_dense_pair_pool_fast(SZrState *state,
                                                                       SZrObject *array,
                                                                       const SZrTypeValue *value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL ||
        !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_FALSE;
    }

    nodeMap = &array->nodeMap;
    if (!nodeMap->isValid || nodeMap->buckets == ZR_NULL || nodeMap->capacity == 0) {
        return ZR_FALSE;
    }

    index = nodeMap->elementCount;
    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, nodeMap, index + 1) ||
        !ZrCore_HashSet_EnsurePairPoolForElementCount(state, nodeMap, nodeMap->pairPoolUsed + 1) ||
        index >= nodeMap->capacity ||
        nodeMap->buckets[index] != ZR_NULL) {
        return ZR_FALSE;
    }

    pair = ZrCore_HashSet_TakeReservedPair(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, (TZrInt64)index, ZR_VALUE_TYPE_INT64);
    ZrCore_Value_ResetAsNullNoProfile(&pair->value);
    ZrCore_Value_CopyNoProfile(state, &pair->value, value);
    nodeMap->buckets[index] = pair;
    nodeMap->elementCount++;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL;
    array->superArrayStorageGeneration++;
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(array), &pair->value);
    array->memberVersion++;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 把Array、Map与Set的新条目接入backing末尾，并为有符号整数选择raw追加快路。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] value 非NULL同域借用值；Map的新Pair和Set元素也经此写入。
 *
 * ## Returns
 * @return raw快路成功时true且不检查线程状态；其余情况返回storage_set的调用后线程状态结果。false无回滚承诺。
 *
 * ## Constraints and lifecycle
 * 本入口使用backing计数决定末尾键，不维护包装length/count或容量策略。raw快路未命中时可进入Core setter并物化表示、分配或复制owner；调用者在成功后仍须独立维护公开元数据。参数借用须覆盖完整调用，返回不构成值快照或owner转移。 调用方须保证backing长度、整数键及length+1可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4260;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4261;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4265;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6650;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7467;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7568;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4124;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4438;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4442;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4485;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881;zr_vm_core/src/zr_vm_core/object/object.c:1730;zr_vm_core/src/zr_vm_core/object/object.c:1810;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1829;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/object/object.c:1925;zr_vm_core/src/zr_vm_core/object/object.c:1950;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4157;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4166;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4176;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:238;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:269;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:290;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:306;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2107;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2115;zr_vm_core/include/zr_vm_core/memory.h:30;zr_vm_core/include/zr_vm_core/memory.h:36
 */
static TZrBool zr_container_storage_push(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) &&
        zr_container_storage_push_raw_int_fast(state, array, value->value.nativeObject.nativeInt64)) {
        return ZR_TRUE;
    }

    return zr_container_storage_set(state, array, zr_container_array_length_fast(array), value);
}

/** @brief raw 分支缩短长度并递增 generation，通用分支只删除末尾整数键节点、不更新数组版本；返回的 key 是普通整数，本层未接管其他 ownership。 */
static TZrBool zr_container_storage_remove_last(SZrState *state, SZrObject *array) {
    SZrTypeValue key;
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (length == 0) {
        return ZR_FALSE;
    }

    if (array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL) {
        array->superArrayRawIntLength = length - 1;
        array->superArrayStorageGeneration++;
        array->superArrayRawIntDirty = ZR_FALSE;
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)(length - 1));
    ZrCore_HashSet_Remove(state, &array->nodeMap, &key);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为canonical raw-int backing提供不涉及值owner的下标删除，供通用删除入口选择标量快路。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] index 小于当前raw长度的有效下标。
 *
 * ## Returns
 * @return 表示不适用或下标越界时在元素修改前false；完成标量删除时true，不检查线程状态。
 *
 * ## Constraints and lifecycle
 * 只涉及int64原生缓冲区，不分配、不复制或释放GC/owner值；调用者仍须保持缓冲区地址有效。成功更新raw长度、generation和dirty，不更新memberVersion或包装length；generation不能作为迭代器快照有效性证明。generic Shared删除的owner问题不适用于此标量分支。 调用方须保证backing长度及整数键可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4476;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4318;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4322;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4326;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4330;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4337;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4339;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4157;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4166;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4176;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:238;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:269;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:290;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:306;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2107;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2115;zr_vm_core/include/zr_vm_core/memory.h:30;zr_vm_core/include/zr_vm_core/memory.h:36
 */
static TZrBool zr_container_storage_remove_at_raw_int_fast(SZrState *state, SZrObject *array, TZrSize index) {
    TZrSize length;

    if (state == ZR_NULL || !zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (index >= length) {
        return ZR_FALSE;
    }

    if (index + 1 < length) {
        TZrSize movedCount = length - index - 1;
        memmove(array->superArrayRawIntData + index,
                array->superArrayRawIntData + index + 1,
                movedCount * sizeof(array->superArrayRawIntData[0]));
    }
    array->superArrayRawIntLength = length - 1;
    array->superArrayStorageGeneration++;
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为raw-int backing提供保持标量表示的插入，供Array.insert优先尝试。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] index 允许0到raw长度的插入位置。
 * @param[in] value 已解码的int64标量。
 *
 * ## Returns
 * @return 表示不适用、位置越界或raw容量准备失败时false；完成插入时true，不检查线程状态。
 *
 * ## Constraints and lifecycle
 * 容量拒绝发生在元素移动前，但上层storage_insert随后仍可物化generic并尝试写入。成功只维护raw长度、generation和dirty，不更新memberVersion/包装length，也不处理owner；调用者保证缓冲区借用和长度上界。单次快路前置失败不等价于外层调用无副作用。 调用方须保证backing长度、整数键及length+1可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4424;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4361;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4365;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4371;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4374;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4378;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4383;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4386;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4388;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7470;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4147;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4157;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4166;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4173;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4176;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4266;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:238;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:269;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:290;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:306;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2107;zr_vm_core/src/zr_vm_core/object/object_super_array.c:2115;zr_vm_core/include/zr_vm_core/memory.h:30;zr_vm_core/include/zr_vm_core/memory.h:36
 */
static TZrBool zr_container_storage_insert_raw_int_fast(SZrState *state,
                                                        SZrObject *array,
                                                        TZrSize index,
                                                        TZrInt64 value) {
    TZrSize length;

    if (state == ZR_NULL || !zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (index > length ||
        !ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, array, length + 1)) {
        return ZR_FALSE;
    }

    if (index < length) {
        TZrSize movedCount = length - index;
        memmove(array->superArrayRawIntData + index + 1,
                array->superArrayRawIntData + index,
                movedCount * sizeof(array->superArrayRawIntData[0]));
    }
    array->superArrayRawIntData[index] = value;
    array->superArrayRawIntLength = length + 1;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
    array->superArrayStorageGeneration++;
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Array.insert提供backing插入；按值形状选择raw路径，否则承接Core节点表示的移位与写入。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] index 不大于backing长度的插入位置，且整数键须可表示为int64。
 * @param[in] value 非NULL同域借用值，须在整个插入期间保持有效。
 *
 * ## Returns
 * @return 参数、位置或generic物化失败时false；raw快路成功时true；generic结果仅取最后一次storage_set，未逐次确认移位写入。
 *
 * ## Constraints and lifecycle
 * 公开length由Array.insert在本入口返回true后更新。本层无快照、批量事务或回滚；generic分支将借用source槽逐项交给Core，缺失source可跳过写入，已发生的表示或元素变化可能保留。TODO: 保留函数体的失败注入待核，检查中途setter拒绝/线程错误与最终布尔、公开length的关系；沿STRUCT复制重入和分配路径核对借用槽与backing的生存边界。 调用方须保证backing长度、整数键及length+1可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4410;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4415;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4419;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4424;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4427;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4432;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4435;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4436;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4438;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4442;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5840;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5849;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7470;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3436;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4124;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4485;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5802;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881;zr_vm_core/src/zr_vm_core/object/object.c:1730;zr_vm_core/src/zr_vm_core/object/object.c:1810;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1829;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1867;zr_vm_core/src/zr_vm_core/object/object.c:1925;zr_vm_core/src/zr_vm_core/object/object.c:1950;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193;zr_vm_core/src/zr_vm_core/ownership/ownership.c:206;zr_vm_core/src/zr_vm_core/ownership/ownership.c:207;zr_vm_core/src/zr_vm_core/object/object.c:1613
 */
static TZrBool zr_container_storage_insert(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (index > length) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) &&
            zr_container_storage_insert_raw_int_fast(state, array, index, value->value.nativeObject.nativeInt64)) {
            return ZR_TRUE;
        }
        if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
            return ZR_FALSE;
        }
    }

    /* TODO: 后移期间忽略 storage_set 的失败结果；需以注入式写入失败验证
     * Array.insert 是否会留下部分移动的 backing，并明确失败后的状态契约。
     */
    for (TZrSize cursor = length; cursor > index; cursor--) {
        const SZrTypeValue *source = zr_container_array_get_value_fast(state, array, cursor - 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_set(state, array, index, value);
}

/**
 * ## Summary
 * 为Array/Map/Set的下标删除提供backing边界；raw分支删标量，generic分支承接节点移位与末项移除。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域的有效状态，调用开始时由上层保证线程与域资格。
 * @param[in,out] array 存活、已初始化的internal ARRAY backing；调用者维持有效表示和连续整数键约束。
 * @param[in] index 小于backing当前长度的有效下标，整数键须可表示为int64。
 *
 * ## Returns
 * @return 参数或位置拒绝时false；raw分支返回其结果；generic分支仅返回storage_remove_last结果，未以所有移位写入成功为条件。
 *
 * ## Constraints and lifecycle
 * 包装length/count由外层在helper成功后维护；generic借用source逐项写入，不提供快照/回滚，末项删除不递增memberVersion。TODO: 保留函数体的移位失败传播待核，检查STRUCT复制/GC后的借用与局部修改边界。BUG: 同域公开SharePlainValue可把普通已初始化GC ARRAY建为Shared，值仍是GC-collectable；注册Array<T>的T无附加约束，公开CallValue调用Array.add，在backing非RAW、稠密桶/PairPool准备成功时可经快路复制它并增strong；NONE或NODE表示均可进入。随后单元素Array.removeAt(0)没有移位，NODE表示进入remove_last/HashSet_Remove，只摘桶链并返回普通整数key，未Release pair.value，pool使用计数也不回退。释放宿主Shared和调用临时副本后，此脱链槽的strong仍未配对；正常pool/Object解构仅释放原生存储，不补偿槽owner，普通Shared引用对象的ignored根仍保留。结论限VM存活期间的合法同域C API静态链；raw-int、guest路径、运行复现或UAF均未由此证明。TODO: 明确generic删除对value owner的释放职责及与pool复用、解构的配对协议。 调用方须保证backing长度及整数键可表示为非负int64/目标TZrSize；本层不统一拒绝所有极值或窄化。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6367;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4463;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4468;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4472;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4476;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4479;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4482;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4483;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4485;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4489;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4295;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4296;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4297;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7467;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7473;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7512;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7574;zr_vm_core/include/zr_vm_core/hash_set.h:566;zr_vm_core/include/zr_vm_core/hash_set.h:578;zr_vm_core/include/zr_vm_core/hash_set.h:580;zr_vm_core/include/zr_vm_core/hash_set.h:584;zr_vm_core/include/zr_vm_core/hash_set.h:586;zr_vm_core/include/zr_vm_core/hash_set.h:587;zr_vm_core/include/zr_vm_core/hash_set.h:588;zr_vm_core/include/zr_vm_core/hash_set.h:591;zr_vm_core/src/zr_vm_core/base/hash_set.c:39;zr_vm_core/src/zr_vm_core/base/hash_set.c:49;zr_vm_core/src/zr_vm_core/base/hash_set.c:64;zr_vm_core/src/zr_vm_core/base/hash_set.c:71;zr_vm_core/src/zr_vm_core/base/hash_set.c:74;zr_vm_core/src/zr_vm_core/base/hash_set.c:79;zr_vm_core/src/zr_vm_core/base/hash_set.c:89;zr_vm_core/include/zr_vm_core/ownership.h:72;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:481;zr_vm_core/src/zr_vm_core/ownership/ownership.c:488;zr_vm_core/src/zr_vm_core/ownership/ownership.c:494;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:736;zr_vm_core/src/zr_vm_core/ownership/ownership.c:755;zr_vm_core/src/zr_vm_core/ownership/ownership.c:801;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:81;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:89;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:106;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:139;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:152;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:161;zr_vm_core/include/zr_vm_core/value.h:200;zr_vm_core/include/zr_vm_core/value.h:225;zr_vm_core/include/zr_vm_core/value.h:236;zr_vm_core/src/zr_vm_core/value/value.c:238;zr_vm_core/src/zr_vm_core/value/value.c:239;zr_vm_library/include/zr_vm_library/native_binding.h:671;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4197;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4204;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4211;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4216;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4217;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4224;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4231;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4233;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4234;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4235;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4236;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4237;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4238;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4239;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5805;zr_vm_core/include/zr_vm_core/hash_set.h:136;zr_vm_core/include/zr_vm_core/hash_set.h:151;zr_vm_core/include/zr_vm_core/hash_set.h:152;zr_vm_core/include/zr_vm_core/hash_set.h:153;zr_vm_core/include/zr_vm_core/hash_set.h:362;zr_vm_core/src/zr_vm_core/base/hash_set.c:102;zr_vm_core/src/zr_vm_core/base/hash_set.c:114;zr_vm_core/src/zr_vm_core/base/hash_set.c:116;zr_vm_core/src/zr_vm_core/base/hash_set.c:119;zr_vm_core/src/zr_vm_core/base/hash_set.c:124;zr_vm_core/src/zr_vm_core/base/hash_set.c:133;zr_vm_core/src/zr_vm_core/base/hash_set.c:141;zr_vm_core/include/zr_vm_core/value.h:229;zr_vm_core/include/zr_vm_core/value.h:232;zr_vm_core/src/zr_vm_core/value/value.c:36;zr_vm_core/src/zr_vm_core/value/value.c:40;zr_vm_core/src/zr_vm_core/value/value.c:43;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:167;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:194;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:202;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:211;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:225;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:231;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:234;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:239;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1016;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1036;zr_vm_core/src/zr_vm_core/ownership/ownership.c:20;zr_vm_core/src/zr_vm_core/ownership/ownership.c:41;zr_vm_core/src/zr_vm_core/ownership/ownership.c:49;zr_vm_core/src/zr_vm_core/ownership/ownership.c:51;zr_vm_core/src/zr_vm_core/ownership/ownership.c:93;zr_vm_core/src/zr_vm_core/ownership/ownership.c:100;zr_vm_core/src/zr_vm_core/ownership/ownership.c:137;zr_vm_core/src/zr_vm_core/ownership/ownership.c:148;zr_vm_core/src/zr_vm_core/ownership/ownership.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership.c:156;zr_vm_core/src/zr_vm_core/ownership/ownership.c:171;zr_vm_core/src/zr_vm_core/ownership/ownership.c:747;zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c:58;zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c:71;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:210;zr_vm_core/src/zr_vm_core/ownership/ownership_shared.c:232;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:1529;zr_vm_core/src/zr_vm_core/object/object.c:1545;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:448;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:456;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:464;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:468;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:690;zr_vm_core/src/zr_vm_core/gc/gc.c:704;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1356;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1365;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1864;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1921;zr_vm_library/include/zr_vm_library/native_binding.h:635;zr_vm_library/include/zr_vm_library/native_binding.h:676;zr_vm_core/include/zr_vm_core/value.h:311;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7396;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7397;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5872;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5877;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5886
 */
static TZrBool zr_container_storage_remove_at(SZrState *state, SZrObject *array, TZrSize index) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (index >= length) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        return zr_container_storage_remove_at_raw_int_fast(state, array, index);
    }

    /* TODO: 前移期间同样忽略 storage_set 的失败结果；需检查 Array.removeAt、
     * Map.remove、Set.remove 的失败传播及元素顺序/计数是否仍一致。
     */
    for (TZrSize cursor = index; cursor + 1 < length; cursor++) {
        const SZrTypeValue *source = zr_container_array_get_value_fast(state, array, cursor + 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_remove_last(state, array);
}

/**
 * ## Summary
 * 为容器隐藏存储提供已有backing的借用读取边界；供entries读取与ensure复用同一对象身份，不因查询主动创建数组。
 *
 * ## Parameters
 * @param[in] state 有效执行域状态，供own字段查找和GC载荷转换；NULL拒绝。
 * @param[in,out] object 存活同域容器对象；读取可更新其字段与backing缓存。
 * @param[in] fieldString 存活同域的隐藏字段String键；当前仅items/entries包装入口使用。
 *
 * ## Returns
 * @return 已有internal ARRAY backing的借用指针；输入拒绝、own字段缺失/值形状不符或载荷不是ARRAY时NULL。NULL不独立解释线程错误。
 *
 * ## Constraints and lifecycle
 * 返回backing为同域GC对象的借用，不retain、不新建root；真实可达性依赖容器own字段及调用frame。缓存指针不构成owner，也不提供快照、锁或跨GC的稳定地址；调用方不得绕过域资格、对象布局及隐藏字段的更新协议。 快捷缓存命中不再次核fieldString、own Pair、memberVersion或具体容器原型，调用方必须保证对象只按自身隐藏存储角色使用。BUG: 正常Map/Set已通过容器路径缓存__zr_entries后，宿主保持旧/新backing可达，合法同域公开Core Object_SetValue可把此own字段替换为另一个已初始化ARRAY。Core现有槽覆盖虽检测hidden缓存，但其刷新只识别__zr_items而对__zr_entries直接返回；memberVersion增加也不清cachedHiddenItemsObject。本读取仅检查旧缓存的internalType后返回，后续容器查询仍读取旧backing，未反映已替换的own字段。仅证明同域C API替换后的陈旧读取，不授guest可访问隐藏字段、运行复现或UAF信用。TODO: 明确Core与容器对__zr_entries替换/删除的统一缓存失效协议，并继续核旧缓存的GC生存边界。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4615;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4516;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4520;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4525;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4526;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4531;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4532;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4533;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4537;zr_vm_lib_container/src/zr_vm_lib_container/module.c:44;zr_vm_lib_container/src/zr_vm_lib_container/module.c:58;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2038;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2242;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2706;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2720;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2727;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2732;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2741;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2753;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4522;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4587;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4588;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6642;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6734;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6803;zr_vm_core/src/zr_vm_core/object/object.c:50;zr_vm_core/src/zr_vm_core/object/object.c:55;zr_vm_core/src/zr_vm_core/object/object.c:249;zr_vm_core/src/zr_vm_core/object/object.c:250;zr_vm_core/src/zr_vm_core/object/object.c:469;zr_vm_core/src/zr_vm_core/object/object.c:475;zr_vm_core/src/zr_vm_core/object/object.c:479;zr_vm_core/src/zr_vm_core/object/object.c:490;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1825;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/object/object.c:1884;zr_vm_core/src/zr_vm_core/object/object.c:1889;zr_vm_core/src/zr_vm_core/object/object.c:1925;zr_vm_core/src/zr_vm_core/object/object.c:1926;zr_vm_core/src/zr_vm_core/object/object.c:1927;zr_vm_core/src/zr_vm_core/object/object.c:1937;zr_vm_core/src/zr_vm_core/object/object.c:1940;zr_vm_core/src/zr_vm_core/object/object.c:1943;zr_vm_core/src/zr_vm_core/object/object.c:1949;zr_vm_core/src/zr_vm_core/object/object.c:1950;zr_vm_core/src/zr_vm_core/object/object.c:1952;zr_vm_core/src/zr_vm_core/object/object_internal.h:311;zr_vm_core/src/zr_vm_core/object/object_internal.h:312;zr_vm_core/src/zr_vm_core/object/object_internal.h:315;zr_vm_core/src/zr_vm_core/object/object_internal.h:318;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:9;tests/container/test_container_runtime.c:1360;tests/container/test_container_runtime.c:1362;tests/container/test_container_runtime.c:1368;tests/container/test_container_runtime.c:1370;tests/container/test_container_runtime.c:1373;tests/container/test_container_runtime.c:1374;tests/container/test_container_runtime.c:1376;tests/container/test_container_runtime.c:1377;tests/container/test_container_runtime.c:1604
 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_hidden_array_fast(SZrState *state,
                                                                     SZrObject *object,
                                                                     SZrString *fieldString) {
    SZrHashKeyValuePair *pair;
    SZrObject *array;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    array = object->cachedHiddenItemsObject;
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, &object->cachedHiddenItemsPair);
    if (pair == ZR_NULL || (pair->value.type != ZR_VALUE_TYPE_OBJECT && pair->value.type != ZR_VALUE_TYPE_ARRAY) ||
        pair->value.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZR_CAST_OBJECT(state, pair->value.value.object);
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        object->cachedHiddenItemsObject = ZR_NULL;
        return ZR_NULL;
    }

    object->cachedHiddenItemsObject = array;
    return array;
}

/**
 * ## Summary
 * 把Map/Set的entries角色绑定到已有隐藏backing读取，供cached包装的回退路径使用；公开count不能替代真实存储查询。
 *
 * ## Parameters
 * @param[in] state 有效执行域状态，供entries字段键缓存及借用读取。
 * @param[in,out] object 存活同域Map/Set包装对象，读取可能预热本对象查找缓存。
 *
 * ## Returns
 * @return 已有entries ARRAY的借用指针；取键/隐藏字段读取不能给出有效backing时NULL，不创建新存储。
 *
 * ## Constraints and lifecycle
 * 返回backing为同域GC对象的借用，不retain、不新建root；真实可达性依赖容器own字段及调用frame。缓存指针不构成owner，也不提供快照、锁或跨GC的稳定地址；调用方不得绕过域资格、对象布局及隐藏字段的更新协议。 当前唯一直接caller是get_entries_array_cached_fast；ensure_entries通过另一个ensure接口，不直接调用此函数。共享隐藏缓存的__zr_entries替换问题见get_hidden_array_fast的BUG。TODO: 新增读取入口须保留不创建backing及线程错误独立检查的语义，不把count字段当ARRAY长度或validity证明。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4591;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4579;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4658;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6642;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6734;tests/container/test_container_runtime.c:1368;tests/container/test_container_runtime.c:1370;tests/container/test_container_runtime.c:1374;tests/container/test_container_runtime.c:1376
 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_entries_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_get_hidden_array_fast(state, object, zr_container_entries_field_string_fast(state));
}

/**
 * ## Summary
 * 为Map只读inline GET、SET现存条目及构造后重取提供不创建存储的借用入口；保留inline miss交给外层回退的约定。
 *
 * ## Parameters
 * @param[in] state 当前caller提供有效执行域状态；缓存命中分支不读取state，不能据此放宽新caller的域前提。
 * @param[in,out] object 存活同域Map包装对象；NULL直接给出NULL，回退可更新其缓存。
 *
 * ## Returns
 * @return 已缓存或通过entries字段查询得到的internal ARRAY借用；没有有效已有backing时NULL，不负责把NULL转为guest null。
 *
 * ## Constraints and lifecycle
 * 返回backing为同域GC对象的借用，不retain、不新建root；真实可达性依赖容器own字段及调用frame。缓存指针不构成owner，也不提供快照、锁或跨GC的稳定地址；调用方不得绕过域资格、对象布局及隐藏字段的更新协议。 readonly GET在无backing时false交dispatcher处理，SET新项在这里miss后另调用ensure_entries；返回指针不能跨用户hash/equals回调或GC当作稳定快照。BUG: 正常Map/Set已通过容器路径缓存__zr_entries后，宿主保持旧/新backing可达，合法同域公开Core Object_SetValue可把此own字段替换为另一个已初始化ARRAY。Core现有槽覆盖虽检测hidden缓存，但其刷新只识别__zr_items而对__zr_entries直接返回；memberVersion增加也不清cachedHiddenItemsObject。本读取仅检查旧缓存的internalType后返回，后续容器查询仍读取旧backing，未反映已替换的own字段。仅证明同域C API替换后的陈旧读取，不授guest可访问隐藏字段、运行复现或UAF信用。TODO: 明确Core与容器对__zr_entries替换/删除的统一缓存失效协议，并继续核旧缓存的GC生存边界。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6642;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6734;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4582;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4586;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4587;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4588;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4591;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6572;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6573;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6644;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:44;zr_vm_lib_container/src/zr_vm_lib_container/module.c:58;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2038;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2242;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2274;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2706;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2720;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2727;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2732;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2741;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2753;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4520;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4521;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4522;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4525;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4537;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6803;zr_vm_core/src/zr_vm_core/object/object.c:50;zr_vm_core/src/zr_vm_core/object/object.c:55;zr_vm_core/src/zr_vm_core/object/object.c:249;zr_vm_core/src/zr_vm_core/object/object.c:250;zr_vm_core/src/zr_vm_core/object/object.c:469;zr_vm_core/src/zr_vm_core/object/object.c:475;zr_vm_core/src/zr_vm_core/object/object.c:479;zr_vm_core/src/zr_vm_core/object/object.c:490;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1823;zr_vm_core/src/zr_vm_core/object/object.c:1825;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/src/zr_vm_core/object/object.c:1875;zr_vm_core/src/zr_vm_core/object/object.c:1884;zr_vm_core/src/zr_vm_core/object/object.c:1889;zr_vm_core/src/zr_vm_core/object/object.c:1925;zr_vm_core/src/zr_vm_core/object/object.c:1926;zr_vm_core/src/zr_vm_core/object/object.c:1927;zr_vm_core/src/zr_vm_core/object/object.c:1937;zr_vm_core/src/zr_vm_core/object/object.c:1940;zr_vm_core/src/zr_vm_core/object/object.c:1943;zr_vm_core/src/zr_vm_core/object/object.c:1949;zr_vm_core/src/zr_vm_core/object/object.c:1950;zr_vm_core/src/zr_vm_core/object/object.c:1952;zr_vm_core/src/zr_vm_core/object/object_internal.h:311;zr_vm_core/src/zr_vm_core/object/object_internal.h:312;zr_vm_core/src/zr_vm_core/object/object_internal.h:315;zr_vm_core/src/zr_vm_core/object/object_internal.h:318;zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:9;tests/container/test_container_runtime.c:1360;tests/container/test_container_runtime.c:1362;tests/container/test_container_runtime.c:1368;tests/container/test_container_runtime.c:1370;tests/container/test_container_runtime.c:1373;tests/container/test_container_runtime.c:1374;tests/container/test_container_runtime.c:1376;tests/container/test_container_runtime.c:1377;tests/container/test_container_runtime.c:1604
 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_entries_array_cached_fast(SZrState *state, SZrObject *object) {
    SZrObject *array;

    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    array = object->cachedHiddenItemsObject;
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    return zr_container_get_entries_array_fast(state, object);
}

/**
 * ## Summary
 * 建立容器按需取得隐藏存储的边界；允许部分查询初始化缺失backing，让写入及迭代调用共享own字段发布协议。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供已有backing读取、GC ARRAY创建和隐藏字段发布。
 * @param[in,out] object 存活同域包装对象，当前调用方保证非空；本接口没有在创建前独立拒绝NULL对象。
 * @param[in] fieldString 同域存活items/entries字段键；新发布时NULL拒绝，命中旧快捷缓存时不会独立验证。
 *
 * ## Returns
 * @return 可复用或已发布的ARRAY借用；创建或隐藏字段发布不能完成时NULL。分配可经Core抛异常，不能仅把它描述成false/NULL返回。
 *
 * ## Constraints and lifecycle
 * 返回backing为同域GC对象的借用，不retain、不新建root；真实可达性依赖容器own字段及调用frame。缓存指针不构成owner，也不提供快照、锁或跨GC的稳定地址；调用方不得绕过域资格、对象布局及隐藏字段的更新协议。 返回的是GC托管ARRAY，调用方不直接free；本函数不初始化公开length/capacity/count，也不修复已有backing内容。隐藏own值形状不符时可能替换为新ARRAY，缓存问题见get_hidden_array_fast的BUG。新建ARRAY先由本地C指针保存再写隐藏字段，没有本层显式root或异常回滚。TODO: 沿Array_New的Init与隐藏字段首次分配回退核未发布backing、包装对象的GC保护及异常pin收尾；沿失败返回明确新ARRAY的可达/回收责任，不能以非空返回替代Core实际写入证明。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:4658;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4679;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4615;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4617;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4621;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4623;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4626;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4627;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4633;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4637;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2706;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2741;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2746;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2753;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1356;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1361;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1363;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1365;zr_vm_core/src/zr_vm_core/object/object.c:1482;zr_vm_core/src/zr_vm_core/object/object.c:1488;zr_vm_core/src/zr_vm_core/object/object.c:1504;zr_vm_core/src/zr_vm_core/object/object.c:1509;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1793;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/base/hash_set.c:25;zr_vm_core/src/zr_vm_core/base/hash_set.c:39;tests/container/test_container_runtime.c:1368;tests/container/test_container_runtime.c:1370;tests/container/test_container_runtime.c:1374;tests/container/test_container_runtime.c:1604
 */
static ZR_FORCE_INLINE SZrObject *zr_container_ensure_hidden_array_fast(SZrState *state,
                                                                        SZrObject *object,
                                                                        SZrString *fieldString) {
    SZrObject *array = zr_container_get_hidden_array_fast(state, object, fieldString);

    if (array != ZR_NULL) {
        return array;
    }

    array = ZrLib_Array_New(state);
    if (array != ZR_NULL) {
        SZrTypeValue fieldValue;

        zr_container_value_set_object_fast(state, &fieldValue, array, ZR_VALUE_TYPE_ARRAY);
        if (fieldString == ZR_NULL ||
            !zr_container_set_cached_field_value_fast(state,
                                                      object,
                                                      fieldString,
                                                      &object->cachedHiddenItemsPair,
                                                      &fieldValue,
                                                      ZR_TRUE)) {
            return ZR_NULL;
        }
    }

    return array;
}

/**
 * ## Summary
 * 为Map/Set普通查询、写入、删除、清空及迭代提供按需存储入口；容器协议允许这些普通路径在缺backing时建立空entries。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态；字段键和backing创建可分配或抛异常。
 * @param[in,out] object 存活同域Map/Set包装对象，调用方先验证self。
 *
 * ## Returns
 * @return 已有或新发布entries ARRAY的借用；不能完成存储准备时NULL，Core异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 返回backing为同域GC对象的借用，不retain、不新建root；真实可达性依赖容器own字段及调用frame。缓存指针不构成owner，也不提供快照、锁或跨GC的稳定地址；调用方不得绕过域资格、对象布局及隐藏字段的更新协议。 本层不自动同步公开count、不预留键索引、不保证查找复杂度；Map存Pair而Set存T，语义由上层区分。普通GET/contains/iterator也调用此函数，不能把所有读取说成不分配；只读inline另用get_entries_array_cached_fast。既有缓存替换问题及新建GC保护疑问分别见get_hidden/ensure_hidden。TODO: 保持普通读取与readonly inline无backing时的结果/回退差异，逐caller核NULL与threadStatus检查。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6333;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6365;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6407;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6446;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6494;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6644;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6803;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6879;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6921;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6960;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4658;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6336;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6408;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6496;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6497;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6498;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6810;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6922;tests/container/test_container_runtime.c:1218;tests/container/test_container_runtime.c:1226;tests/container/test_container_runtime.c:1247;tests/container/test_container_runtime.c:1332;tests/container/test_container_runtime.c:1343;tests/container/test_container_runtime.c:1374;tests/container/test_container_runtime.c:1560;tests/container/test_container_runtime.c:1604
 */
static ZR_FORCE_INLINE SZrObject *zr_container_ensure_entries_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_ensure_hidden_array_fast(state, object, zr_container_entries_field_string_fast(state));
}

/**
 * ## Summary
 * 把Array元素存储角色绑定到按需隐藏ARRAY取得；供公开元素操作、清空和迭代使用，物理内容与公开length/capacity的同步由各caller负责。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供items字段键及backing创建。
 * @param[in,out] object 存活同域Array包装对象，caller须先验证self并维护公开字段。
 *
 * ## Returns
 * @return 已有或新发布items ARRAY的借用；不能完成准备时NULL，分配异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 返回backing为同域GC对象的借用，不retain、不新建root；真实可达性依赖容器own字段及调用frame。缓存指针不构成owner，也不提供快照、锁或跨GC的稳定地址；调用方不得绕过域资格、对象布局及隐藏字段的更新协议。 不分配元素快照、不预留raw或dense容量、不把公开capacity/length当物理已分配证明；清空后也可复用既有backing。借用读取、写入、枚举可改变backing表示或代际，caller需重取相应视图。TODO: 所有本接口caller须按ensure_hidden的创建/异常边界处理；不要用缓存存在代替GC保护、capacity预约或owner释放协议。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5791;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5840;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5881;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5918;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5958;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6033;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6101;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6196;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6246;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1509;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4679;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5798;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5845;tests/container/test_container_runtime.c:768;tests/container/test_container_runtime.c:769;tests/container/test_container_runtime.c:770;tests/container/test_container_runtime.c:771;tests/container/test_container_runtime.c:772;tests/container/test_container_runtime.c:773;tests/container/test_container_runtime.c:774
 */
static ZR_FORCE_INLINE SZrObject *zr_container_ensure_items_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_ensure_hidden_array_fast(state, object, zr_container_items_field_string_fast(state));
}

/**
 * ## Summary
 * 为Map新条目建立Pair的构造与发布边界；只有两个分量均初始化完成才把GC对象借用交给entries追加路径。
 *
 * ## Parameters
 * @param[in,out] state 有效同域执行状态；类型查找、对象或字段初始化可分配/抛异常。
 * @param[in] first 同域存活首分量，NULL表示初始化为null；Map调用方提供满足K契约的键。
 * @param[in] second 同域存活次分量，NULL表示初始化为null；Map调用方提供待写值。
 *
 * ## Returns
 * @return 两个字段初始化完成的Pair借用；可返回的构造/字段失败路径为NULL，分配异常可非局部退出。返回尚未接入Map。
 *
 * ## Constraints and lifecycle
 * 按已注册Pair原型创建普通STRUCT，字段复制遵守Core ownership规则；本接口不建立额外root或事务回滚。返回后由caller接入entries，借用须受相应分配/GC保护。BUG: 触发条件为合法同域C API对正常Map<String,int>新增项，Pair的first新own槽进入Object_SetValue/HashSet_Add。字段名和输入先预热，Map及已有entries backing已正常构造；宿主分配器按sizeof(HashKeyValuePair)/HASH_PAIR拒绝新对象首字段的申请与FullGC重试两次，然后解除拒绝，让异常规范化申请成功。外层TryRun捕获MEMORY_ERROR、ResetThread后继续同一VM。Core临时ignored pin因longjmp越过本地unpin而滞留，TryRun仅恢复AOT根和GC域scope、ResetThread不清ignored registry，后续完整GC仍把未发布对象作为根。TODO: 建立字段写入异常与正常返回等价的unpin清理，并独立核解除临时根之后的字段owner释放职责。 后续字段初始化时已有Shared owner的滞留须另行选择准确分配序号验证，不从首字段失败推定已经复制该owner。 这是当前源码的条件性静态调用链；尚未运行容器故障注入复现。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4701;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4702;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4706;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4707;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4711;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4716;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4721;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4723;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6650;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7544;zr_vm_core/src/zr_vm_core/object/object.c:1482;zr_vm_core/src/zr_vm_core/object/object.c:1488;zr_vm_core/src/zr_vm_core/object/object.c:1504;zr_vm_core/src/zr_vm_core/object/object.c:1509;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:558;zr_vm_core/src/zr_vm_core/object/object.c:559;zr_vm_core/src/zr_vm_core/object/object.c:565;zr_vm_core/src/zr_vm_core/object/object.c:573;zr_vm_core/src/zr_vm_core/object/object.c:1702;zr_vm_core/src/zr_vm_core/object/object.c:1713;zr_vm_core/src/zr_vm_core/object/object.c:1716;zr_vm_core/src/zr_vm_core/object/object.c:1727;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1754;zr_vm_core/src/zr_vm_core/object/object.c:1793;zr_vm_core/src/zr_vm_core/object/object.c:1795;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1843;zr_vm_core/src/zr_vm_core/object/object.c:1845;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1858;zr_vm_core/src/zr_vm_core/object/object.c:1860;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/include/zr_vm_core/hash_set.h:488;zr_vm_core/include/zr_vm_core/hash_set.h:494;zr_vm_core/include/zr_vm_core/hash_set.h:503;zr_vm_core/include/zr_vm_core/hash_set.h:507;zr_vm_core/include/zr_vm_core/hash_set.h:510;zr_vm_core/src/zr_vm_core/base/memory.c:13;zr_vm_core/src/zr_vm_core/base/memory.c:16;zr_vm_core/src/zr_vm_core/base/memory.c:17;zr_vm_core/src/zr_vm_core/base/memory.c:18;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:42;zr_vm_core/src/zr_vm_core/base/memory.c:44;zr_vm_core/src/zr_vm_core/base/memory.c:45;zr_vm_core/src/zr_vm_core/base/memory.c:46;zr_vm_core/src/zr_vm_core/ownership/ownership.c:137;zr_vm_core/src/zr_vm_core/ownership/ownership.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership.c:156;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:755;zr_vm_core/src/zr_vm_core/ownership/ownership.c:801;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:690;zr_vm_core/src/zr_vm_core/gc/gc.c:701;zr_vm_core/src/zr_vm_core/gc/gc.c:702;zr_vm_core/src/zr_vm_core/gc/gc.c:704;zr_vm_core/src/zr_vm_core/gc/gc.c:711;zr_vm_core/src/zr_vm_core/gc/gc.c:733;zr_vm_core/src/zr_vm_core/gc/gc.c:745;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:448;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:456;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:457;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:464;zr_vm_core/src/zr_vm_core/exception/exception.c:548;zr_vm_core/src/zr_vm_core/exception/exception.c:556;zr_vm_core/src/zr_vm_core/exception/exception.c:557;zr_vm_core/src/zr_vm_core/exception/exception.c:560;zr_vm_core/src/zr_vm_core/exception/exception.c:563;zr_vm_core/src/zr_vm_core/exception/exception.c:565;zr_vm_core/src/zr_vm_core/exception/exception.c:566;zr_vm_core/src/zr_vm_core/exception/exception.c:570;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:19;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:22;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:24;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:26;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:30;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:43;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:45;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:53;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:59;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:62;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:36;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:40;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:52;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:54;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:58;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:59;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:60;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:63;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:74;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:76;zr_vm_core/src/zr_vm_core/runtime/state.c:272;zr_vm_core/src/zr_vm_core/runtime/state.c:274;zr_vm_core/src/zr_vm_core/runtime/state.c:281;zr_vm_core/src/zr_vm_core/runtime/state.c:284;zr_vm_core/src/zr_vm_core/runtime/state.c:291;zr_vm_core/src/zr_vm_core/runtime/state.c:297;zr_vm_core/src/zr_vm_core/runtime/state.c:299;zr_vm_core/src/zr_vm_core/runtime/state.c:305;zr_vm_core/src/zr_vm_core/runtime/state.c:311;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:873;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:881;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:884;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:889;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:890;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1214;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1223;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1237;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1241;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1674;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1675;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7444;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7445;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7446;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7733;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7735;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7737;zr_vm_library/src/zr_vm_library/builtin_module.c:355;zr_vm_library/src/zr_vm_library/builtin_module.c:356;zr_vm_library/src/zr_vm_library/builtin_module.c:358;zr_vm_core/include/zr_vm_core/gc.h:346;zr_vm_core/include/zr_vm_core/gc.h:358;zr_vm_core/include/zr_vm_core/gc.h:362;zr_vm_core/include/zr_vm_core/gc.h:366;zr_vm_core/include/zr_vm_core/gc.h:367;zr_vm_core/include/zr_vm_core/global.h:174;zr_vm_core/include/zr_vm_core/memory.h:22;zr_vm_core/include/zr_vm_core/memory.h:37;zr_vm_core/include/zr_vm_core/memory.h:39;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:339;tests/core/test_ssa_cross_domain_clone_oom.c:151;tests/core/test_ssa_cross_domain_clone_oom.c:160;tests/core/test_ssa_cross_domain_clone_oom.c:162;tests/core/test_ssa_cross_domain_clone_oom.c:163;tests/core/test_ssa_cross_domain_clone_oom.c:170;tests/core/test_ssa_cross_domain_clone_oom.c:172;tests/core/test_ssa_cross_domain_clone_oom.c:173;tests/core/test_ssa_cross_domain_clone_oom.c:175;tests/core/test_ssa_cross_domain_clone_oom.c:178;tests/core/test_ssa_cross_domain_clone_oom.c:186;tests/core/test_ssa_cross_domain_clone_oom.c:190;tests/core/test_ssa_cross_domain_clone_oom.c:195;tests/core/test_ssa_cross_domain_clone_oom.c:514;tests/core/test_ssa_cross_domain_clone_oom.c:525;tests/core/test_ssa_cross_domain_clone_oom.c:557;tests/core/test_ssa_cross_domain_clone_oom.c:562;zr_vm_core/src/zr_vm_core/runtime/global.c:360;zr_vm_core/src/zr_vm_core/runtime/global.c:370;zr_vm_core/src/zr_vm_core/runtime/global.c:374;zr_vm_core/src/zr_vm_core/exception/exception.c:710;zr_vm_core/src/zr_vm_core/exception/exception.c:719;zr_vm_core/src/zr_vm_core/exception/exception.c:731
 */
static SZrObject *zr_container_make_pair(SZrState *state, const SZrTypeValue *first, const SZrTypeValue *second) {
    SZrObject *pair = ZrLib_Type_NewInstance(state, "Pair");
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    if (first != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, pair, kContainerPairFirstField, first)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, pair, kContainerPairFirstField)) {
            return ZR_NULL;
        }
    }
    if (second != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, pair, kContainerPairSecondField, second)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, pair, kContainerPairSecondField)) {
            return ZR_NULL;
        }
    }
    return pair;
}

/**
 * ## Summary
 * 为LinkedList新增元素建立待接入节点的初始化边界；把元素保存与双向链接初始化收拢为一个交付约定。
 *
 * ## Parameters
 * @param[in,out] state 有效同域执行状态；对象或字段初始化可分配/抛异常。
 * @param[in] value 同域存活元素，NULL表示初始化为null；T无额外泛型约束。
 *
 * ## Returns
 * @return 已初始化value及空next/previous的LinkedNode借用；可返回的创建/字段失败为NULL，分配异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 节点尚未接入list，本接口不改变首尾/count、不转移输入owner、不另建持久root；value通过Core复制，外部节点引用可继续保持元素可达。BUG: 触发条件为合法同域C API把普通String交给无附加T约束的LinkedList.addFirst/addLast，新节点的value首own槽进入Object_SetValue/HashSet_Add。字段名和输入先预热，LinkedList已正常初始化first/last/count；宿主分配器按sizeof(HashKeyValuePair)/HASH_PAIR拒绝新对象首字段的申请与FullGC重试两次，然后解除拒绝，让异常规范化申请成功。外层TryRun捕获MEMORY_ERROR、ResetThread后继续同一VM。Core临时ignored pin因longjmp越过本地unpin而滞留，TryRun仅恢复AOT根和GC域scope、ResetThread不清ignored registry，后续完整GC仍把未发布对象作为根。TODO: 建立字段写入异常与正常返回等价的unpin清理，并独立核解除临时根之后的字段owner释放职责。 后续next/previous字段失败时已有Shared owner的滞留须另行选择准确分配序号验证。 这是当前源码的条件性静态调用链；尚未运行容器故障注入复现。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7170;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4744;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4745;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4749;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4750;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4754;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4755;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4759;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4761;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7122;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7175;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7622;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7625;zr_vm_core/src/zr_vm_core/object/object.c:1482;zr_vm_core/src/zr_vm_core/object/object.c:1488;zr_vm_core/src/zr_vm_core/object/object.c:1504;zr_vm_core/src/zr_vm_core/object/object.c:1509;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:558;zr_vm_core/src/zr_vm_core/object/object.c:559;zr_vm_core/src/zr_vm_core/object/object.c:565;zr_vm_core/src/zr_vm_core/object/object.c:573;zr_vm_core/src/zr_vm_core/object/object.c:1702;zr_vm_core/src/zr_vm_core/object/object.c:1713;zr_vm_core/src/zr_vm_core/object/object.c:1716;zr_vm_core/src/zr_vm_core/object/object.c:1727;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1754;zr_vm_core/src/zr_vm_core/object/object.c:1793;zr_vm_core/src/zr_vm_core/object/object.c:1795;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1843;zr_vm_core/src/zr_vm_core/object/object.c:1845;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1858;zr_vm_core/src/zr_vm_core/object/object.c:1860;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/include/zr_vm_core/hash_set.h:488;zr_vm_core/include/zr_vm_core/hash_set.h:494;zr_vm_core/include/zr_vm_core/hash_set.h:503;zr_vm_core/include/zr_vm_core/hash_set.h:507;zr_vm_core/include/zr_vm_core/hash_set.h:510;zr_vm_core/src/zr_vm_core/base/memory.c:13;zr_vm_core/src/zr_vm_core/base/memory.c:16;zr_vm_core/src/zr_vm_core/base/memory.c:17;zr_vm_core/src/zr_vm_core/base/memory.c:18;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:42;zr_vm_core/src/zr_vm_core/base/memory.c:44;zr_vm_core/src/zr_vm_core/base/memory.c:45;zr_vm_core/src/zr_vm_core/base/memory.c:46;zr_vm_core/src/zr_vm_core/ownership/ownership.c:137;zr_vm_core/src/zr_vm_core/ownership/ownership.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership.c:156;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:755;zr_vm_core/src/zr_vm_core/ownership/ownership.c:801;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:690;zr_vm_core/src/zr_vm_core/gc/gc.c:701;zr_vm_core/src/zr_vm_core/gc/gc.c:702;zr_vm_core/src/zr_vm_core/gc/gc.c:704;zr_vm_core/src/zr_vm_core/gc/gc.c:711;zr_vm_core/src/zr_vm_core/gc/gc.c:733;zr_vm_core/src/zr_vm_core/gc/gc.c:745;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:448;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:456;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:457;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:464;zr_vm_core/src/zr_vm_core/exception/exception.c:548;zr_vm_core/src/zr_vm_core/exception/exception.c:556;zr_vm_core/src/zr_vm_core/exception/exception.c:557;zr_vm_core/src/zr_vm_core/exception/exception.c:560;zr_vm_core/src/zr_vm_core/exception/exception.c:563;zr_vm_core/src/zr_vm_core/exception/exception.c:565;zr_vm_core/src/zr_vm_core/exception/exception.c:566;zr_vm_core/src/zr_vm_core/exception/exception.c:570;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:19;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:22;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:24;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:26;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:30;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:43;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:45;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:53;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:59;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:62;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:36;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:40;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:52;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:54;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:58;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:59;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:60;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:63;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:74;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:76;zr_vm_core/src/zr_vm_core/runtime/state.c:272;zr_vm_core/src/zr_vm_core/runtime/state.c:274;zr_vm_core/src/zr_vm_core/runtime/state.c:281;zr_vm_core/src/zr_vm_core/runtime/state.c:284;zr_vm_core/src/zr_vm_core/runtime/state.c:291;zr_vm_core/src/zr_vm_core/runtime/state.c:297;zr_vm_core/src/zr_vm_core/runtime/state.c:299;zr_vm_core/src/zr_vm_core/runtime/state.c:305;zr_vm_core/src/zr_vm_core/runtime/state.c:311;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:873;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:881;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:884;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:889;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:890;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1214;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1223;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1237;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1241;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1674;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1675;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7444;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7445;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7446;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7733;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7735;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7737;zr_vm_library/src/zr_vm_library/builtin_module.c:355;zr_vm_library/src/zr_vm_library/builtin_module.c:356;zr_vm_library/src/zr_vm_library/builtin_module.c:358;zr_vm_core/include/zr_vm_core/gc.h:346;zr_vm_core/include/zr_vm_core/gc.h:358;zr_vm_core/include/zr_vm_core/gc.h:362;zr_vm_core/include/zr_vm_core/gc.h:366;zr_vm_core/include/zr_vm_core/gc.h:367;zr_vm_core/include/zr_vm_core/global.h:174;zr_vm_core/include/zr_vm_core/memory.h:22;zr_vm_core/include/zr_vm_core/memory.h:37;zr_vm_core/include/zr_vm_core/memory.h:39;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:339;tests/core/test_ssa_cross_domain_clone_oom.c:151;tests/core/test_ssa_cross_domain_clone_oom.c:160;tests/core/test_ssa_cross_domain_clone_oom.c:162;tests/core/test_ssa_cross_domain_clone_oom.c:163;tests/core/test_ssa_cross_domain_clone_oom.c:170;tests/core/test_ssa_cross_domain_clone_oom.c:172;tests/core/test_ssa_cross_domain_clone_oom.c:173;tests/core/test_ssa_cross_domain_clone_oom.c:175;tests/core/test_ssa_cross_domain_clone_oom.c:178;tests/core/test_ssa_cross_domain_clone_oom.c:186;tests/core/test_ssa_cross_domain_clone_oom.c:190;tests/core/test_ssa_cross_domain_clone_oom.c:195;tests/core/test_ssa_cross_domain_clone_oom.c:514;tests/core/test_ssa_cross_domain_clone_oom.c:525;tests/core/test_ssa_cross_domain_clone_oom.c:557;tests/core/test_ssa_cross_domain_clone_oom.c:562;zr_vm_core/src/zr_vm_core/runtime/global.c:360;zr_vm_core/src/zr_vm_core/runtime/global.c:370;zr_vm_core/src/zr_vm_core/runtime/global.c:374;zr_vm_core/src/zr_vm_core/exception/exception.c:710;zr_vm_core/src/zr_vm_core/exception/exception.c:719;zr_vm_core/src/zr_vm_core/exception/exception.c:731
 */
static SZrObject *zr_container_make_linked_node(SZrState *state, const SZrTypeValue *value) {
    SZrObject *node = ZrLib_Type_NewInstance(state, "LinkedNode");
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (value != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, node, kContainerValueField, value)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, node, kContainerValueField)) {
            return ZR_NULL;
        }
    }
    if (!zr_container_set_null_field_fast(state, node, kContainerNextField) ||
        !zr_container_set_null_field_fast(state, node, kContainerPreviousField)) {
        return ZR_NULL;
    }
    return node;
}

/**
 * ## Summary
 * 为Map.containsKey/remove提供首个匹配键的实际backing位置及可选Pair借用值。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域有效状态；用户hash/equality可更新线程状态或非局部退出。
 * @param[in,out] entries 存活的internal ARRAY backing，调用者维持布局、连续整数键和域资格。
 * @param[in] key 调用期间有效的借用键槽；由caller维持Core所需owner/域资格。
 * @param[out] outIndex 可NULL，进入时先置0，命中写下标。
 * @param[out] outEntryObject 可NULL，进入时先置NULL，命中写借用Pair指针。
 *
 * ## Returns
 * @return 普通命中true并填写非NULL输出；false涵盖输入/转换拒绝或未命中，不单独证明线程无错误。用户hash/equality抛错或longjmp不由bool表示。
 *
 * ## Constraints and lifecycle
 * 实际元素来自entries，公开count不决定扫描边界。字符串命中cache可直接返回；miss后各扫描分支捕获一次scalar length，不构成元素/版本快照。短字符串身份比较依赖Core驻留契约，长字符串比较hash及内容；其他键可调用用户hash/equality。输出Pair借自backing/cache，不retain、不复制或建立独立root。既有cache失效BUG沿cache helper记录。TODO: 核用户回调重入/GC期间backing、键槽与Pair借用值的有效期和修改可见性。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6366;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4784;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4806;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4834;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4893;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4895;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4076;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7444;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_core/src/zr_vm_core/string/string.c:767;zr_vm_core/src/zr_vm_core/string/string.c:778;zr_vm_core/src/zr_vm_core/string/string.c:796;zr_vm_core/src/zr_vm_core/string/string.c:804
 */
static TZrBool zr_container_map_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *key,
                                           TZrSize *outIndex,
                                           SZrObject **outEntryObject) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (outEntryObject != ZR_NULL) {
        *outEntryObject = ZR_NULL;
    }
    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_FALSE;
    }

    if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
        SZrRawObject *wantedRawObject = key->value.object;
        SZrString *wantedString = ZR_NULL;

        {
            ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);

            if (zr_container_try_hot_map_lookup_cache(state,
                                                      cache,
                                                      entries,
                                                      wantedRawObject,
                                                      outIndex,
                                                      outEntryObject)) {
                return ZR_TRUE;
            }
        }

        wantedHash = wantedRawObject->hash;
        wantedString = ZR_CAST_STRING(state, wantedRawObject);
        if (wantedString == ZR_NULL) {
            return ZR_FALSE;
        }

        length = zr_container_array_length_fast(entries);
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryKey;
            SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

            if (entryObject == ZR_NULL) {
                continue;
            }

            entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
            if (entryKey == ZR_NULL || entryKey->type != ZR_VALUE_TYPE_STRING || entryKey->value.object == ZR_NULL) {
                continue;
            }
            if (entryKey->value.object == wantedRawObject) {
                if (outIndex != ZR_NULL) {
                    *outIndex = index;
                }
                if (outEntryObject != ZR_NULL) {
                    *outEntryObject = entryObject;
                }
                zr_container_update_hot_map_lookup_cache(state,
                                                         entries,
                                                         wantedRawObject,
                                                         index,
                                                         entryObject);
                return ZR_TRUE;
            }
            if (entryKey->value.object->hash != wantedHash) {
                continue;
            }

            {
                SZrString *entryString = ZR_CAST_STRING(state, entryKey->value.object);

                if (entryString != ZR_NULL &&
                    !ZrCore_String_IsShort(entryString) &&
                    !ZrCore_String_IsShort(wantedString) &&
                    ZrCore_String_Equal(entryString, wantedString)) {
                    if (outIndex != ZR_NULL) {
                        *outIndex = index;
                    }
                    if (outEntryObject != ZR_NULL) {
                        *outEntryObject = entryObject;
                    }
                    zr_container_update_hot_map_lookup_cache(state,
                                                             entries,
                                                             wantedRawObject,
                                                             index,
                                                             entryObject);
                    return ZR_TRUE;
                }
            }
        }

        zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, 0u, ZR_NULL);
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = zr_container_array_length_fast(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryKey;
        SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

        if (entryObject == ZR_NULL) {
            continue;
        }

        entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            if (outEntryObject != ZR_NULL) {
                *outEntryObject = entryObject;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/**
 * ## Summary
 * 为Map普通与inline GET/SET返回匹配Pair entry，避免额外结果复制和下标转换。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域有效状态；用户hash/equality可更新线程状态或非局部退出。
 * @param[in,out] entries 存活的internal ARRAY backing，调用者维持布局、连续整数键和域资格。
 * @param[in] key 调用期间有效的借用键槽；由caller维持Core所需owner/域资格。
 *
 * ## Returns
 * @return 普通命中返回Pair借用指针；NULL涵盖缺项、输入/转换拒绝或搜索失败。用户回调非局部退出不由NULL表示，caller须结合threadStatus。
 *
 * ## Constraints and lifecycle
 * 实际元素来自entries，公开count不决定扫描边界。字符串命中cache可直接返回；miss后各扫描分支捕获一次scalar length，不构成元素/版本快照。短字符串身份比较依赖Core驻留契约，长字符串比较hash及内容；其他键可调用用户hash/equality。输出Pair借自backing/cache，不retain、不复制或建立独立root。既有cache失效BUG沿cache helper记录。TODO: 核用户回调重入/GC期间backing、键槽与Pair借用值的有效期和修改可见性。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6626;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6736;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4927;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4945;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4966;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5004;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6475
 */
static ZR_FORCE_INLINE SZrObject *zr_container_map_find_entry_object_fast(SZrState *state,
                                                                          SZrObject *entries,
                                                                          const SZrTypeValue *key) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_NULL;
    }

    if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
        SZrRawObject *wantedRawObject = key->value.object;
        SZrString *wantedString = ZR_NULL;
        SZrObject *entryObject = ZR_NULL;

        {
            ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);

            entryObject = zr_container_try_hot_map_lookup_cache_entry(cache, entries, wantedRawObject);
            if (entryObject != ZR_NULL) {
                return entryObject;
            }
        }

        wantedHash = wantedRawObject->hash;
        wantedString = ZR_CAST_STRING(state, wantedRawObject);
        if (wantedString == ZR_NULL) {
            return ZR_NULL;
        }

        length = zr_container_array_length_fast(entries);
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryKey;

            entryObject = zr_container_array_get_object_fast(state, entries, index);
            if (entryObject == ZR_NULL) {
                continue;
            }

            entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
            if (entryKey == ZR_NULL || entryKey->type != ZR_VALUE_TYPE_STRING || entryKey->value.object == ZR_NULL) {
                continue;
            }
            if (entryKey->value.object == wantedRawObject) {
                zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, index, entryObject);
                return entryObject;
            }
            if (entryKey->value.object->hash != wantedHash) {
                continue;
            }

            {
                SZrString *entryString = ZR_CAST_STRING(state, entryKey->value.object);

                if (entryString != ZR_NULL &&
                    !ZrCore_String_IsShort(entryString) &&
                    !ZrCore_String_IsShort(wantedString) &&
                    ZrCore_String_Equal(entryString, wantedString)) {
                    zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, index, entryObject);
                    return entryObject;
                }
            }
        }

        return ZR_NULL;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = zr_container_array_length_fast(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryKey;
        SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

        if (entryObject == ZR_NULL) {
            continue;
        }

        entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

/**
 * ## Summary
 * 为Set去重、成员查询和删除提供首个相等元素的位置。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域有效状态；用户hash/equality可更新线程状态或非局部退出。
 * @param[in,out] entries 存活的internal ARRAY backing，调用者维持布局、连续整数键和域资格。
 * @param[in] value 调用期间有效的借用查找值，caller维持Core所需owner/域资格。
 * @param[out] outIndex 可NULL，进入时先置0，命中写下标。
 *
 * ## Returns
 * @return 普通命中true并填写下标；false涵盖无效输入、无匹配或搜索失败。用户hash/equality可能非局部退出，bool不替代线程状态。
 *
 * ## Constraints and lifecycle
 * 入口捕获一次scalar length，按实际backing线性搜索；它不复制元素或建立版本快照。raw-int表示对整数needle尝试int64读取，失败后不回通用比较；普通整数仅使用整数helper，非整数使用hash/equality。TODO: 核unsigned越界lane、跨标签等值意图，以及用户回调重入期间借用值/扫描边界有效期；不升级BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6849;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6880;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5033;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5047;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5066;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5068;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5078;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5081;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3450;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4076;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7449;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7731;tests/container/test_container_runtime.c:1731;tests/container/test_container_runtime.c:1745
 */
static TZrBool zr_container_set_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *value,
                                           TZrSize *outIndex) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (state == ZR_NULL || entries == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(entries);
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        if (zr_container_array_raw_int_active(entries)) {
            TZrInt64 signedNeedle;

            if (zr_container_try_read_int64_needle_fast(value, &signedNeedle)) {
                for (TZrSize index = 0; index < length; index++) {
                    if (entries->superArrayRawIntData[index] == signedNeedle) {
                        if (outIndex != ZR_NULL) {
                            *outIndex = index;
                        }
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, entries, index);

            if (zr_container_int_values_equal_fast(entryValue, value)) {
                if (outIndex != ZR_NULL) {
                    *outIndex = index;
                }
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, value);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, entries, index);
        if (entryValue != ZR_NULL && zr_container_value_hash(state, entryValue) == wantedHash &&
            zr_container_values_equal(state, entryValue, value)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/**
 * ## Summary
 * 维护Array公开capacity的增长政策，供add/insert在元素写入前同步可见容量；这项元数据更新不预约真实backing空间。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供capacity字段读取和写入；NULL拒绝。
 * @param[in,out] arrayObject 存活同域Array包装对象，既有capacity须符合元数据契约；NULL拒绝。
 * @param[in] requiredLength caller从实际backing长度得到的所需元素数，须可表示且增长计算不能溢出。
 *
 * ## Returns
 * @return 当前公开capacity已覆盖所需长度或容量字段发布成功时true；输入或字段发布失败时false，不表示之后元素申请必能成功。
 *
 * ## Constraints and lifecycle
 * state、包装对象或backing均为本次借用，本接口不建立root、owner或互斥；caller必须保持有效同域对象及所用nodeMap地址。 这里操作包装对象而非internal backing；add/insert先调用本函数再执行存储，后续失败不回滚已经增加的capacity。非正capacity使用配置初值；本层不校验业务原型、不修复length/count，也不检查requiredLength加法或倍增上界。TODO: 对照两caller的length+1与TZrInt64倍增统一可表示上限，明确字段可写和极大C API输入的合法边界；现有普通容量测试没有证明极值下不会溢出或循环。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5798;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5114;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5118;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5119;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5120;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5121;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5126;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5130;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5131;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5132;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5134;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5797;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5841;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7458;tests/container/test_container_runtime.c:190;tests/container/test_container_runtime.c:219;tests/container/test_container_runtime.c:745;tests/container/test_container_runtime.c:747;tests/container/test_container_runtime.c:751;tests/container/test_container_runtime.c:768;zr_vm_lib_container/include/zr_vm_lib_container/conf.h:12;zr_vm_lib_container/include/zr_vm_lib_container/conf.h:13
 */
static TZrBool zr_container_array_ensure_capacity(SZrState *state, SZrObject *arrayObject, TZrSize requiredLength) {
    TZrInt64 capacity;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    capacity = zr_container_get_int_field(state, arrayObject, kContainerCapacityField, 0);
    if (capacity <= 0) {
        capacity = ZR_CONTAINER_SEQUENCE_INITIAL_CAPACITY;
        if (!zr_container_set_int_field_fast(state, arrayObject, kContainerCapacityField, capacity)) {
            return ZR_FALSE;
        }
    }

    if ((TZrSize)capacity >= requiredLength) {
        return ZR_TRUE;
    }

    /* TODO: 正值扩容倍增仍需检查 TZrInt64 上界，防止溢出。 */
    while ((TZrSize)capacity < requiredLength) {
        capacity *= ZR_CONTAINER_SEQUENCE_GROWTH_FACTOR;
    }
    return zr_container_set_int_field_fast(state, arrayObject, kContainerCapacityField, capacity);
}

/**
 * ## Summary
 * 连接Array带初始capacity构造与Core稠密桶/Pair池预约，让构造器在公开包装字段发布前准备物理追加支持。
 *
 * ## Parameters
 * @param[in,out] state 有效执行域状态，供bucket/pool申请；NULL拒绝。
 * @param[in,out] itemsObject caller新建、同域存活且已初始化nodeMap的internal ARRAY backing；本函数只核指针非空。
 * @param[in] requiredCapacity 要预约的稠密元素数，须满足Core容量/块字节可表示上限；零表示不预约。
 *
 * ## Returns
 * @return 零预约或稠密桶与Pair池准备均完成时true；输入或准备失败时false。零分支不证明nodeMap就绪，GcMalloc失败可抛异常。
 *
 * ## Constraints and lifecycle
 * state、包装对象或backing均为本次借用，本接口不建立root、owner或互斥；caller必须保持有效同域对象及所用nodeMap地址。 仅用于Array构造，不设公开capacity/length、不增加元素、不预留raw-int buffer；dense成功而pool失败可留下已扩大的bucket，没有事务回滚。本层也不校验internalType、不pin新backing。TODO: 追Array构造器的未发布itemsObject经Pair池GcMalloc/FullGC重试时的根与地址稳定性，并统一极大requiredCapacity在round-up、bucket字节和pool块字节的拒绝条件；当前容量预约测试不授OOM或moving-GC信用。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5752;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5158;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5161;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5162;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5165;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5166;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5169;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5732;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5749;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5756;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5758;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5761;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1356;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1361;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1363;zr_vm_core/include/zr_vm_core/hash_set.h:131;zr_vm_core/include/zr_vm_core/hash_set.h:136;zr_vm_core/include/zr_vm_core/hash_set.h:362;zr_vm_core/include/zr_vm_core/hash_set.h:367;zr_vm_core/include/zr_vm_core/hash_set.h:371;zr_vm_core/include/zr_vm_core/hash_set.h:378;zr_vm_core/include/zr_vm_core/hash_set.h:379;zr_vm_core/include/zr_vm_core/hash_set.h:380;zr_vm_core/src/zr_vm_core/base/hash_set.c:102;zr_vm_core/src/zr_vm_core/base/hash_set.c:110;zr_vm_core/src/zr_vm_core/base/hash_set.c:114;zr_vm_core/src/zr_vm_core/base/hash_set.c:115;zr_vm_core/src/zr_vm_core/base/hash_set.c:118;zr_vm_core/src/zr_vm_core/base/hash_set.c:119;zr_vm_core/src/zr_vm_core/base/hash_set.c:124;zr_vm_core/src/zr_vm_core/base/hash_set.c:128;zr_vm_core/src/zr_vm_core/base/hash_set.c:140;tests/container/test_container_runtime.c:190;tests/container/test_container_runtime.c:219
 */
static TZrBool zr_container_array_prepare_backing_storage(SZrState *state,
                                                          SZrObject *itemsObject,
                                                          TZrSize requiredCapacity) {
    if (state == ZR_NULL || itemsObject == ZR_NULL) {
        return ZR_FALSE;
    }
    if (requiredCapacity == 0) {
        return ZR_TRUE;
    }

    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, &itemsObject->nodeMap, requiredCapacity)) {
        return ZR_FALSE;
    }

    return ZrCore_HashSet_EnsurePairPoolForElementCount(state, &itemsObject->nodeMap, requiredCapacity);
}

/**
 * ## Summary
 * 为五类getIterator入口建立独立Enumerator游标，并通过实例字段保持来源及当前位置可达；集合本身不被复制成快照。
 *
 * ## Parameters
 * @param[in,out] state 有效执行状态，负责分配与字段发布。
 * @param[in] source 可为空的数组backing、内建数组或列表；非空值仍由原域管理。
 * @param[in] sourceType 非空source的OBJECT/ARRAY标签，调用方保证与对象一致。
 * @param[in] indexValue 初始整数游标，通常0；本函数不检查范围。
 * @param[in] nextNode 可为空的初始链表节点，列表入口取当时first。
 * @param[in] moveNextFunction 本模块两种受支持native回调之一。
 *
 * ## Returns
 * @return 字段初始化成功后的GC实例借用指针，供上层发布到result；显式失败返回NULL。空source本身不导致拒绝。
 *
 * ## Constraints and lifecycle
 * Array传items、Map/Set传entries、内建array传自身、LinkedList传列表与首节点；来源标签/归属/节点语义由这些调用方保证。字段建立可达边，不是独立root handle或所有权转移。
 * 不记录长度/版本；后续来源变更可影响遍历。失败不回滚已分配实例、已写字段或已建立永久原型；分配器异常不一定经过NULL分支。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6153;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7387;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5194;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5207;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5221;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5223;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2821;tests/container/test_container_runtime.c:259;tests/container/test_container_runtime.c:301;tests/container/test_container_runtime.c:1290;tests/container/test_container_runtime.c:1632
 */
static SZrObject *zr_container_iterator_make(SZrState *state,
                                             SZrObject *source,
                                             EZrValueType sourceType,
                                             TZrInt64 indexValue,
                                             SZrObject *nextNode,
                                             FZrNativeFunction moveNextFunction) {
    SZrObject *iterator;
    SZrObjectPrototype *iteratorPrototype;

    if (state == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    iteratorPrototype = zr_container_iterator_runtime_prototype(state, moveNextFunction);
    if (iteratorPrototype == ZR_NULL) {
        return ZR_NULL;
    }

    iterator = ZrCore_Object_New(state, iteratorPrototype);
    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Object_Init(state, iterator);

    if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
        return ZR_NULL;
    }
    if (!zr_container_set_iterator_source_object_fast(state, iterator, source, sourceType) ||
        !zr_container_set_iterator_index_fast(state, iterator, indexValue) ||
        !zr_container_set_iterator_next_node_fast(state, iterator, nextNode)) {
        return ZR_NULL;
    }
    return iterator;
}

/**
 * ## Summary
 * 为两种native推进回调借用当前调用帧的接收者，遵守核心receiver放在functionBase+1的调用布局。
 *
 * ## Parameters
 * @param[in] state 当前native调用状态；须有有效可读的callInfo及receiver槽。
 *
 * ## Returns
 * @return 非空OBJECT/ARRAY接收值的借用对象，显式状态/帧/值标签拒绝返回NULL。
 *
 * ## Constraints and lifecycle
 * 先按值标签及非空指针显式拒绝；ZR_CAST_OBJECT的raw OBJECT/ARRAY种类只由调试断言核对，发布构建仍依赖调用方保证布局。这里不检查Enumerator原型、Iterator contract或owner类型。没有单独GC根；帧及对象字段维持可达性，不能把返回指针跨释放、改帧或未经核实的GC边界保存。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5433;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5245;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5254;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/include/zr_vm_core/conversion.h:65
 */
static SZrObject *zr_container_iterator_self(SZrState *state) {
    SZrCallInfo *callInfo;
    SZrTypeValue *selfValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = state->callInfoList;
    selfValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, selfValue->value.object);
}

/**
 * ## Summary
 * 把推进payload按native单结果ABI交回核心迭代消费层，让耗尽与调用帧结果数量分别表达。
 *
 * ## Parameters
 * @param[in,out] state 当前有效native帧，functionBase指向可写结果槽。
 * @param[in] ok 迭代payload；false既用于耗尽也用于本组回调的显式失败。
 *
 * ## Returns
 * @return 有效帧写入一项bool并返回结果数量1；缺state/callInfo返回0且不更新栈。C返回值不是payload成功布尔。
 *
 * ## Constraints and lifecycle
 * 调用后旧返回槽/stackTop布局已经改变；调用方不得继续依赖被覆盖的旧值。本函数不清threadStatus或制造异常，也不区分自然结束和字段操作失败。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5451;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5453;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5457;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5459;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5465;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5467;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5472;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5474;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5500;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5506;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5508;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5514;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5518;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5524;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5526;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5280;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:3429
 */
static TZrInt64 zr_container_iterator_finish_move_next(SZrState *state, TZrBool ok) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, ok, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

/**
 * ## Summary
 * 为数组backing推进借用实例保存的实际来源，避免把包装Array/Map/Set对象误当作可索引backing。
 *
 * ## Parameters
 * @param[in] state 有效所属域状态，用于类型转换及字段回退。
 * @param[in] iterator 带来源字段的实例；允许NULL。
 *
 * ## Returns
 * @return 缓存或自有字段中非空OBJECT/ARRAY的借用指针；空来源或不合适字段返回NULL。
 *
 * ## Constraints and lifecycle
 * 实例来源字段承担可达性，cached Pair与返回C地址不另建根。缓存为NULL值时直接视作空来源，其它非引用标签回退自有字段；不检查backing内部形状或所属域，不转移来源所有权。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5442;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5310;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5330;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2821;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6961
 */
static ZR_FORCE_INLINE SZrObject *zr_container_iterator_source_object_fast(SZrState *state, SZrObject *iterator) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }

    pair = iterator->cachedIteratorSourcePair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_CAST_OBJECT(state, value->value.object);
        }
        if (value->type == ZR_VALUE_TYPE_NULL) {
            return ZR_NULL;
        }
    }

    return zr_container_get_object_field_fast(state, iterator, kContainerSourceField);
}

/**
 * ## Summary
 * 为LinkedList推进借用实例持有的当前节点游标，使游标生存期与实例字段关联。
 *
 * ## Parameters
 * @param[in] state 有效所属域状态，用于类型转换和字段读取。
 * @param[in] iterator 带nextNode字段的实例；允许NULL。
 *
 * ## Returns
 * @return 非空OBJECT/ARRAY游标的借用对象；空、缺失或非引用字段返回NULL，推进层将其作为结束。
 *
 * ## Constraints and lifecycle
 * 不证明游标为LinkedNode，不另建root；缓存空值不再回退，其他不适合的缓存标签回退自有字段。正确来源和游标由构造/推进写入保持可达，摘链或修改next会影响后续遍历。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5503;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5350;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5370;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5223;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5523;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7391
 */
static ZR_FORCE_INLINE SZrObject *zr_container_iterator_next_node_object_fast(SZrState *state, SZrObject *iterator) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }

    pair = iterator->cachedIteratorNextNodePair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_CAST_OBJECT(state, value->value.object);
        }
        if (value->type == ZR_VALUE_TYPE_NULL) {
            return ZR_NULL;
        }
    }

    return zr_container_get_object_field_fast(state, iterator, kContainerNextNodeField);
}

/**
 * ## Summary
 * 为数组推进取得实例的整数位置，在字段缺失或标签异常时保留调用方指定的起始策略。
 *
 * ## Parameters
 * @param[in] state 有效所属域状态，供字段回退读取。
 * @param[in] iterator 实例；允许NULL。
 * @param[in] defaultValue 未取得整数时使用的值，当前调用传0。
 *
 * ## Returns
 * @return 整数转换成TZrInt64的结果，否则defaultValue；读取成功不代表下标可用于当前backing。
 *
 * ## Constraints and lifecycle
 * 不验证负值、长度或backing版本；无符号值直接转TZrInt64，不做范围拒绝。TODO: 沿guest隐藏字段访问/恢复入口核对大于INT64范围的UINT游标是否可达，不能由默认0推导任意外部字段安全。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5443;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5391;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5404;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5407;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5412
 */
static ZR_FORCE_INLINE TZrInt64 zr_container_iterator_index_fast(SZrState *state,
                                                                 SZrObject *iterator,
                                                                 TZrInt64 defaultValue) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return defaultValue;
    }

    pair = iterator->cachedIteratorIndexPair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
            return value->value.nativeObject.nativeInt64;
        }
        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
            return (TZrInt64)value->value.nativeObject.nativeUInt64;
        }
    }

    return zr_container_get_int_field(state, iterator, kContainerIndexField, defaultValue);
}

/**
 * ## Summary
 * 为Array、内建array及Map/Set数组backing提供下一项或终态，按当前来源读取而非遍历创建时的集合快照。
 *
 * ## Parameters
 * @param[in,out] state 当前native调用状态，receiver由functionBase+1提供。
 *
 * ## Returns
 * @return 有效帧返回数量1，bool true表示current与下一游标已发布；false可能是耗尽、无效接收/来源或读写失败。无帧返回0，不把payload当作C返回值。
 *
 * ## Constraints and lifecycle
 * raw-int表示可避免物化元素；其余元素经既有ownership-aware current字段setter发布；这里不额外定义对象复制策略。guest null元素的值指针仍存在，能够yield true；来源为空或游标为负可在调用Array_Get前结束；Array_Get返回NULL指针也进入无元素分支。
 * 每步取当前长度和值；自然结束尝试清current。字段写失败不回滚已发布current，也不保证清空；外层另看threadStatus。核心cached-array快路可在指令层直接推进；命中时绕过本回调及Object_IterMoveNext。它与后备Object路径共用数组游标helper，需按两入口核对语义。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1763;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1777;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6107;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6153;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6158;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6452;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6966;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5432;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5449;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5455;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5462;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5470;zr_vm_core/src/zr_vm_core/object/object.c:376;zr_vm_core/src/zr_vm_core/object/object.c:4107;zr_vm_core/src/zr_vm_core/object/object.c:4112;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:3417;tests/container/test_container_runtime.c:259;tests/container/test_container_runtime.c:545;tests/container/test_container_runtime.c:1290;tests/container/test_container_runtime.c:1632
 */
static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *source;
    TZrInt64 index;
    const SZrTypeValue *current;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    source = zr_container_iterator_source_object_fast(state, iterator);
    index = zr_container_iterator_index_fast(state, iterator, 0);
    if (source != ZR_NULL &&
        source->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        source->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
        source->superArrayRawIntData != ZR_NULL &&
        source->superArrayRawIntLength <= source->superArrayRawIntCapacity) {
        if (index < 0 || (TZrUInt64)index >= (TZrUInt64)source->superArrayRawIntLength) {
            if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
                return zr_container_iterator_finish_move_next(state, ZR_FALSE);
            }
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        if (!zr_container_set_iterator_current_int_fast(state, iterator, source->superArrayRawIntData[(TZrSize)index]) ||
            !zr_container_set_iterator_index_fast(state, iterator, index + 1)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_TRUE);
    }

    current = (source != ZR_NULL && index >= 0) ? ZrLib_Array_Get(state, source, (TZrSize)index) : ZR_NULL;
    if (current == ZR_NULL) {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    if (!zr_container_set_iterator_current_value_fast(state, iterator, current) ||
        !zr_container_set_iterator_index_fast(state, iterator, index + 1)) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

/**
 * ## Summary
 * 为LinkedList迭代协议产出节点载荷，并按当前节点链接继续；迭代器字段维持列表与游标可达。
 *
 * ## Parameters
 * @param[in,out] state 当前native调用状态，receiver由functionBase+1提供。
 *
 * ## Returns
 * @return 有效帧返回一项bool；有节点且current/游标发布成功为true，空游标、接收拒绝或字段失败为false；无帧返回数量0。
 *
 * ## Constraints and lifecycle
 * 不读取source或count作遍历边界；缺失value与guest null值都可产出NULL current且true。后续访问当时的next，没有版本快照或环检测；current已写而游标失败不回滚。
 * 借用节点不建立独立GC根；不要从列表count或初始first保证固定元素序列。空表guest fixture已覆盖重复空遍历，所引协议用例另确认Iterable元数据。TODO: 当前所查测试尚缺非空LinkedList推进、遍历中修改来源或迭代器单独保活来源的GC验收，需补查这些契约边界。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1765;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7387;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7392;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5494;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5503;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5511;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5523;zr_vm_core/src/zr_vm_core/object/object.c:4112;tests/iterator/test_enumerator_protocol.c:131;tests/container/test_container_runtime.c:2379;tests/container/test_container_runtime.c:2390;tests/fixtures/reference/core_semantics/protocols_iteration_comparable/empty_iterator_boundary.zr:13
 */
static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *node;
    const SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    node = zr_container_iterator_next_node_object_fast(state, iterator);
    if (node == ZR_NULL) {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    value = zr_container_get_own_field_value_fast(state, node, kContainerValueField);
    if (value != ZR_NULL) {
        if (!zr_container_set_iterator_current_value_fast(state, iterator, value)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
    } else {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
    }
    if (!zr_container_set_iterator_next_node_fast(state,
                                                  iterator,
                                                  zr_container_get_object_field_fast(state, node, kContainerNextField))) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

/**
 * ## Summary
 * 为Pair的零参/双参构造建立字段初始化与结果发布边界；不把描述符允许范围等同于实际支持的每个arity。
 *
 * ## Parameters
 * @param[in,out] context 活动构造上下文；零参初始化两字段为null，双参借用first/second；回调不另验K/V类型。
 * @param[out] result dispatcher初始化的空输出槽，按对象结果发布协议接收。
 *
 * ## Returns
 * @return 回调发布对象结果时true；目标解析或受检字段失败时false。一个参数调用不返回的RaiseArityError；线程错误由外层独立处理。
 *
 * ## Constraints and lifecycle
 * 注册constructor声明0..2，但实现只支持0或2。构造目标可复用兼容self或新建；Pair内部STRUCT、外部OBJECT标签不等于共享身份。第二字段失败不回滚第一字段，也不释放或转移输入owner；正常分配/复制限制由Core与调用frame承担。TODO: 核单参数负面测试及字段写入失败后的构造状态、临时根清理，不把Array/Map/Set的其他构造风险直接推广。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7608;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5546;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5552;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5556;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5559;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5562;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5567;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5574;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5582;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1907;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1983;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2010;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2128;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7733;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1223;tests/container/test_container_runtime.c:1794;tests/container/test_container_runtime.c:1796;tests/container/test_container_runtime.c:2013;tests/container/test_container_runtime.c:2023;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_pair_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *pair;
    TZrSize argc;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    argc = ZrLib_CallContext_ArgumentCount(context);
    if (argc != 0 && argc != 2) {
        ZrLib_CallContext_RaiseArityError(context, 0, 2);
    }

    pair = zr_container_resolve_construct_target(context);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argc == 2) {
        if (!zr_container_set_value_field_fast(context->state,
                                               pair,
                                               kContainerPairFirstField,
                                               ZrLib_CallContext_Argument(context, 0)) ||
            !zr_container_set_value_field_fast(context->state,
                                               pair,
                                               kContainerPairSecondField,
                                               ZrLib_CallContext_Argument(context, 1))) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_null_field_fast(context->state, pair, kContainerPairFirstField) ||
            !zr_container_set_null_field_fast(context->state, pair, kContainerPairSecondField)) {
            return ZR_FALSE;
        }
    }

    return zr_container_finish_object(context, result, pair);
}

/**
 * ## Summary
 * 为Pair相等协议比较两个分量；other不是非空OBJECT或不属于注册owner时给出不相等结果。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。 业务实参other由context借用取得；NULL槽拒绝，仅非空OBJECT进入owner核验，ARRAY/其它标签或空payload给bool false结果。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 参数/输出入口拒绝时false；合法比较或other拒绝时写bool并回调true。bool false结果不是回调失败，用户equals错误由dispatcher检测。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 other经owner原型链检查，但receiver只经self对象标签筛选；调用者不能据此推断对双方泛型/字段布局均已验证。两个对象的四个字段借用在用户equals调用前取得，相等协议可重入修改或分配。TODO: 核用户equals改变Pair/backing或触发GC时的字段借用生存边界和比较一致性，当前不授快照或移动GC缺陷信用。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7596;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5602;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5612;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5617;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5621;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5626;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5627;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5630;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5631;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1870;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3181;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3725;tests/container/test_container_runtime.c:1798;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5615
 */
static TZrBool zr_container_pair_equals(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    const SZrTypeValue *selfFirst;
    const SZrTypeValue *selfSecond;
    const SZrTypeValue *otherFirst;
    const SZrTypeValue *otherSecond;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    if (!zr_container_object_is_owner_instance(context, other)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    selfFirst = zr_container_pair_get_first_fast(context->state, self);
    selfSecond = zr_container_pair_get_second_fast(context->state, self);
    otherFirst = zr_container_pair_get_first_fast(context->state, other);
    otherSecond = zr_container_pair_get_second_fast(context->state, other);
    zr_container_result_set_bool_fast(result,
                                      zr_container_values_equal(context->state, selfFirst, otherFirst) &&
                                              zr_container_values_equal(context->state, selfSecond, otherSecond));
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Pair的compareTo与COMPARE共用字典序比较入口；other为非空OBJECT时先比较first，相等再比较second，其它标签给比较值1。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。 业务实参other由context借用取得；NULL槽拒绝，仅非空OBJECT进入字段比较，ARRAY/其它标签或空payload给int64(1)结果。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 输入入口拒绝时false；写比较整数时回调true。结果不独立保证线程FINE、跨运行稳定或严格全序。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 other转对象后没有equals入口的owner检查，也不核K/V形状；底层可调用用户equals/compareTo后使用哈希/类型兜底。BUG: 合法Pair数值分量比较仍经过已记录values_compare::numeric-order的double精度与unsigned排序缺陷；此入口引用既有问题，不增加新问题身份。TODO: 沿用户比较重入核分量借用、形状限制与顺序一致性。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7599;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7610;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5653;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5663;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5668;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5673;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5675;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5678;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3994;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4034;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4037;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4038;tests/container/test_container_runtime.c:1799;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_pair_compare(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    TZrInt64 compare;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        zr_container_result_set_int_fast(result, 1);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    compare = zr_container_values_compare(context->state,
                                          zr_container_pair_get_first_fast(context->state, self),
                                          zr_container_pair_get_first_fast(context->state, other));
    if (compare == 0) {
        compare = zr_container_values_compare(context->state,
                                              zr_container_pair_get_second_fast(context->state, self),
                                              zr_container_pair_get_second_fast(context->state, other));
    }
    zr_container_result_set_int_fast(result, compare);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Pair的Hashable协议组合分量哈希；与Pair相等入口共同要求分量保持等值兼容的哈希关系。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 入口拒绝时false；写入混合整数时回调true。用户hashCode已设置的线程错误不会由本回调清除，外层独立拒绝结果。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 对象分量可执行用户hashCode；失败或非整数结果可退到Core哈希，不能把fallback当作用户协议成功。receiver只筛OBJECT/ARRAY标签，分量逐次借用；不承诺回调之间字段不变或哈希跨运行稳定。TODO: 核用户hashCode错误/重入、字段变化与哈希一致性及借用保护。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7602;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5698;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5705;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5709;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5710;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5711;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5712;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3181;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3213;tests/container/test_container_runtime.c:1800;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_pair_hash_code(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    TZrUInt64 firstHash;
    TZrUInt64 secondHash;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL) {
        return ZR_FALSE;
    }

    firstHash = zr_container_value_hash(context->state, zr_container_pair_get_first_fast(context->state, self));
    secondHash = zr_container_value_hash(context->state, zr_container_pair_get_second_fast(context->state, self));
    zr_container_result_set_int_fast(result,
                                     (TZrInt64)((firstHash * ZR_CONTAINER_HASH_MIX_PRIME) ^
                                                (secondHash + ZR_CONTAINER_HASH_MIX_OFFSET)));
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Array<T>构造元方法建立公开长度/容量与私有backing的对应关系，完成初始化后发布实例。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；允许无receiver；元数据接受0..1个实参，实参实际为capacity。
 * @param[out] result 派发层提供、调用期间有效的结果槽；仅成功初始化后写入构造对象，不保留槽指针。
 *
 * ## Returns
 * @return 初始化及结果发布成功true；普通可恢复的参数、backing或字段失败false。负capacity调用noreturn的ZrCore_Debug_RunError。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 上下文须来自已注册Array构造元方法；resolver选择或新建目标，失败没有回滚保证。T无附加泛型约束。TODO: 元数据把capacity实参命名为index，需核命名实参和签名帮助；TZrSize窄化、预约上界及items有效期沿prepare_backing_storage核实。引用resolve_construct_target既有条件性BUG：已注册Array.__constructor可经公开CallValue(receiver=NULL,arguments=NULL,count=0)调用，新target在后续指定GcFull链中未于finish_object前独立root；完整GC条件沿helper记录，不扩大为所有guest构造或GC模式。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参及存在的receiver由调用帧保持有效；构造目标沿resolver选择/创建契约。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7493;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5732;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5748;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5752;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5761;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7727;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_core/include/zr_vm_core/debug.h:418;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *arrayObject = zr_container_resolve_construct_target(context);
    TZrInt64 capacity = 0;
    SZrObject *items;

    if (arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLib_CallContext_ArgumentCount(context) == 1 && !ZrLib_CallContext_ReadInt(context, 0, &capacity)) {
        return ZR_FALSE;
    }
    if (capacity < 0) {
        ZrCore_Debug_RunError(context->state, "Array capacity must be non-negative");
    }

    items = ZrLib_Array_New(context->state);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_array_prepare_backing_storage(context->state, items, capacity > 0 ? (TZrSize)capacity : 0)) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, arrayObject, kContainerItemsField, items) ||
        !zr_container_set_int_field_fast(context->state, arrayObject, kContainerLengthField, 0) ||
        !zr_container_set_int_field_fast(context->state, arrayObject, kContainerCapacityField, capacity)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, arrayObject);
}

/**
 * ## Summary
 * 提供Array追加入口，将backing增长与公开length更新衔接。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Array实例方法，恰一个value:T实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功写null，不保留槽指针。
 *
 * ## Returns
 * @return 追加及length写入都成功true；检查或helper普通失败false。最后length写失败时元素可能已追加，不回滚。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。T无附加泛型约束；值复制和owner屏障由Core存储路径承担。描述符没有READONLY_RECEIVER。容量增长、dense PairPool快路及通用push沿对应helper契约；TODO: 核长度加一和容量倍增的可表示上界。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7467;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5791;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5798;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4197;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4260;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5111;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    value = ZrLib_CallContext_Argument(context, 0);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(items);
    if (value == ZR_NULL || !zr_container_array_ensure_capacity(context->state, self, length + 1)) {
        return ZR_FALSE;
    }
    if (!zr_container_storage_push_gc_value_dense_pair_pool_fast(context->state, items, value) &&
        !zr_container_storage_push(context->state, items, value)) {
        return ZR_FALSE;
    }
    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Array提供0..length范围的插入入口，并维护公开长度。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Array实例方法，恰index:int和value:T两个实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功写null，不保留槽指针。
 *
 * ## Returns
 * @return 容量、存储插入及length发布成功true；普通可恢复helper失败false。越界位置调用noreturn的Debug_RunError。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。容量可能在后续失败前增长；没有事务回滚。TODO: 沿storage_insert核generic位移失败传播和部分修改；length写失败不撤销插入。描述符没有READONLY_RECEIVER。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7470;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5830;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5842;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5845;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4410;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5111;zr_vm_core/include/zr_vm_core/debug.h:418;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_insert(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    length = zr_container_array_length_fast(items);
    if (indexValue < 0 || (TZrSize)indexValue > length) {
        ZrCore_Debug_RunError(context->state, "Array.insert index out of range");
    }
    if (!zr_container_array_ensure_capacity(context->state, self, length + 1) ||
        !zr_container_storage_insert(context->state, items, (TZrSize)indexValue, ZrLib_CallContext_Argument(context, 1)) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Array提供现存位置删除入口，把公开length同步到实际backing。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Array实例方法，恰一个index:int实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；删除及length更新成功写null，不保留槽指针。
 *
 * ## Returns
 * @return 删除及length发布成功true；最后length写失败false。负索引或storage_remove_at返回false均进入noreturn的out-of-range RunError。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。TODO: 当前条件合并底层存储失败与越界，应沿storage_remove_at和错误派发区分原因；不能断言该false一定是越界。Shared删除既有BUG属于storage_remove_at；删除后的length失败不回滚。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7473;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5872;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4463;zr_vm_core/include/zr_vm_core/debug.h:418;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_remove_at(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (indexValue < 0 || !zr_container_storage_remove_at(context->state, items, (TZrSize)indexValue)) {
        ZrCore_Debug_RunError(context->state, "Array.removeAt index out of range");
    }

    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)zr_container_array_length_fast(items))) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Array提供清空并复用backing的入口，保留公开capacity。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；无参Array实例方法。
 * @param[out] result 派发层提供、调用期间有效的结果槽；清空和length发布成功写null，不保留槽指针。
 *
 * ## Returns
 * @return backing准备及length写成功true；普通准备或字段失败false，清空后的length失败不回滚。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。清理由array_clear_items_reuse_storage承担，本wrapper不逐槽析构。引用该helper既有Shared owner/非池Pair丢失静态BUG与TODO；不建立重复缺陷身份，也不承诺失败时元素未改变。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7476;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5918;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5923;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5924;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3539;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3502;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(items);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 提供Array.indexOf首个匹配查询，未命中返回-1。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Array实例方法，恰一个value:T实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；接收int下标或-1，不保留槽指针。
 *
 * ## Returns
 * @return 命中或普通扫描结束时写查询结果并true；无效context/self/result时false。用户比较可能抛错或留下线程错误，外围派发结合线程状态处理。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。循环保存开始时的scalar length并以它为界，不复制元素或建立一致性/版本快照；raw-int分支不调用对象equals。TODO: equals可执行用户代码，重入后没有重新取得backing、needle或当前借用槽，应核有效期及修改后观察行为；现有证据不证明UAF。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7481;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5948;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5958;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5960;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5962;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5979;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5990;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3483;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_index_of(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = zr_container_array_length_fast(items);
    if (needle != ZR_NULL && ZR_VALUE_IS_TYPE_INT(needle->type)) {
        if (zr_container_array_raw_int_active(items)) {
            TZrInt64 signedNeedle;

            if (zr_container_try_read_int64_needle_fast(needle, &signedNeedle)) {
                for (TZrSize index = 0; index < length; index++) {
                    if (items->superArrayRawIntData[index] == signedNeedle) {
                        zr_container_result_set_int_fast(result, (TZrInt64)index);
                        return ZR_TRUE;
                    }
                }
            }

            zr_container_result_set_int_fast(result, -1);
            return ZR_TRUE;
        }

        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (zr_container_int_values_equal_fast(candidate, needle)) {
                zr_container_result_set_int_fast(result, (TZrInt64)index);
                return ZR_TRUE;
            }
        }
    } else {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
                zr_container_result_set_int_fast(result, (TZrInt64)index);
                return ZR_TRUE;
            }
        }
    }

    zr_container_result_set_int_fast(result, -1);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 提供Array.contains成员查询。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Array实例方法，恰一个value:T实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；接收bool存在性结果，不保留槽指针。
 *
 * ## Returns
 * @return 命中或普通扫描结束时写查询结果并true；无效context/self/result时false。用户比较可能抛错或留下线程错误，外围派发结合线程状态处理。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。循环保存开始时的scalar length并以它为界，不复制元素或建立一致性/版本快照；raw-int分支不调用对象equals。TODO: equals可执行用户代码，重入后没有重新取得backing、needle或当前借用槽，应核有效期及修改后观察行为；现有证据不证明UAF。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7478;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6018;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6033;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6035;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6036;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6064;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3483;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_container_self_object(context);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = zr_container_array_length_fast(items);
    if (zr_container_array_raw_int_active(items)) {
        TZrInt64 signedNeedle;

        if (zr_container_try_read_int64_needle_fast(needle, &signedNeedle)) {
            for (TZrSize index = 0; index < length; index++) {
                if (items->superArrayRawIntData[index] == signedNeedle) {
                    zr_container_result_set_bool_fast(result, ZR_TRUE);
                    return ZR_TRUE;
                }
            }
        }
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (needle != ZR_NULL && ZR_VALUE_IS_TYPE_INT(needle->type)) {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (zr_container_int_values_equal_fast(candidate, needle)) {
                zr_container_result_set_bool_fast(result, ZR_TRUE);
                return ZR_TRUE;
            }
        }
    } else {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
                zr_container_result_set_bool_fast(result, ZR_TRUE);
                return ZR_TRUE;
            }
        }
    }

    zr_container_result_set_bool_fast(result, ZR_FALSE);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 作为Array的ITERABLE_INIT入口，为Iterable<T>消费者创建backing Enumerator。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；无参Array实例方法，receiver为帧内借用值。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功接收Enumerator对象，不保留槽指针。
 *
 * ## Returns
 * @return iterator创建并finish_object发布成功true；检查、创建或发布普通失败false。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。iterator.source引用隐藏items，从index0推进，不复制元素或保存长度/版本快照。字段初始化失败不保证回滚；source保活、current复制及修改可见性沿iterator_make/Array iterator契约。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7484;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6092;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6101;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6108;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5194;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          items,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/**
 * ## Summary
 * 为VM内建ARRAY原型接入本模块native getIterator协议。
 *
 * ## Parameters
 * @param[in,out] state 当前native调用的有效状态；读取callInfo/栈槽并更新结果和stackTop。
 *
 * ## Returns
 * @return state或callInfo缺失返回0；存在帧时报告一项结果并返回1，包括receiver无效或iterator分配失败而写null的分支。
 *
 * ## Constraints and lifecycle
 * 只能按有效native closure调用帧的functionBase布局调用，结果/self槽及payload须在调用期间有效并满足当前执行域资格。本函数只筛ARRAY标签和非空payload，不验证业务布局。成功iterator引用原数组，无长度快照；source/current生命周期沿iterator helper。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7888;zr_vm_core/src/zr_vm_core/function/function.c:5004;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6127;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6135;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6141;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6148;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6153;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6162;zr_vm_core/src/zr_vm_core/function/function.c:5113
 */
static TZrInt64 zr_container_native_array_get_iterator_native(SZrState *state) {
    SZrCallInfo *callInfo;
    TZrStackValuePointer base;
    SZrTypeValue *resultValue;
    SZrTypeValue *selfValue;
    SZrObject *self;
    SZrObject *iterator;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    base = callInfo->functionBase.valuePointer;
    resultValue = ZrCore_Stack_GetValue(base);
    selfValue = ZrCore_Stack_GetValue(base + 1);
    if (resultValue == ZR_NULL || selfValue == ZR_NULL ||
        selfValue->type != ZR_VALUE_TYPE_ARRAY || selfValue->value.object == ZR_NULL) {
        if (resultValue != ZR_NULL) {
            zr_container_result_set_null_fast(resultValue);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    iterator = zr_container_iterator_make(state,
                                          self,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    if (iterator == ZR_NULL) {
        zr_container_result_set_null_fast(resultValue);
    } else {
        zr_container_value_set_object_fast(state, resultValue, iterator, ZR_VALUE_TYPE_OBJECT);
    }

    state->stackTop.valuePointer = base + 1;
    return 1;
}

/**
 * ## Summary
 * 提供Array只读下标取值入口，按Core值复制契约交付元素。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；GET_ITEM元方法，恰一个index:int实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；接收null、整数或复制的元素，不保留槽指针。
 *
 * ## Returns
 * @return Invalid indices raise a runtime bounds error. Valid indices copy the element.
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。READONLY_RECEIVER决定绑定receiver派发角色，不禁止ensure_items_array_fast维护隐藏backing。普通元素是短期借用输入，结果经Core Value copy保留ownership语义，不跨调用保留槽指针。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7495;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6186;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6196;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6200;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6204;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3955;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7497;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (indexValue < 0 || (TZrUInt64)indexValue >= (TZrUInt64)zr_container_array_length_fast(items)) {
        ZrCore_Debug_RunError(context->state, "Array index out of range");
    }
    if (zr_container_array_raw_int_active(items)) {
        zr_container_result_set_int_fast(result, items->superArrayRawIntData[(TZrSize)indexValue]);
        return ZR_TRUE;
    }

    value = zr_container_array_get_value_fast(context->state, items, (TZrSize)indexValue);
    return zr_container_result_copy_no_profile(context->state, result, value);
}

/**
 * ## Summary
 * 提供Array现存下标赋值入口，不扩展公开长度。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；SET_ITEM元方法，恰index:int和value:T两个实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功接收赋值值，不保留槽指针。
 *
 * ## Returns
 * @return 越界调用noreturn的Debug_RunError；raw-int写标量后true；普通路径先调用Core setter，再返回result_copy_no_profile状态。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 调用要求有效的同域Array实例及正确私有backing不变量；本wrapper筛接收标签，不验证Array原型、泛型或完整内部布局。实参槽及结果槽须由调用帧保持有效。descriptor未设READONLY_RECEIVER。raw-int快路限当前raw表示活跃且value为signed-int，直接更新buffer及storageGeneration；其他值由Core setter承担资格、复制和屏障。最后结果复制失败不回滚已写元素。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7499;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6234;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6250;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6259;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6261;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6267;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6269;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6272;zr_vm_core/src/zr_vm_core/object/object_super_array.c:1943;zr_vm_core/include/zr_vm_core/debug.h:418;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_array_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    const SZrTypeValue *value;
    SZrTypeValue receiverValue;
    SZrTypeValue key;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (indexValue < 0 || (TZrSize)indexValue >= zr_container_array_length_fast(items)) {
        ZrCore_Debug_RunError(context->state, "Array index out of range");
    }

    value = ZrLib_CallContext_Argument(context, 1);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(items) && ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        items->superArrayRawIntData[(TZrSize)indexValue] = value->value.nativeObject.nativeInt64;
        items->superArrayStorageGeneration++;
        items->superArrayRawIntDirty = ZR_FALSE;
        zr_container_result_set_int_fast(result, value->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    zr_container_array_make_receiver_value(context->state, self, &receiverValue);
    ZrCore_Value_InitAsInt(context->state, &key, indexValue);
    if (!ZrCore_Object_SuperArraySetInt(context->state, &receiverValue, &key, value)) {
        return ZR_FALSE;
    }
    return zr_container_result_copy_no_profile(context->state, result, value);
}

/**
 * ## Summary
 * 为Map构造元方法建立entries与公开count的一致初始状态并发布实例。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；零参构造，可没有接收对象。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功发布Map实例，不保留槽指针。
 *
 * ## Returns
 * @return entries建立、字段初始化及finish_object普通成功true；普通分配/写字段失败false。分配异常可非局部退出，不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 零参构造，上下文来自已注册类型的构造元方法，合法调用可无Self，resolver选择/新建target；target及新backing须满足当前执行域资格。字段初始化或结果发布失败无回滚保证。引用resolve_construct_target既有条件性公开C API构造期GC BUG，不复制新缺陷身份；TODO: 中间backing/target有效期沿resolver和字段发布helper核查。Map的K声明IHashable/IEquatable<K>，V无附加约束。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参及存在的receiver由调用帧保持有效；构造目标沿resolver选择/创建契约。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7526;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6292;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1907;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6293;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6294;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6300;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1429;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_map_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, self, kContainerEntriesField, entries) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

/**
 * ## Summary
 * 为Map提供实际backing的成员查询，交付guest bool结果。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Map实例方法，恰一个key:K实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；写guest bool，不保留槽指针。
 *
 * ## Returns
 * @return 普通查询写搜索bool并native true；guest false也可源于helper输入/搜索失败，native false属于回调层失败。用户回调非局部退出不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Map对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。ensure_entries可能建立backing；搜索使用实际长度而非count，hash泛型约束不承诺按桶O(1)查询。TODO: helper false不区分缺项与搜索错误，外围须结合threadStatus；重入与既有cache问题沿find helper记录。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7509;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6324;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6334;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4784;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7444;tests/container/test_container_runtime.c:1558;tests/container/test_container_runtime.c:1580;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_map_contains_key(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrBool found;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    found = zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL, ZR_NULL);
    zr_container_result_set_bool_fast(result, found);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Map删除首个匹配项，在storage成功后将count同步到实际长度。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Map实例方法，恰一个key:K实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；普通返回写guest删除bool，不保留槽指针。
 *
 * ## Returns
 * @return 查找/storage普通false写guest false并native true；移除及count更新成功写guest true并native true；最后count失败返回native false，结果与删除不保证回滚。用户回调可非局部退出。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Map对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。TODO: guest false合并缺项与底层失败，不证明backing未改变；成功删除后的count失败也没有事务回滚。owner释放/通用位移已审问题沿storage_remove_at记录，不在wrapper重复BUG。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7512;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6356;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6366;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6367;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4463;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4458;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_map_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (!zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index, ZR_NULL) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Map清空并复用entries backing，将公开count置零。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；无参Map实例方法。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功写null，不保留槽指针。
 *
 * ## Returns
 * @return entries准备及count发布普通成功true；ensure/count普通失败false，count失败可发生在清空之后。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Map对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。清理由array_clear_items_reuse_storage承担，引用其既有owner BUG/TODO，不重复计数。TODO: 清空后的count失败不回滚，需核上层如何观察部分状态。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7515;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6399;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6407;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6412;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6416;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3539;tests/container/test_container_runtime.c:1558;tests/container/test_container_runtime.c:1580;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_map_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(entries);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 作为Map的ITERABLE_INIT入口，为Iterable<Pair<K,V>>消费者创建entries Enumerator。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；无参Map实例方法。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功接收Enumerator，不保留槽指针。
 *
 * ## Returns
 * @return iterator建立及finish_object普通成功true；否则false。分配异常可能非局部退出，不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Map对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。source引用entries，不复制内容或建立长度/版本快照；后续推进读live backing。current遵循Array iterator/Value_Copy契约，Pair为STRUCT值复制而非保证别名。TODO: backing修改期间推进与借用有效期沿iterator组核查；本wrapper不另建source root。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7517;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_core/src/zr_vm_core/object/object.c:4025;zr_vm_core/src/zr_vm_core/object/object.c:4045;zr_vm_core/src/zr_vm_core/module/module_prototype.c:505;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1258;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6437;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5194;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5432;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7427;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_foreach.c:360;zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_foreach.c:380;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:3394;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:8926;tests/container/test_container_runtime.c:1290;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_map_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/**
 * ## Summary
 * 提供普通Map GET的结果边界，命中按Core值复制规则取得second，缺键交付null；只读inline GET另行实现相同缺键结果约定。
 *
 * ## Parameters
 * @param[in,out] state 有效同域执行状态，ensure存储和查找回调可分配或置错误。
 * @param[in] selfValue 存活同域Map接收者槽；本层只核OBJECT/ARRAY标签和非空载荷，不验业务原型。
 * @param[in] keyValue caller提供的有效键；当前普通GET从实参槽取得，本函数不独立验非空或K泛型约束。
 * @param[out] result dispatcher新初始化的普通结果槽；不可把尚持有owner的旧槽直接当此契约。
 *
 * ## Returns
 * @return 输入/对象拒绝为false；entry为NULL时写null并返回true，这还可能覆盖ensure/查找失败，不能用true证明线程正常。命中返回second结果复制的局部bool；抛异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 普通GET会按需确保backing，readonly inline只读已有backing，缺backing时false由外层FINE派发转null，并不调用本核心。K语义由语言类型契约与合法caller保证，helper不做独立运行时泛型验证。哈希/相等可执行用户代码，entries、Pair和second是借用，复制与回调不能保证成员快照；线程异常由外层dispatch额外检查。TODO: 对照hash/equals重入的backing修改及成员重取协议，核查借用槽有效性；同步普通ensure失败与缺键的可观察区分。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6527;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6484;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6489;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6494;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6496;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6497;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6498;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6501;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6502;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6522;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6552;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6572;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6573;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6576;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7527;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7542;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:169;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:170;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:171;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:172;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:173;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:174;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:176;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:614;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:618;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:623;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:668
 */
static ZR_FORCE_INLINE TZrBool zr_container_map_get_item_core(SZrState *state,
                                                              const SZrTypeValue *selfValue,
                                                              const SZrTypeValue *keyValue,
                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entryObject;
    SZrObject *entries;
    const SZrTypeValue *mappedValue;

    if (state == ZR_NULL || selfValue == ZR_NULL || result == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(state, self);
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    mappedValue = zr_container_map_entry_get_second_value_fast(state, entryObject);
    return zr_container_result_copy_no_profile(state, result, mappedValue);
}

/**
 * ## Summary
 * 将Map普通GET_ITEM调用转交读取核心，统一entry查找结果及值复制契约。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Map GET_ITEM，恰一个key:K实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；接收entry为空时的null或按Core规则复制的Pair.second，不保留槽指针。
 *
 * ## Returns
 * @return 透传map_get_item_core状态；entry为NULL时统一写null并返回true，涵盖未命中及entries准备/搜索返回NULL的其他失败；命中后值复制失败可返回false。用户比较非局部退出不由bool表示。普通dispatcher在回调false且threadStatus为FINE时将guest结果归为null，非FINE沿线程错误处理；本helper的true/null分支不依赖该false转换。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Map对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。backing建立、Pair.second借用与Value_Copy由core helper负责；false不证明无前序字段/cache副作用。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。 TODO: entry的NULL不单独证明未命中；结合threadStatus核entries准备/搜索失败的合法输入可达性与是否需区分。普通dispatcher仍最终检查线程错误，FINE状态下guest null无法区别这些原因。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7531;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6522;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6527;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6475;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6501;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6502;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7403;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6495;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4933;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4953;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5000
 */
static TZrBool zr_container_map_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_map_get_item_core(context->state,
                                          ZrLib_CallContext_Self(context),
                                          ZrLib_CallContext_Argument(context, 0),
                                          result);
}

/**
 * ## Summary
 * 为Map提供既存backing的inline GET，将entry查找结果转换为复制值或null，并由Core检查最终线程状态。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域有效状态。
 * @param[in] selfValue caller保持有效的Map接收槽，编码须为OBJECT或ARRAY。
 * @param[in] keyValue 调用期间有效且满足Core owner/域资格的借用键槽。
 * @param[out] result 派发层提供、调用期间有效的结果槽；按Core规则复制Pair.second或写null，不保留槽指针。
 *
 * ## Returns
 * @return find_entry返回NULL时统一写null并true，涵盖未命中及helper的NULL拒绝/字符串转换失败等路径；命中时透传值复制状态。无backing、无效参数/receiver或复制失败可false，由Core结合threadStatus解释。用户比较非局部退出不由bool表示。
 *
 * ## Constraints and lifecycle
 * receiver及私有backing布局由caller保证，本函数不验证完整原型/泛型。cached getter不创建backing，输入/result槽由caller保持有效。Core inline GET在false且threadStatus为FINE时写null并视为成功；非FINE向外失败，没有普通GET callback回退。 entry NULL在本函数内返回true/null，不是Core对callback false+FINE的转换；若比较留下非FINE线程状态，Core最终仍拒绝结果，FINE时guest null无法区分上述NULL原因。TODO: 按合法K输入约束核helper非miss NULL路径的可达性及是否需区分。cached backing缺失在此前直接返回false，须与entry NULL的true分支分开。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7542;zr_vm_core/src/zr_vm_core/object/object.c:963;zr_vm_core/src/zr_vm_core/object/object.c:1112;zr_vm_core/src/zr_vm_core/object/object.c:1163;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:169;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6552;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6571;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6575;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6581;zr_vm_core/src/zr_vm_core/object/object.c:923;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:176;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6576;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4933;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4953;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5000
 */
static ZR_FORCE_INLINE TZrBool zr_container_map_get_item_readonly_inline_fast(SZrState *state,
                                                                              const SZrTypeValue *selfValue,
                                                                              const SZrTypeValue *keyValue,
                                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject;
    const SZrTypeValue *mappedValue;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || result == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_get_entries_array_cached_fast(state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        return ZR_TRUE;
    }

    mappedValue = zr_container_map_entry_get_second_value_fast(state, entryObject);
    return zr_container_result_copy_no_profile(state, result, mappedValue);
}

/**
 * ## Summary
 * 统一Map普通SET与无结果inline新增回退的写入约定；覆盖现存项或构造并发布新Pair，只有新增后同步count。
 *
 * ## Parameters
 * @param[in,out] state 有效同域执行状态，字段、存储及用户比较可分配/抛异常。
 * @param[in] selfValue 存活同域Map接收者槽；本层核OBJECT/ARRAY标签和载荷，不独立验Map原型。
 * @param[in] keyValue 有效同域键，caller负责满足K契约；NULL拒绝。
 * @param[in] mappedValue 有效同域待写值，Core复制处理其ownership；NULL拒绝。
 * @param[out] result 可为NULL省略赋值结果；非空须满足普通dispatcher的新结果槽约定。
 *
 * ## Returns
 * @return 当前字段/追加、必要count及可选结果复制都完成时true；已检查失败返回false，先前写入没有事务回滚。分配异常可非局部退出，bool也不替代外层线程错误检查。
 *
 * ## Constraints and lifecycle
 * 普通callback直接调用；readonly无结果SET先自行更新已有Pair，新增时才回退本核心(NULL result)。查找可能执行用户代码，新Pair创建后重取self/entries，追加后又使用裸借用同步count。BUG: 合法同域C API对正常Map<String,int>新增项，会调用make_pair；若其first新own槽的HashPair首次申请及FullGC重试被选择性拒绝、随后异常规范化能成功，外层TryRun/ResetThread后，尚未接入entries的Pair仍遗留Core临时ignored根。TryRun、线程reset和正常GC不自动撤该登记。TODO: 配对清理新增字段写入异常的临时pin；Pair完整创建后storage_push的精确分配序号、已有Shared字段的清理以及追加后count更新的self/entries地址稳定性均须独立核查。已完成的更新/count/结果复制失败不提供事务回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:6689;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6613;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6620;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6626;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6627;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6632;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6634;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6638;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6642;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6644;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6650;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6654;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6657;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6661;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6662;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6664;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6734;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6738;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4260;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4270;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4125;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7544;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7558;zr_vm_core/src/zr_vm_core/object/object.c:1482;zr_vm_core/src/zr_vm_core/object/object.c:1488;zr_vm_core/src/zr_vm_core/object/object.c:1504;zr_vm_core/src/zr_vm_core/object/object.c:1509;zr_vm_core/src/zr_vm_core/object/object.c:1519;zr_vm_core/src/zr_vm_core/object/object.c:558;zr_vm_core/src/zr_vm_core/object/object.c:559;zr_vm_core/src/zr_vm_core/object/object.c:565;zr_vm_core/src/zr_vm_core/object/object.c:573;zr_vm_core/src/zr_vm_core/object/object.c:1702;zr_vm_core/src/zr_vm_core/object/object.c:1713;zr_vm_core/src/zr_vm_core/object/object.c:1716;zr_vm_core/src/zr_vm_core/object/object.c:1727;zr_vm_core/src/zr_vm_core/object/object.c:1752;zr_vm_core/src/zr_vm_core/object/object.c:1754;zr_vm_core/src/zr_vm_core/object/object.c:1793;zr_vm_core/src/zr_vm_core/object/object.c:1795;zr_vm_core/src/zr_vm_core/object/object.c:1818;zr_vm_core/src/zr_vm_core/object/object.c:1840;zr_vm_core/src/zr_vm_core/object/object.c:1843;zr_vm_core/src/zr_vm_core/object/object.c:1845;zr_vm_core/src/zr_vm_core/object/object.c:1849;zr_vm_core/src/zr_vm_core/object/object.c:1858;zr_vm_core/src/zr_vm_core/object/object.c:1860;zr_vm_core/src/zr_vm_core/object/object.c:1863;zr_vm_core/src/zr_vm_core/object/object.c:1864;zr_vm_core/include/zr_vm_core/hash_set.h:488;zr_vm_core/include/zr_vm_core/hash_set.h:494;zr_vm_core/include/zr_vm_core/hash_set.h:503;zr_vm_core/include/zr_vm_core/hash_set.h:507;zr_vm_core/include/zr_vm_core/hash_set.h:510;zr_vm_core/src/zr_vm_core/base/memory.c:13;zr_vm_core/src/zr_vm_core/base/memory.c:16;zr_vm_core/src/zr_vm_core/base/memory.c:17;zr_vm_core/src/zr_vm_core/base/memory.c:18;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:42;zr_vm_core/src/zr_vm_core/base/memory.c:44;zr_vm_core/src/zr_vm_core/base/memory.c:45;zr_vm_core/src/zr_vm_core/base/memory.c:46;zr_vm_core/src/zr_vm_core/ownership/ownership.c:137;zr_vm_core/src/zr_vm_core/ownership/ownership.c:153;zr_vm_core/src/zr_vm_core/ownership/ownership.c:156;zr_vm_core/src/zr_vm_core/ownership/ownership.c:463;zr_vm_core/src/zr_vm_core/ownership/ownership.c:499;zr_vm_core/src/zr_vm_core/ownership/ownership.c:503;zr_vm_core/src/zr_vm_core/ownership/ownership.c:680;zr_vm_core/src/zr_vm_core/ownership/ownership.c:724;zr_vm_core/src/zr_vm_core/ownership/ownership.c:730;zr_vm_core/src/zr_vm_core/ownership/ownership.c:755;zr_vm_core/src/zr_vm_core/ownership/ownership.c:801;zr_vm_core/src/zr_vm_core/ownership/ownership.c:804;zr_vm_core/src/zr_vm_core/ownership/ownership.c:808;zr_vm_core/src/zr_vm_core/gc/gc.c:678;zr_vm_core/src/zr_vm_core/gc/gc.c:690;zr_vm_core/src/zr_vm_core/gc/gc.c:701;zr_vm_core/src/zr_vm_core/gc/gc.c:702;zr_vm_core/src/zr_vm_core/gc/gc.c:704;zr_vm_core/src/zr_vm_core/gc/gc.c:711;zr_vm_core/src/zr_vm_core/gc/gc.c:733;zr_vm_core/src/zr_vm_core/gc/gc.c:745;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:448;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:456;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:457;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:464;zr_vm_core/src/zr_vm_core/exception/exception.c:548;zr_vm_core/src/zr_vm_core/exception/exception.c:556;zr_vm_core/src/zr_vm_core/exception/exception.c:557;zr_vm_core/src/zr_vm_core/exception/exception.c:560;zr_vm_core/src/zr_vm_core/exception/exception.c:563;zr_vm_core/src/zr_vm_core/exception/exception.c:565;zr_vm_core/src/zr_vm_core/exception/exception.c:566;zr_vm_core/src/zr_vm_core/exception/exception.c:570;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:19;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:22;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:24;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:26;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:30;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:43;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:45;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:53;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:59;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:62;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:36;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:40;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:52;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:54;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:58;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:59;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:60;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:63;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:74;zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c:76;zr_vm_core/src/zr_vm_core/runtime/state.c:272;zr_vm_core/src/zr_vm_core/runtime/state.c:274;zr_vm_core/src/zr_vm_core/runtime/state.c:281;zr_vm_core/src/zr_vm_core/runtime/state.c:284;zr_vm_core/src/zr_vm_core/runtime/state.c:291;zr_vm_core/src/zr_vm_core/runtime/state.c:297;zr_vm_core/src/zr_vm_core/runtime/state.c:299;zr_vm_core/src/zr_vm_core/runtime/state.c:305;zr_vm_core/src/zr_vm_core/runtime/state.c:311;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:873;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:881;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:884;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:889;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:890;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1214;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1223;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1237;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1241;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1674;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1675;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7421;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7439;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7444;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7445;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7446;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7733;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7735;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7737;zr_vm_library/src/zr_vm_library/builtin_module.c:355;zr_vm_library/src/zr_vm_library/builtin_module.c:356;zr_vm_library/src/zr_vm_library/builtin_module.c:358;zr_vm_core/include/zr_vm_core/gc.h:346;zr_vm_core/include/zr_vm_core/gc.h:358;zr_vm_core/include/zr_vm_core/gc.h:362;zr_vm_core/include/zr_vm_core/gc.h:366;zr_vm_core/include/zr_vm_core/gc.h:367;zr_vm_core/include/zr_vm_core/global.h:174;zr_vm_core/include/zr_vm_core/memory.h:22;zr_vm_core/include/zr_vm_core/memory.h:37;zr_vm_core/include/zr_vm_core/memory.h:39;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:327;zr_vm_core/src/zr_vm_core/gc/gc_domain.c:339;tests/core/test_ssa_cross_domain_clone_oom.c:151;tests/core/test_ssa_cross_domain_clone_oom.c:160;tests/core/test_ssa_cross_domain_clone_oom.c:162;tests/core/test_ssa_cross_domain_clone_oom.c:163;tests/core/test_ssa_cross_domain_clone_oom.c:170;tests/core/test_ssa_cross_domain_clone_oom.c:172;tests/core/test_ssa_cross_domain_clone_oom.c:173;tests/core/test_ssa_cross_domain_clone_oom.c:175;tests/core/test_ssa_cross_domain_clone_oom.c:178;tests/core/test_ssa_cross_domain_clone_oom.c:186;tests/core/test_ssa_cross_domain_clone_oom.c:190;tests/core/test_ssa_cross_domain_clone_oom.c:195;tests/core/test_ssa_cross_domain_clone_oom.c:514;tests/core/test_ssa_cross_domain_clone_oom.c:525;tests/core/test_ssa_cross_domain_clone_oom.c:557;tests/core/test_ssa_cross_domain_clone_oom.c:562;zr_vm_core/src/zr_vm_core/runtime/global.c:360;zr_vm_core/src/zr_vm_core/runtime/global.c:370;zr_vm_core/src/zr_vm_core/runtime/global.c:374;zr_vm_core/src/zr_vm_core/exception/exception.c:710;zr_vm_core/src/zr_vm_core/exception/exception.c:719;zr_vm_core/src/zr_vm_core/exception/exception.c:731
 */
static ZR_FORCE_INLINE TZrBool zr_container_map_set_item_core(SZrState *state,
                                                              const SZrTypeValue *selfValue,
                                                              const SZrTypeValue *keyValue,
                                                              const SZrTypeValue *mappedValue,
                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject;
    TZrBool insertedNewEntry = ZR_FALSE;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || mappedValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(state, self);
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject != ZR_NULL) {
        if (!zr_container_map_entry_set_second_value_fast(state, entryObject, mappedValue)) {
            return ZR_FALSE;
        }
    } else {
        SZrObject *pair = zr_container_make_pair(state, keyValue, mappedValue);
        SZrTypeValue pairValue;
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }

        self = ZR_CAST_OBJECT(state, selfValue->value.object);
        if (self == ZR_NULL) {
            return ZR_FALSE;
        }
        entries = zr_container_get_entries_array_cached_fast(state, self);
        if (entries == ZR_NULL) {
            entries = zr_container_ensure_entries_array_fast(state, self);
            if (entries == ZR_NULL) {
                return ZR_FALSE;
            }
        }

        zr_container_value_set_object_fast(state, &pairValue, pair, ZR_VALUE_TYPE_OBJECT);
        if (!zr_container_storage_push(state, entries, &pairValue)) {
            return ZR_FALSE;
        }
        insertedNewEntry = ZR_TRUE;
    }

    if (insertedNewEntry &&
        !zr_container_set_int_field_fast(state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    return result == ZR_NULL ? ZR_TRUE : zr_container_result_copy_no_profile(state, result, mappedValue);
}

/**
 * ## Summary
 * 将Map普通SET_ITEM调用转交写入核心，统一更新/新增和结果复制契约。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Map SET_ITEM，恰key:K和value:V两个实参。
 * @param[out] result 普通dispatcher提供的非NULL且预先置null的短期赋值结果槽；不保留槽指针。
 *
 * ## Returns
 * @return 透传map_set_item_core状态；有result时由核心复制赋值值。false不表示已回滚，用户比较非局部退出不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Map对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。普通callback接收dispatcher局部结果槽；RESULT_OPTIONAL选择无结果inline lane，不表示此callback收到NULL。泛型K的hash/equality约束由类型注册和调用层承担，V无附加约束。引用SET核心已有Pair分配/部分写入BUG/TODO，不复制新身份；TODO: 失败状态及count/赋值结果一致性沿核心调用链核查。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7547;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6684;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6689;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6605;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6664;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7405;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7729;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616
 */
static TZrBool zr_container_map_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_map_set_item_core(context->state,
                                          ZrLib_CallContext_Self(context),
                                          ZrLib_CallContext_Argument(context, 0),
                                          ZrLib_CallContext_Argument(context, 1),
                                          result);
}

/**
 * ## Summary
 * 为Map提供无结果inline SET；已有Pair直接更新，缺backing或缺键才在函数内转交SET核心新增。
 *
 * ## Parameters
 * @param[in,out] state 当前执行域有效状态。
 * @param[in] selfValue caller保持有效的Map接收槽，编码须为OBJECT或ARRAY。
 * @param[in] keyValue 调用期间有效并满足Core资格的借用键槽。
 * @param[in] mappedValue 调用期间有效且可按Core字段复制规则写入的借用值槽。
 *
 * ## Returns
 * @return 普通Pair更新或miss分支核心成功true；参数/字段/核心路径普通失败false。用户比较非局部退出不由bool表示。
 *
 * ## Constraints and lifecycle
 * 无result形参；receiver/backing不变量由caller保证。cached getter不隐式建backing，miss调用map_set_item_core(...,NULL)。READONLY_INLINE_VALUE_CONTEXT/RESULT_OPTIONAL不表示receiver只读。Core inline SET在false且threadStatus为FINE时当作成功，且不回退普通callback。TODO: 核已有Pair setter false的线程错误传播与静默no-op边界；此疑点不升级BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7558;zr_vm_core/src/zr_vm_core/object/object.c:1003;zr_vm_core/src/zr_vm_core/object/object.c:1201;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:219;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6715;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6734;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6736;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6739;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3367;zr_vm_core/src/zr_vm_core/object/object.c:980;zr_vm_core/src/zr_vm_core/object/object.c:1233;zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:225
 */
static ZR_FORCE_INLINE TZrBool zr_container_map_set_item_readonly_inline_no_result_fast(
        SZrState *state,
        const SZrTypeValue *selfValue,
        const SZrTypeValue *keyValue,
        const SZrTypeValue *mappedValue) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject = ZR_NULL;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || mappedValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_get_entries_array_cached_fast(state, self);
    if (entries != ZR_NULL) {
        entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    }
    if (entryObject != ZR_NULL) {
        return zr_container_map_entry_set_second_value_fast(state, entryObject, mappedValue);
    }

    return zr_container_map_set_item_core(state, selfValue, keyValue, mappedValue, ZR_NULL);
}

/**
 * ## Summary
 * 为Set构造元方法建立entries与公开count的一致初始状态并发布实例。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；零参构造，可没有接收对象。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功发布Set实例，不保留槽指针。
 *
 * ## Returns
 * @return entries建立、字段初始化及finish_object普通成功true；普通分配/写字段失败false。分配异常可非局部退出，不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * 零参构造，上下文来自已注册类型的构造元方法，合法调用可无Self，resolver选择/新建target；target及新backing须满足当前执行域资格。字段初始化或结果发布失败无回滚保证。引用resolve_construct_target既有条件性公开C API构造期GC BUG，不复制新缺陷身份；TODO: 中间backing/target有效期沿resolver和字段发布helper核查。Set的T声明IHashable/IEquatable<T>。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参及存在的receiver由调用帧保持有效；构造目标沿resolver选择/创建契约。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7585;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1390;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6762;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1907;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6763;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6764;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6770;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7449;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7731;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1429;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_set_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, self, kContainerEntriesField, entries) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

/**
 * ## Summary
 * 为Set提供去重追加入口，在存储新增成功后发布实际count。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Set实例方法，恰一个value:T实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；普通成功写guest是否新增bool，不保留槽指针。
 *
 * ## Returns
 * @return 普通重复项写guest false并native true；push/count成功写guest true并native true；检查、存储或count普通失败native false，失败不保证已写结果。用户回调可非局部退出。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Set对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。T声明hash/equality约束，但实现线性查重。TODO: search false后直接尝试storage_push，需核helper/dispatcher是否阻止用户比较已置错误后的追加；不能仅凭缺少显式分支宣称BUG。push成功而count失败不回滚；值的owner复制由storage helper承担。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7568;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6794;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6805;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6813;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6815;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4260;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7449;tests/container/test_container_runtime.c:1632;tests/container/test_container_runtime.c:1637;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_set_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    value = ZrLib_CallContext_Argument(context, 0);
    if (zr_container_set_find_index(context->state, entries, value, ZR_NULL)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_storage_push(context->state, entries, value) ||
        !zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Set提供实际backing的成员查询，交付guest bool结果。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Set实例方法，恰一个value:T实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；写guest bool，不保留槽指针。
 *
 * ## Returns
 * @return 普通查询写搜索bool并native true；guest false也可源于helper输入/搜索失败，native false属于回调层失败。用户回调非局部退出不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Set对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。ensure_entries可能建立backing；搜索使用实际长度而非count，hash泛型约束不承诺按桶O(1)查询。TODO: helper false不区分缺项与搜索错误，外围须结合threadStatus；重入与既有cache问题沿find helper记录。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7571;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6838;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6846;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6849;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5033;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7423;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7731;tests/container/test_container_runtime.c:1731;tests/container/test_container_runtime.c:1745;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_set_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    zr_container_result_set_bool_fast(
            result,
            zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL));
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Set删除首个匹配项，在storage成功后将count同步到实际长度。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；Set实例方法，恰一个value:T实参。
 * @param[out] result 派发层提供、调用期间有效的结果槽；普通返回写guest删除bool，不保留槽指针。
 *
 * ## Returns
 * @return 查找/storage普通false写guest false并native true；移除及count更新成功写guest true并native true；最后count失败返回native false，结果与删除不保证回滚。用户回调可非局部退出。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Set对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。TODO: guest false合并缺项与底层失败，不证明backing未改变；成功删除后的count失败也没有事务回滚。owner释放/通用位移已审问题沿storage_remove_at记录，不在wrapper重复BUG。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7574;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6870;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6880;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4458;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4463;tests/container/test_container_runtime.c:1731;tests/container/test_container_runtime.c:1747;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_set_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (!zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为Set清空并复用entries backing，将公开count置零。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；无参Set实例方法。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功写null，不保留槽指针。
 *
 * ## Returns
 * @return entries准备及count发布普通成功true；ensure/count普通失败false，count失败可发生在清空之后。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Set对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。清理由array_clear_items_reuse_storage承担，引用其既有owner BUG/TODO，不重复计数。TODO: 清空后的count失败不回滚，需核上层如何观察部分状态。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7577;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6913;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6921;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6926;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6930;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3539;tests/container/test_container_runtime.c:1662;tests/container/test_container_runtime.c:1683;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_set_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(entries);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 作为Set的ITERABLE_INIT入口，为Iterable<T>消费者创建entries Enumerator。
 *
 * ## Parameters
 * @param[in,out] context 有效的绑定调用上下文；无参Set实例方法。
 * @param[out] result 派发层提供、调用期间有效的结果槽；成功接收Enumerator，不保留槽指针。
 *
 * ## Returns
 * @return iterator建立及finish_object普通成功true；否则false。分配异常可能非局部退出，不由bool表示。 此处bool描述普通绑定回调的局部状态；dispatcher在false且threadStatus仍为FINE时将guest调用完成为null，非FINE沿线程错误路径处理。
 *
 * ## Constraints and lifecycle
 * receiver须为有效同域Set对象并满足entries的internal ARRAY不变量；本wrapper不独立验证完整原型/泛型布局。输入由调用帧借用，结果槽须符合dispatcher约定。source引用entries，不复制内容或建立长度/版本快照；后续推进读live backing。current为T，遵循通用Array iterator/Value_Copy值复制契约，不保证与源元素槽保持别名。TODO: backing修改期间推进与借用有效期沿iterator组核查；本wrapper不另建source root。 普通dispatcher预先将局部result置null并传入非NULL短期结果槽；实参与receiver由调用帧保持有效。回调false不等同guest调用失败，也不证明状态回滚。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7579;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1295;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277;zr_vm_core/src/zr_vm_core/object/object.c:4025;zr_vm_core/src/zr_vm_core/object/object.c:4045;zr_vm_core/src/zr_vm_core/module/module_prototype.c:505;zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1258;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6951;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5194;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5218;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5432;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7429;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7731;zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_foreach.c:360;zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_foreach.c:380;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:3394;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:8926;tests/container/test_container_runtime.c:1637;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:616;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:624
 */
static TZrBool zr_container_set_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/**
 * ## Summary
 * 提供可独立持有的LinkedNode<T>构造入口，分开节点元素的持有与加入链表的职责；链表插入使用make_linked_node而不调用此元方法。
 *
 * ## Parameters
 * @param[in,out] context 有效同域LinkedNode<T>构造上下文；允许无receiver，元数据接收0..1个业务实参，value:T借自帧，字段复制决定其owner。
 * @param[out] result dispatcher提供的短期结果槽；字段初始化完成后接收构造节点，不保留槽地址。
 *
 * ## Returns
 * @return 目标选择、字段初始化及结果发布成功true；受检失败false，已初始化字段没有回滚保证。分配异常可非局部退出；普通dispatcher预置null并提供非NULL短期结果槽；回调局部false且threadStatus为FINE时guest完成为null，非FINE按线程错误路径处理。实参及存在的receiver借自调用帧；C借用指针不能据此推定跨移动GC仍有效。
 *
 * ## Constraints and lifecycle
 * 目标沿resolve_construct_target选择或创建，callback不独立核完整原型/泛型；T无附加约束，省略元素对应guest null。成功构造的next/previous为空，不自动加入任何列表；不要把独立node构造等同addFirst/addLast的链操作。TODO: 核无receiver的新target在可能分配的字段初始化期间的root/借用指针生命周期，以及对既有receiver再次构造时的成员身份约束；不扩大Array构造临时root既有BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7658;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1982;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1989;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2128;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2150;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7650;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7651;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7652;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7653;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7737;tests/container/test_container_metadata.c:208;tests/container/test_container_metadata.c:249;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrBool zr_container_linked_node_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *node = zr_container_resolve_construct_target(context);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 0) {
        if (!zr_container_set_value_field_fast(context->state,
                                               node,
                                               kContainerValueField,
                                               ZrLib_CallContext_Argument(context, 0))) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_null_field_fast(context->state, node, kContainerValueField)) {
            return ZR_FALSE;
        }
    }
    if (!zr_container_set_null_field_fast(context->state, node, kContainerNextField) ||
        !zr_container_set_null_field_fast(context->state, node, kContainerPreviousField)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

/**
 * ## Summary
 * 承接首尾或搜索所得候选节点的链表摘除边界，维护相邻链接、端点和count；脱链保留node.value，供外部节点持有者继续使用。
 *
 * ## Parameters
 * @param[in,out] state 有效同域执行状态；NULL时不操作。
 * @param[in,out] list 存活同域且first/last/count已正常初始化的链表；NULL时不操作。
 * @param[in,out] node 候选存活节点；调用方须保证调用时仍属于list，next/previous须有效；本函数不核membership，NULL时不操作。
 *
 * ## Returns
 * 更新相邻链接或list端点，清node.next/previous并减少count且下限为零；保留node.value。本void接口无成功/失败返回值。
 *
 * ## Constraints and lifecycle
 * 没有membership验证、独立root、互斥或回滚；removeFirst/removeLast先复制value，通用remove可在比较回调之后调用，caller须保证其后成员身份仍有效。内部setter bool均丢弃，正常List/Node已有相应own槽，不能凭多次setter就断言HashSet_Add OOM可达。TODO: 明确用户equals重入后的成员核验及多次字段更新的失败传播/部分连接状态；沿既有null字段setter审共享owner覆盖协议，脱链不是释放value。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7307;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7034;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7038;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7039;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7040;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7041;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7045;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7046;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7048;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7051;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7052;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7053;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7054;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7228;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7229;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7269;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7306;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7631;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7634
 */
static void zr_container_linked_list_unlink_node(SZrState *state, SZrObject *list, SZrObject *node) {
    SZrObject *previous;
    SZrObject *next;
    TZrInt64 count;

    if (state == ZR_NULL || list == ZR_NULL || node == ZR_NULL) {
        return;
    }

    previous = zr_container_get_object_field_fast(state, node, kContainerPreviousField);
    next = zr_container_get_object_field_fast(state, node, kContainerNextField);
    if (previous != ZR_NULL) {
        (void)zr_container_set_object_field_fast(state, previous, kContainerNextField, next);
    } else {
        (void)zr_container_set_object_field_fast(state, list, kContainerFirstField, next);
    }
    if (next != ZR_NULL) {
        (void)zr_container_set_object_field_fast(state, next, kContainerPreviousField, previous);
    } else {
        (void)zr_container_set_object_field_fast(state, list, kContainerLastField, previous);
    }

    (void)zr_container_set_null_field_fast(state, node, kContainerNextField);
    (void)zr_container_set_null_field_fast(state, node, kContainerPreviousField);
    count = zr_container_get_int_field(state, list, kContainerCountField, 0);
    (void)zr_container_set_int_field_fast(state, list, kContainerCountField, count > 0 ? count - 1 : 0);
}

/**
 * ## Summary
 * 为LinkedList<T>提供满足空表不变量的构造结果，作为后续首尾插入与Iterable消费的起点；构造不创建节点或元素backing。
 *
 * ## Parameters
 * @param[in,out] context 有效同域LinkedList<T>构造上下文；允许无receiver，描述符不接受业务实参，目标沿共享resolver取得。
 * @param[out] result dispatcher提供的短期结果槽；空表字段初始化完成后接收构造对象。
 *
 * ## Returns
 * @return 目标选择、空表初始化及结果发布成功true；任一受检失败false，先前字段修改不回滚。分配异常可非局部退出；普通dispatcher预置null并提供非NULL短期结果槽；回调局部false且threadStatus为FINE时guest完成为null，非FINE按线程错误路径处理。实参及存在的receiver借自调用帧；C借用指针不能据此推定跨移动GC仍有效。
 *
 * ## Constraints and lifecycle
 * 空表契约为count=0且first/last为null；字段初始化与结果发布不组成事务。T无附加约束；callback不全面验证既有receiver的类型或链状态。TODO: 核新target在初始化写入分配期间的root/指针有效期、失败后的部分状态和再次构造非空receiver的链接责任；不套用其他构造器的BUG结论。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7646;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1910;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1982;zr_vm_lib_container/src/zr_vm_lib_container/module.c:1989;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7615;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7616;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7617;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7440;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7735;tests/container/test_container_type_inference.c:350;tests/container/test_container_type_inference.c:356;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrBool zr_container_linked_list_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerFirstField) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerLastField)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

/**
 * ## Summary
 * 为LinkedList头部插入建立节点构造、连接与结果发布边界；调用成功后外部可保留新节点。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。 业务实参value:T由context借用取得；同域字段复制不转移输入owner，T签名不等于本回调的动态类型校验。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 节点创建或受检连接/元数据写入失败时false；完成连接后发布节点结果。false不回滚先前修改，内存异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 receiver未另核LinkedList owner，合法调用须使用相应实例。首尾与count的多次写入不组成事务；新节点value和输入的身份/owner由Core复制决定。本入口无业务用户回调，但复制/分配仍可触发GC。make_linked_node首字段OOM临时根问题见其独立契约。TODO: 保留函数体失败注入项，核部分连接、端点与count状态及count上界，不把未注入的setter失败推广为新BUG。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7622;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7104;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7105;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7113;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7117;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7122;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7123;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7127;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7130;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7135;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7138;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7140;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4743;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2010;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2171;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_linked_list_add_first(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: addFirst/addLast/clear 分多次写首尾和节点连接；需用字段写入失败
     * 注入验证中途退出后 count、首尾与双向链接的一致性。
     */
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *first;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    if (!zr_container_set_object_field_fast(context->state, node, kContainerNextField, first)) {
        return ZR_FALSE;
    }
    if (first != ZR_NULL) {
        if (!zr_container_set_object_field_fast(context->state, first, kContainerPreviousField, node)) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_object_field_fast(context->state, self, kContainerLastField, node)) {
            return ZR_FALSE;
        }
    }
    count = zr_container_get_int_field(context->state, self, kContainerCountField, 0);
    if (!zr_container_set_object_field_fast(context->state, self, kContainerFirstField, node) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, count + 1)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

/**
 * ## Summary
 * 为LinkedList尾部插入建立节点构造、连接与结果发布边界，与头部插入共享字段更新约束。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。 业务实参value:T由context借用取得；同域字段复制不转移输入owner，T签名不等于本回调的动态类型校验。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 节点创建或受检连接/元数据写入失败时false；完成连接后发布节点结果，失败无回滚承诺。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 receiver未另核LinkedList owner；外部节点引用可继续保活其value。连接与首尾/count逐项写入，不是事务。make_linked_node首字段OOM临时根问题见其独立契约；正常调用frame资格不能代替任意裸helper的GC保护。TODO: 核部分写入失败、count边界及节点发布前的根清理，正常addLast测试不覆盖故障注入。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7625;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7160;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7168;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7170;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7175;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7176;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7180;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7183;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7188;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7191;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7193;zr_vm_lib_container/src/zr_vm_lib_container/module.c:4743;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2010;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2171;tests/container/test_container_runtime.c:1832;tests/container/test_container_runtime.c:1834;tests/container/test_container_runtime.c:1917;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_linked_list_add_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *last;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field_fast(context->state, self, kContainerLastField);
    if (!zr_container_set_object_field_fast(context->state, node, kContainerPreviousField, last)) {
        return ZR_FALSE;
    }
    if (last != ZR_NULL) {
        if (!zr_container_set_object_field_fast(context->state, last, kContainerNextField, node)) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_object_field_fast(context->state, self, kContainerFirstField, node)) {
            return ZR_FALSE;
        }
    }
    count = zr_container_get_int_field(context->state, self, kContainerCountField, 0);
    if (!zr_container_set_object_field_fast(context->state, self, kContainerLastField, node) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, count + 1)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

/**
 * ## Summary
 * 为LinkedList首节点删除提供先复制结果再摘链的发布顺序，空表返回null。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 输入拒绝或结果复制失败时false，复制失败尚未摘链；空表或复制后继续摘链时回调true。unlink为void，true不独立证明每项字段写入成功。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 结果按Core copy/ownership规则复制，节点原value仍保留；unlink只断链接与更新首尾/count，外部节点引用可继续保活value。TODO: 核unlink忽略setter结果时的部分更新；若T为非RefLike STRUCT，Value_Copy可CloneStruct并分配；需核复制期间源节点/value槽的可达与地址稳定、result发布前的根资格，以及节点局部跨复制后unlink的有效性。value指针在复制后不再读取；当前未闭合合法收集/移动链，不标为已证实BUG。 非空节点的value字段缺失或为null也可发布null并继续摘链；null结果本身不表示链表原本为空。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7628;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7213;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7222;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7224;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7225;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7228;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7229;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7233;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3955;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7029;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193;zr_vm_core/src/zr_vm_core/ownership/ownership.c:206;zr_vm_core/src/zr_vm_core/ownership/ownership.c:207;zr_vm_core/src/zr_vm_core/object/object.c:1613;zr_vm_core/src/zr_vm_core/gc/gc.c:792;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3005;tests/container/test_container_runtime.c:1876;tests/container/test_container_runtime.c:1917;tests/container/test_container_runtime.c:1921;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_linked_list_remove_first(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *first;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    if (first == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    value = zr_container_get_own_field_value_fast(context->state, first, kContainerValueField);
    if (!zr_container_result_copy_no_profile(context->state, result, value)) {
        return ZR_FALSE;
    }
    zr_container_linked_list_unlink_node(context->state, self, first);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为LinkedList尾节点删除提供先复制结果再摘链的发布顺序，空表返回null。
 *
 * ## Parameters
 * @param[in,out] context 活动同域绑定上下文；调用方须提供本方法所属类型或派生类型的有效receiver。回调只筛对象标签，dispatcher提供owner元数据而不全面校验receiver实例。
 * @param[out] result dispatcher初始化的空输出槽；调用者按绑定结果协议接收，不传待释放的旧owner。
 *
 * ## Returns
 * @return 输入拒绝或复制失败时false，复制失败尚未摘链；空表或复制后继续摘链时回调true，不独立确认void unlink的各项写入。
 *
 * ## Constraints and lifecycle
 * 回调bool与VM返回值数量、threadStatus分开；线程非FINE时外层dispatcher拒绝结果，无错误但false时外层转null。字段值是借用，Core复制决定owner/STRUCT行为；回调不建立跨域、锁或完整字段快照。 复制不转移或清除节点原value，外部节点引用可继续保活它；首尾、链接和count更新无原子性承诺。TODO: 核unlink失败状态；若T为非RefLike STRUCT，Value_Copy可CloneStruct并分配，须核复制期间源节点/value槽的可达与地址稳定、result发布前的根资格，以及尾节点局部跨复制后unlink的有效性。value指针在复制后不再读取；尚无完整合法收集/移动链，空表测试不覆盖非空removeLast、setter失败或移动GC。 非空节点的value字段缺失或为null也可发布null并继续摘链；null结果本身不表示链表原本为空。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7631;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7253;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7262;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7264;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7265;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7268;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7269;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7273;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3955;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7029;zr_vm_core/src/zr_vm_core/ownership/ownership.c:193;zr_vm_core/src/zr_vm_core/ownership/ownership.c:206;zr_vm_core/src/zr_vm_core/ownership/ownership.c:207;zr_vm_core/src/zr_vm_core/object/object.c:1613;zr_vm_core/src/zr_vm_core/gc/gc.c:792;zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3005;tests/container/test_container_runtime.c:1876;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;zr_vm_library/include/zr_vm_library/native_binding.h:671
 */
static TZrBool zr_container_linked_list_remove_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *last;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field_fast(context->state, self, kContainerLastField);
    if (last == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    value = zr_container_get_own_field_value_fast(context->state, last, kContainerValueField);
    if (!zr_container_result_copy_no_profile(context->state, result, value)) {
        return ZR_FALSE;
    }
    zr_container_linked_list_unlink_node(context->state, self, last);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 提供按value删除首个匹配节点的实例方法，以guest bool表达是否命中；摘链后外部节点持有者仍可保留其value。
 *
 * ## Parameters
 * @param[in,out] context 有效同域LinkedList<T>实例上下文及恰一个借用value:T；存储value作为左值参与相等协议，实参为右值。
 * @param[out] result dispatcher提供的短期结果槽；接收业务bool，与callback自身状态分开解释。
 *
 * ## Returns
 * @return 正常扫描命中或结束均true，并写是否找到并尝试摘链的bool；无效receiver/结果槽false。比较false也可能伴随线程错误，不能等同普通不匹配；普通dispatcher预置null并提供非NULL短期结果槽；回调局部false且threadStatus为FINE时guest完成为null，非FINE按线程错误路径处理。实参及存在的receiver借自调用帧；C借用指针不能据此推定跨移动GC仍有效。
 *
 * ## Constraints and lifecycle
 * caller须维持已初始化且链接/端点/count自洽的同域链表；wrapper只筛接收标签，不验证完整类型、membership或环。T无额外约束；equals可能运行guest代码，Node/value/needle是借用，不形成版本快照。void unlink不反馈每次字段写入状态，回调true不证明事务性更新。TODO: 明确是否支持equals重入修改同一列表；无移动GC且旧节点另有可达引用时，clear后addLast再返回true可令外层对已脱链旧节点摘链而清空新列表，但重入支持契约未定，因此不升级BUG。另核可执行比较期间借用指针与移动GC边界、公开可写链接的环/一致性责任。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7634;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3683;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3696;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3702;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3725;zr_vm_lib_container/src/zr_vm_lib_container/module.c:3742;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7029;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7038;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7043;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7048;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7054;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7306;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7307;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1303;zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1354;zr_vm_core/src/zr_vm_core/base/memory.c:39;tests/container/test_container_runtime.c:1836;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrBool zr_container_linked_list_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    const SZrTypeValue *needle;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    needle = ZrLib_CallContext_Argument(context, 0);
    node = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    while (node != ZR_NULL) {
        const SZrTypeValue *value = zr_container_get_own_field_value_fast(context->state, node, kContainerValueField);
        if (value != ZR_NULL && zr_container_values_equal(context->state, value, needle)) {
            zr_container_linked_list_unlink_node(context->state, self, node);
            zr_container_result_set_bool_fast(result, ZR_TRUE);
            return ZR_TRUE;
        }
        node = zr_container_get_object_field_fast(context->state, node, kContainerNextField);
    }

    zr_container_result_set_bool_fast(result, ZR_FALSE);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 将LinkedList恢复为空表并解除节点间的双向联系；外部持有节点的使用者仍保留节点value，清空列表不等于释放这些值。
 *
 * ## Parameters
 * @param[in,out] context 有效同域零参LinkedList<T>实例上下文；caller维持有限、无环且链接自洽的列表。
 * @param[out] result dispatcher提供的短期结果槽；成功接收guest null。
 *
 * ## Returns
 * @return 全部受检写入及结果发布完成true；无效receiver/结果槽或字段写入失败false。失败可能已改变部分节点、端点或count，没有回滚保证；普通dispatcher预置null并提供非NULL短期结果槽；回调局部false且threadStatus为FINE时guest完成为null，非FINE按线程错误路径处理。实参及存在的receiver借自调用帧；C借用指针不能据此推定跨移动GC仍有效。
 *
 * ## Constraints and lifecycle
 * node.value保留，外部节点引用决定该值继续可达；next/previous脱链与list端点/count更新并非原子操作。首尾null写入沿set_null_field_fast，其已证明Shared-owner热槽覆盖BUG仅按同一触发条件引用，不另建clear缺陷身份，也不宣称普通GC-node列表必然触发。TODO: 定义部分清链失败后的恢复协议、旧owner释放配对及公开链接修改的无环责任；正常同域失败注入尚未证明。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7637;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2122;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2123;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2128;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2150;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7345;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7346;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7353;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7354;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7355;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7614;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7650;tests/container/test_container_runtime.c:1841;tests/container/test_container_runtime.c:1850;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664
 */
static TZrBool zr_container_linked_list_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    while (node != ZR_NULL) {
        SZrObject *next = zr_container_get_object_field_fast(context->state, node, kContainerNextField);
        if (!zr_container_set_null_field_fast(context->state, node, kContainerNextField) ||
            !zr_container_set_null_field_fast(context->state, node, kContainerPreviousField)) {
            return ZR_FALSE;
        }
        node = next;
    }

    if (!zr_container_set_null_field_fast(context->state, self, kContainerFirstField) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerLastField) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 把LinkedList<T>接入统一Iterable<T>消费协议，为迭代消费者返回沿节点关系观察列表的Enumerator，而不是复制元素序列。
 *
 * ## Parameters
 * @param[in,out] context 有效同域零参LinkedList<T>实例上下文；caller维持接收者及初始first节点可用。
 * @param[out] result dispatcher提供的短期结果槽；成功接收Enumerator<T>对象。
 *
 * ## Returns
 * @return iterator建立、字段初始化及结果发布成功true；受检失败false，部分初始化没有回滚保证。空first并不阻止创建空迭代器；普通dispatcher预置null并提供非NULL短期结果槽；回调局部false且threadStatus为FINE时guest完成为null，非FINE按线程错误路径处理。实参及存在的receiver借自调用帧；C借用指针不能据此推定跨移动GC仍有效。
 *
 * ## Constraints and lifecycle
 * 以ITERABLE_INIT角色供Core IterInit/foreach链选择；source保持list可达，nextNode是初始游标。后续读取live节点value/next，current遵循T的Core值复制/owner规则，不提供长度或版本快照、锁或修改检测。字段可达边不是分配初始化期间独立临时root的证明。空表guest fixture检查重复空遍历；协议测试另检查Iterable元数据。TODO: 补查非空链表实际推进、修改中遍历、来源单独保活及临时对象分配GC边界，这些不能由空遍历或元数据测试替代。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7640;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5194;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5226;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5494;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5523;zr_vm_lib_container/src/zr_vm_lib_container/module.c:2844;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7437;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7735;zr_vm_core/src/zr_vm_core/object/object.c:4025;zr_vm_core/src/zr_vm_core/object/object.c:4054;zr_vm_core/src/zr_vm_core/object/object.c:4284;zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:3403;tests/iterator/test_enumerator_protocol.c:131;tests/iterator/test_enumerator_protocol.c:153;tests/fixtures/reference/core_semantics/protocols_iteration_comparable/empty_iterator_boundary.zr:1;tests/fixtures/reference/core_semantics/protocols_iteration_comparable/empty_iterator_boundary.zr:13;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:620;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:664;tests/container/test_container_runtime.c:2379;tests/container/test_container_runtime.c:2390
 */
static TZrBool zr_container_linked_list_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    iterator = zr_container_iterator_make(context->state,
                                          self,
                                          ZR_VALUE_TYPE_OBJECT,
                                          0,
                                          zr_container_get_object_field_fast(context->state, self, kContainerFirstField),
                                          zr_container_linked_list_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/* Array.add 等元素参数的泛型 T 契约。 */
static const ZrLibParameterDescriptor kArrayValueParameter[] = {{"value", "T", ZR_NULL}};
/* 插入与 SET_ITEM 的 index/value 顺序契约。 */
static const ZrLibParameterDescriptor kArrayInsertParameters[] = {{"index", "int", ZR_NULL}, {"value", "T", ZR_NULL}};
/* 索引参数；当前构造器也借用它，capacity 命名差异见元方法 TODO。 */
static const ZrLibParameterDescriptor kArrayIndexParameter[] = {{"index", "int", ZR_NULL}};
/* Map 读取/查找/删除的 K 参数契约。 */
static const ZrLibParameterDescriptor kMapKeyParameter[] = {{"key", "K", ZR_NULL}};
/* Map 写入键 K 与值 V 的参数顺序契约。 */
static const ZrLibParameterDescriptor kMapSetItemParameters[] = {{"key", "K", ZR_NULL}, {"value", "V", ZR_NULL}};
/* Set 相等查找和新增的 T 参数契约。 */
static const ZrLibParameterDescriptor kSetValueParameter[] = {{"value", "T", ZR_NULL}};
/* Pair 两个分量的 K/V 构造参数契约。 */
static const ZrLibParameterDescriptor kPairParameters[] = {{"first", "K", ZR_NULL}, {"second", "V", ZR_NULL}};
/* Pair 的同泛型 equals/compareTo 参数契约。 */
static const ZrLibParameterDescriptor kPairOtherParameter[] = {{"other", "Pair<K,V>", ZR_NULL}};
/* 链表节点构造及链表操作的元素 T 参数契约。 */
static const ZrLibParameterDescriptor kLinkedNodeValueParameter[] = {{"value", "T", ZR_NULL}};
/* 连续视图的相对 start/length 参数，实际边界由视图回调校验。 */
static const ZrLibParameterDescriptor kSpanSliceParameters[] = {
        {"start", "int", ZR_NULL},
        {"length", "int", ZR_NULL},
};

/* Map 键须提供哈希与相等契约；当前实现仍以 entries 扫描而非按键哈希建表。 */
static const TZrChar *kMapKeyConstraints[] = {"zr.builtin.IHashable", "zr.builtin.IEquatable<K>"};
/* Set 元素的哈希与相等类型约束，不承诺 O(1) 查找。 */
static const TZrChar *kSetValueConstraints[] = {"zr.builtin.IHashable", "zr.builtin.IEquatable<T>"};
/* Array 同时声明索引与迭代接口，角色闭包由注册表绑定。 */
static const TZrChar *kArrayImplements[] = {"zr.builtin.IArrayLike<T>", "zr.iteration.Iterable<T>"};
/* Map 枚举 Pair<K,V>，不只枚举键。 */
static const TZrChar *kMapImplements[] = {"zr.iteration.Iterable<Pair<K,V>>"};
/* Set 枚举其 T 元素。 */
static const TZrChar *kSetImplements[] = {"zr.iteration.Iterable<T>"};
/* Pair 声明相等、排序和哈希接口，均绑定下方本地回调。 */
static const TZrChar *kPairImplements[] = {
        "zr.builtin.IEquatable<Pair<K,V>>",
        "zr.builtin.IComparable<Pair<K,V>>",
        "zr.builtin.IHashable"
};
/* LinkedList 枚举 T，迭代源是节点链接而非 entries backing。 */
static const TZrChar *kLinkedListImplements[] = {"zr.iteration.Iterable<T>"};

/* 无额外约束的单参数 T 契约，供 Array/LinkedList/Node/视图共享。 */
static const ZrLibGenericParameterDescriptor kSingleGenericT[] = {{"T", ZR_NULL, ZR_NULL, 0}};
/* Pair 的无附加约束 K/V 参数，不自动继承 Map 的键约束。 */
static const ZrLibGenericParameterDescriptor kGenericKV[] = {{"K", ZR_NULL, ZR_NULL, 0}, {"V", ZR_NULL, ZR_NULL, 0}};
/* 只给 K 附加哈希/相等约束，V 不增加约束。 */
static const ZrLibGenericParameterDescriptor kMapGenericParameters[] = {
        {"K", ZR_NULL, kMapKeyConstraints, ZR_ARRAY_COUNT(kMapKeyConstraints)},
        {"V", ZR_NULL, ZR_NULL, 0},
};
/* 给 T 附加哈希/相等约束。 */
static const ZrLibGenericParameterDescriptor kSetGenericParameters[] = {
        {"T", ZR_NULL, kSetValueConstraints, ZR_ARRAY_COUNT(kSetValueConstraints)},
};

/* 可见 length/capacity 字段；length 绑定 INDEX_LENGTH，隐藏 items 不在公开字段表。 */
static const ZrLibFieldDescriptor kArrayFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "length", "int", ZR_NULL,
                ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("capacity", "int", ZR_NULL),
};
/* Array 的普通方法与 span/迭代角色绑定表；callback 由 registry 创建闭包后派发。 */
static const ZrLibMethodDescriptor kArrayMethods[] = {
        /* 绑定 Array.span 到 ContiguousView_FromArray，返回引用当前数组区间的 Span<T>。 实例无参；VIEW_CREATE 角色供连续视图 lowering 使用；不复制/拥有元素，区间与来源由视图实现检查。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("span", 0, 0, ZrVmLibContainer_ContiguousView_FromArray,
                                           "Span<T>", ZR_NULL, ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE),
        /* 绑定 Array.add，把 value:T 追加到 backing 并更新公开 length。 实例恰1参，默认派发flags0；普通 null 结果，追加/元数据失败不提供事务回滚。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, zr_container_array_add, "null", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        /* 绑定 Array.insert，在 index 处移位后插入 value:T。 实例恰2参 index:int/value:T，位置0..length；普通 null 结果，generic移位逐次失败传播见 TODO。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("insert", 2, 2, zr_container_array_insert, "null", ZR_NULL, ZR_FALSE,
                                      kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)),
        /* 绑定 Array.removeAt，移除指定现存位置并更新 length。 实例恰1参 index:int；普通 null 结果，非法位置或存储失败走当前边界错误路径。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeAt", 1, 1, zr_container_array_remove_at, "null", ZR_NULL, ZR_FALSE,
                                      kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)),
        /* 绑定 Array.clear，复用存储并将 length 置零。 实例无参，普通 null 结果；capacity 保留，快速清空未逐槽 release，合法owner元素限制待核。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_array_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        /* 绑定 Array.contains，按当前元素相等规则返回存在性。 实例恰1参 value:T，普通 bool 结果；raw-int可专用搜索，对象equals可执行用户代码。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("contains", 1, 1, zr_container_array_contains, "bool", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        /* 绑定 Array.indexOf，返回首个相等位置或 -1。 实例恰1参 value:T，普通 int 结果；不假定equals无副作用，不搜索未在backing中的公开length虚值。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("indexOf", 1, 1, zr_container_array_index_of, "int", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        /* 绑定 Array.getIterator，为 Iterable<T> 创建 backing Enumerator。 实例无参，ITERABLE_INIT 角色；返回GC迭代对象，其source保活backing，没有长度/版本快照。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_array_get_iterator, "zr.iteration.Enumerator<T>", ZR_NULL,
                                           ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* Array 构造和下标回调表；GET_ITEM 标注只读接收者，不限制隐藏缓存维护。 */
static const ZrLibMetaMethodDescriptor kArrayMetaMethods[] = {
        /* TODO: 此构造参数实际作为 capacity 读取，却复用名为 index 的元数据；
         * 需检查命名实参、签名帮助与 LSP 是否向用户暴露错误参数名。
         */
        /* 绑定 Array<T> 构造器，建立空 backing 与指定公开容量。 0..1参，返回已初始化Array<T>；可选参数实际capacity>=0，但元数据复用index名称；本项无GET_ITEM只读flag。 */
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_array_constructor, "Array<T>", ZR_NULL, kArrayIndexParameter, 1},
        /* 绑定 Array GET_ITEM，从有效下标复制元素，越界返回 null。 恰1参 index:int，READONLY_RECEIVER；结果目标由绑定层初始化，复制元素保留Value_Copy ownership语义。 */
        {ZR_META_GET_ITEM, 1, 1, zr_container_array_get_item, "T", ZR_NULL,
         kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter), ZR_NULL, 0,
         ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER},
        /* 绑定 Array SET_ITEM，把 value:T 写到现存 index 并返回该值。 恰2参 index:int/value:T，默认flags0；越界错误，不允许本元方法扩长度，结果复制按值ownership。 */
        {ZR_META_SET_ITEM, 2, 2, zr_container_array_set_item, "T", ZR_NULL, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
};

/* Map 公开 count 元数据，entries 保留为内部字段。 */
static const ZrLibFieldDescriptor kMapFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
};
/* Map 查询、删除、清空和 Pair 迭代入口绑定。 */
static const ZrLibMethodDescriptor kMapMethods[] = {
        /* 绑定 Map.containsKey，按实际entries查找K并返回存在性。 实例恰1参 key:K，普通bool结果；K受Map泛型哈希/相等约束，字符串正命中可缓存。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("containsKey", 1, 1, zr_container_map_contains_key, "bool", ZR_NULL, ZR_FALSE,
                                      kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)),
        /* 绑定 Map.remove，删除首个匹配键并同步count。 实例恰1参 key:K，普通bool结果；缺键或存储移除失败为guest false，callback状态另由派发消费。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_map_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)),
        /* 绑定 Map.clear，清空并复用entries、重置count。 实例无参，普通null结果；不逐Pair字段析构，保留空间，版本失效由清空backing实现。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_map_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        /* 绑定 Map.getIterator，供 Iterable<Pair<K,V>> 枚举条目。 实例无参，ITERABLE_INIT角色；返回GC迭代器引用entries，current的Pair复制遵循STRUCT语义而非保证别名。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_map_get_iterator, "zr.iteration.Enumerator<Pair<K,V>>",
                                           ZR_NULL, ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* Map 的下标操作同时提供普通 callback 与内联快路径；只读 get、无结果 set
 * 由派发标志选择，新增路径必须保留相同的缺失键与插入/更新语义。
 */
/* Map 下标的普通/栈根内联派发契约；RESULT_OPTIONAL 允许省略赋值结果，不表示禁止写内容。 */
static const ZrLibMetaMethodDescriptor kMapMetaMethods[] = {
        /* 绑定 Map<K,V> 无参构造，建立entries及零count。 恰0参，返回完成初始化的Map；本项默认flags0，没有GET/SET的RESULT_OPTIONAL或栈根快路径flag。 */
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_map_constructor, "Map<K,V>", ZR_NULL, ZR_NULL, 0},
        /* 绑定 Map GET_ITEM 普通getter及只读内联快读，返回V或null。 恰1参 key:K；STACK_ROOT_CONTEXT/NO_SELF_REBIND/INLINE_VALUE_CONTEXT/RESULT_ALWAYS_WRITTEN/READONLY_INLINE_VALUE_CONTEXT/READONLY_RECEIVER；inline缺backing false由派发转换，缺键自身null/true。 */
        {.metaType = ZR_META_GET_ITEM,
         .minArgumentCount = 1,
         .maxArgumentCount = 1,
         .callback = zr_container_map_get_item,
         .returnTypeName = "V",
         .documentation = ZR_NULL,
         .parameters = kMapKeyParameter,
         .parameterCount = ZR_ARRAY_COUNT(kMapKeyParameter),
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
         .readonlyInlineGetFastCallback = zr_container_map_get_item_readonly_inline_fast},
        /* 绑定 Map SET_ITEM 普通setter及省结果内联写，更新或新增K/V条目。 恰2参 key:K/value:V；STACK_ROOT_CONTEXT/NO_SELF_REBIND/INLINE_VALUE_CONTEXT/RESULT_ALWAYS_WRITTEN/READONLY_INLINE_VALUE_CONTEXT/RESULT_OPTIONAL；无READONLY_RECEIVER，result可省略，写入仍改变内容。 */
        {.metaType = ZR_META_SET_ITEM,
         .minArgumentCount = 2,
         .maxArgumentCount = 2,
         .callback = zr_container_map_set_item,
         .returnTypeName = "V",
         .documentation = ZR_NULL,
         .parameters = kMapSetItemParameters,
         .parameterCount = ZR_ARRAY_COUNT(kMapSetItemParameters),
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL,
         .readonlyInlineSetNoResultFastCallback = zr_container_map_set_item_readonly_inline_no_result_fast},
};

/* Set 公开 count 元数据；实际搜索长度仍取 backing。 */
static const ZrLibFieldDescriptor kSetFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
};
/* Set 普通操作及 T 迭代入口绑定，不提供下标元方法。 */
static const ZrLibMethodDescriptor kSetMethods[] = {
        /* 绑定 Set.add，按相等规则检查重复后新增T。 实例恰1参 value:T，普通bool结果；重复为guest false、callback true，新增后更新count，失败无事务回滚。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, zr_container_set_add, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        /* 绑定 Set.contains，在线性entries中查找T。 实例恰1参 value:T，普通bool结果；T的哈希/相等约束不等于此实现按元素哈希O(1)查找。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("contains", 1, 1, zr_container_set_contains, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        /* 绑定 Set.remove，删除首个相等T并同步count。 实例恰1参 value:T，普通bool结果；缺失或存储移除失败为guest false，generic位移失败传播待核。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_set_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        /* 绑定 Set.clear，复用entries空间并清零count。 实例无参，普通null结果；快速clear不逐值release，owner元素可达限制与析构需核查。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_set_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        /* 绑定 Set.getIterator，供 Iterable<T> 枚举实际entries。 实例无参，ITERABLE_INIT角色；GC迭代器source保持backing可达，不捕获修改版本。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_set_get_iterator, "zr.iteration.Enumerator<T>", ZR_NULL,
                                           ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* Set 仅公开无参构造元方法。 */
static const ZrLibMetaMethodDescriptor kSetMetaMethods[] = {
        /* 绑定 Set<T> 无参构造，建立entries和零count。 恰0参，返回Set<T>，默认flags0；T哈希/相等约束在type泛型表，本项没有下标元方法。 */
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_set_constructor, "Set<T>", ZR_NULL, ZR_NULL, 0},
};

/* Pair 两个泛型分量字段，字段描述符默认可写。 */
static const ZrLibFieldDescriptor kPairFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("first", "K", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("second", "V", ZR_NULL),
};
/* Pair 相等/字典序/哈希普通方法入口。 */
static const ZrLibMethodDescriptor kPairMethods[] = {
        /* 绑定 Pair.equals，确认同owner Pair再比较两个分量。 实例恰1参 other:Pair<K,V>，普通bool结果；对象equals可回调，K/V没有本表新增约束。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("equals", 1, 1, zr_container_pair_equals, "bool", ZR_NULL, ZR_FALSE,
                                      kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)),
        /* 绑定 Pair.compareTo，按first后second字典序比较。 实例恰1参 other:Pair<K,V>，普通int结果；与COMPARE共用pair_compare，合法int64数字回退有已记录BUG。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareTo", 1, 1, zr_container_pair_compare, "int", ZR_NULL, ZR_FALSE,
                                      kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)),
        /* 绑定 Pair.hashCode，组合两个分量的哈希供IHashable使用。 实例无参，普通int结果；用户hashCode可能置线程异常，本回调仍true，由外层派发检查。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("hashCode", 0, 0, zr_container_pair_hash_code, "int", ZR_NULL, ZR_FALSE, ZR_NULL,
                                      0),
};
/* Pair 零或两参构造，以及与 compareTo 共用的 COMPARE 回调。 */
static const ZrLibMetaMethodDescriptor kPairMetaMethods[] = {
        /* 绑定 Pair<K,V> 结构构造，初始化first/second。 元数据0..2参 first:K/second:V，回调只允许0或2并拒绝1；返回普通STRUCT值，复制不保证共享对象身份。 */
        {ZR_META_CONSTRUCTOR, 0, 2, zr_container_pair_constructor, "Pair<K,V>", ZR_NULL, kPairParameters, ZR_ARRAY_COUNT(kPairParameters)},
        /* 绑定 Pair COMPARE 到与compareTo相同的字典序回调。 恰1参 other:Pair<K,V>，普通int结果，默认flags0；数值回退BUG可经该合法元方法到达。 */
        {ZR_META_COMPARE, 1, 1, zr_container_pair_compare, "int", ZR_NULL, kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)},
};

/* 链表 count/first/last 可见字段，内部修改需要保持它们与链接一致。 */
static const ZrLibFieldDescriptor kLinkedListFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("first", "LinkedNode<T>", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("last", "LinkedNode<T>", ZR_NULL),
};
/* 链表头尾新增/移除、线性删除和遍历绑定表。 */
static const ZrLibMethodDescriptor kLinkedListMethods[] = {
        /* 绑定 LinkedList.addFirst，新建含T的节点并连接为首节点。 实例恰1参 value:T，返回GC LinkedNode<T>；node.value保留元素，失败可能已有部分链接修改。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("addFirst", 1, 1, zr_container_linked_list_add_first, "LinkedNode<T>", ZR_NULL,
                                      ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        /* 绑定 LinkedList.addLast，新建含T的节点并连接为尾节点。 实例恰1参 value:T，返回GC LinkedNode<T>；外部node引用可在摘链后继续保活value。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("addLast", 1, 1, zr_container_linked_list_add_last, "LinkedNode<T>", ZR_NULL,
                                      ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        /* 绑定 LinkedList.removeFirst，复制首节点value再摘链。 实例无参，返回T或空表null；Value_Copy处理结果ownership，摘链不清空node.value。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeFirst", 0, 0, zr_container_linked_list_remove_first, "T", ZR_NULL,
                                      ZR_FALSE, ZR_NULL, 0),
        /* 绑定 LinkedList.removeLast，复制尾节点value再摘链。 实例无参，返回T或空表null；先复制值，后摘链接/count，多个setter并非事务。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeLast", 0, 0, zr_container_linked_list_remove_last, "T", ZR_NULL, ZR_FALSE,
                                      ZR_NULL, 0),
        /* 绑定 LinkedList.remove，沿next删除首个相等value。 实例恰1参 value:T，普通bool结果；用户equals可运行代码，遍历没有版本快照或原子删除保证。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_linked_list_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        /* 绑定 LinkedList.clear，断开节点链接并重置首尾/count。 实例无参，普通null结果；保留node.value，外部node持有者仍可达元素，失败可部分清链。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_linked_list_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL,
                                      0),
        /* 绑定 LinkedList.getIterator，为Iterable<T>建立nextNode游标。 实例无参，ITERABLE_INIT角色；source保持list可达，之后跟随当前节点链接，未提供修改检测。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_linked_list_get_iterator, "zr.iteration.Enumerator<T>",
                                           ZR_NULL, ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* LinkedList 无参空表构造元方法。 */
static const ZrLibMetaMethodDescriptor kLinkedListMetaMethods[] = {
        /* 绑定 LinkedList<T> 空表构造，first/last为空且count为零。 恰0参，返回GC LinkedList<T>，默认flags0；本项不分配节点或元素backing。 */
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_linked_list_constructor, "LinkedList<T>", ZR_NULL, ZR_NULL, 0},
};

/* 节点 value 及双向链接，脱链仍保留 value 可达性。 */
static const ZrLibFieldDescriptor kLinkedNodeFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("value", "T", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("next", "LinkedNode<T>", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("previous", "LinkedNode<T>", ZR_NULL),
};
/* LinkedNode 零或一参构造：省略元素时 value 为空。 */
static const ZrLibMetaMethodDescriptor kLinkedNodeMetaMethods[] = {
        /* 绑定 LinkedNode<T> 构造，初始化value和空双向链接。 0..1参 value:T，省略时value为空；返回GC LinkedNode<T>，脱链不等于释放value。 */
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_linked_node_constructor, "LinkedNode<T>", ZR_NULL, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
};

/* 共享视图 source/start/length 角色表；readonly 视图复用字段表，写限制主要由协议/元方法决定。 */
static const ZrLibFieldDescriptor kSpanFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("source", "object", ZR_NULL,
                                          ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SOURCE),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("start", "int", ZR_NULL,
                                          ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_START),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("length", "int", ZR_NULL,
                                          ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH),
};

/* 可写视图的 slice 与 asReadOnly 角色；源区间由 contiguous_view 实现检查。 */
static const ZrLibMethodDescriptor kSpanMethods[] = {
        /* 绑定 Span.slice 到ContiguousView_Slice，创建同源相对区间。 实例恰2参 start:int/length:int，VIEW_SLICE角色；返回Span<T>，来源及合法边界由视图回调验证，不复制元素。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("slice", 2, 2, ZrVmLibContainer_ContiguousView_Slice,
                                           "Span<T>", ZR_NULL, ZR_FALSE,
                                           kSpanSliceParameters, ZR_ARRAY_COUNT(kSpanSliceParameters),
                                           ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE),
        /* 绑定 Span.asReadOnly，保留同源区间并转换只读视图类型。 实例无参，READONLY_VIEW_CONVERSION角色；返回ReadOnlySpan<T>而非冻结原数组，来源生命周期仍须满足借用约束。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("asReadOnly", 0, 0,
                                           ZrVmLibContainer_ContiguousView_AsReadOnly,
                                           "ReadOnlySpan<T>", ZR_NULL, ZR_FALSE,
                                           ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION),
};

/* 只读视图 slice 返回同只读类型，不提供转回可写入口。 */
static const ZrLibMethodDescriptor kReadOnlySpanMethods[] = {
        /* 绑定 ReadOnlySpan.slice，切分同源只读区间。 实例恰2参 start:int/length:int，VIEW_SLICE角色；返回ReadOnlySpan<T>，无转回可写视图接口。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("slice", 2, 2, ZrVmLibContainer_ContiguousView_Slice,
                                           "ReadOnlySpan<T>", ZR_NULL, ZR_FALSE,
                                           kSpanSliceParameters, ZR_ARRAY_COUNT(kSpanSliceParameters),
                                           ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE),
};

/* 可写视图空构造与相对索引 GET/SET 入口。 */
static const ZrLibMetaMethodDescriptor kSpanMetaMethods[] = {
        /* 绑定 Span<T> 空构造到ContiguousView_Construct，重置空区间。 恰0参，返回RefLike Span<T>，默认flags0；type允许值构造但禁止装箱，不获得元素所有权。 */
        {ZR_META_CONSTRUCTOR, 0, 0, ZrVmLibContainer_ContiguousView_Construct,
         "Span<T>", ZR_NULL, ZR_NULL, 0},
        /* 绑定 Span GET_ITEM，按相对index读取来源元素。 恰1参 index:int，READONLY_RECEIVER；返回T，实际区间/来源检查在ContiguousView_GetItem，本flag不禁止Span SET_ITEM。 */
        {ZR_META_GET_ITEM, 1, 1, ZrVmLibContainer_ContiguousView_GetItem,
         "T", ZR_NULL, kArrayIndexParameter,
         ZR_ARRAY_COUNT(kArrayIndexParameter), ZR_NULL, 0,
         ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER},
        /* 绑定 Span SET_ITEM，按相对index修改来源并返回赋值结果。 恰2参 index:int/value:T，默认flags0；来源数组或lease有效性和边界由ContiguousView_SetItem校验，视图不拥有元素。 */
        {ZR_META_SET_ITEM, 2, 2, ZrVmLibContainer_ContiguousView_SetItem,
         "T", ZR_NULL, kArrayInsertParameters,
         ZR_ARRAY_COUNT(kArrayInsertParameters)},
};

/* 只读视图空构造和 GET 入口，没有 SET_ITEM。 */
static const ZrLibMetaMethodDescriptor kReadOnlySpanMetaMethods[] = {
        /* 绑定 ReadOnlySpan<T> 空构造到公共视图构造回调。 恰0参，返回RefLike ReadOnlySpan<T>，允许值构造禁止装箱；本项不声明SET_ITEM。 */
        {ZR_META_CONSTRUCTOR, 0, 0, ZrVmLibContainer_ContiguousView_Construct,
         "ReadOnlySpan<T>", ZR_NULL, ZR_NULL, 0},
        /* 绑定 ReadOnlySpan GET_ITEM，按相对index读取来源元素。 恰1参 index:int，READONLY_RECEIVER；返回T，来源可由其他持有者修改，本类型无SET_ITEM而非复制冻结快照。 */
        {ZR_META_GET_ITEM, 1, 1, ZrVmLibContainer_ContiguousView_GetItem,
         "T", ZR_NULL, kArrayIndexParameter,
         ZR_ARRAY_COUNT(kArrayIndexParameter), ZR_NULL, 0,
         ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER},
};

/* 容器类型表把构造权、泛型约束及 Iterable/RefLike 协议映射到运行时回调。 */
/* 八种容器类型的反射/泛型/协议表；Pair 是普通 STRUCT，Span 两型是 RefLike 且禁止装箱构造。 */
static const ZrLibTypeDescriptor g_container_types[] = {
        /* 声明Array<T>类，提供索引和T迭代接口，连接Array字段/普通方法/元方法表。 单T无额外约束；CLASS、允许值与装箱构造，ARRAY_LIKE|ITERABLE协议；length角色用于索引长度，隐藏items由回调维护。 */
        {"Array", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kArrayFields, ZR_ARRAY_COUNT(kArrayFields), kArrayMethods, ZR_ARRAY_COUNT(kArrayMethods), kArrayMetaMethods, ZR_ARRAY_COUNT(kArrayMetaMethods), ZR_NULL, ZR_NULL, kArrayImplements, ZR_ARRAY_COUNT(kArrayImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Array<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ARRAY_LIKE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明Map<K,V>类，迭代Pair<K,V>并提供键下标读写。 K受IHashable/IEquatable<K>约束、V无附加约束；CLASS、允许值与装箱构造、ITERABLE协议；count可见但实际长度取entries。 */
        {"Map", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kMapFields, ZR_ARRAY_COUNT(kMapFields), kMapMethods, ZR_ARRAY_COUNT(kMapMethods), kMapMetaMethods, ZR_ARRAY_COUNT(kMapMetaMethods), ZR_NULL, ZR_NULL, kMapImplements, ZR_ARRAY_COUNT(kMapImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Map<K,V>()", kMapGenericParameters, ZR_ARRAY_COUNT(kMapGenericParameters), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明Set<T>类，提供去重元素操作及T迭代。 T受IHashable/IEquatable<T>约束；CLASS、允许值与装箱构造、ITERABLE协议；无下标元方法，当前查询线性扫描。 */
        {"Set", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kSetFields, ZR_ARRAY_COUNT(kSetFields), kSetMethods, ZR_ARRAY_COUNT(kSetMethods), kSetMetaMethods, ZR_ARRAY_COUNT(kSetMetaMethods), ZR_NULL, ZR_NULL, kSetImplements, ZR_ARRAY_COUNT(kSetImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Set<T>()", kSetGenericParameters, ZR_ARRAY_COUNT(kSetGenericParameters), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明Pair<K,V>普通结构，提供分量、相等、排序及哈希方法。 K/V无附加泛型约束；STRUCT、允许值与装箱构造，EQUATABLE|COMPARABLE|HASHABLE协议；无RefLike位，值复制不能当共享别名。 */
        {"Pair", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kPairFields, ZR_ARRAY_COUNT(kPairFields), kPairMethods, ZR_ARRAY_COUNT(kPairMethods), kPairMetaMethods, ZR_ARRAY_COUNT(kPairMetaMethods), ZR_NULL, ZR_NULL, kPairImplements, ZR_ARRAY_COUNT(kPairImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Pair<K,V>(first: K, second: V)", kGenericKV, ZR_ARRAY_COUNT(kGenericKV), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)},
        /* 声明LinkedList<T>类，提供首尾节点操作和T迭代。 单T无额外约束；CLASS、允许值与装箱构造、ITERABLE协议；GC节点链接维护count/首尾，不使用entries数组。 */
        {"LinkedList", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedListFields, ZR_ARRAY_COUNT(kLinkedListFields), kLinkedListMethods, ZR_ARRAY_COUNT(kLinkedListMethods), kLinkedListMetaMethods, ZR_ARRAY_COUNT(kLinkedListMetaMethods), ZR_NULL, ZR_NULL, kLinkedListImplements, ZR_ARRAY_COUNT(kLinkedListImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedList<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明LinkedNode<T>类，保存value及双向链接。 单T无额外约束；CLASS、允许值与装箱构造、protocolMask0；零或一参构造，脱链后value仍可由外部节点引用保活。 */
        {"LinkedNode", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedNodeFields, ZR_ARRAY_COUNT(kLinkedNodeFields), ZR_NULL, 0, kLinkedNodeMetaMethods, ZR_ARRAY_COUNT(kLinkedNodeMetaMethods), ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedNode<T>(value: T)", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), 0},
        /* 声明可写Span<T>连续非拥有视图，接入区间字段与slice/read/write回调。 单T无额外约束；STRUCT、允许值构造禁止装箱，REF_LIKE|CONTIGUOUS_VIEW_MUTABLE协议；source/start/length来源与边界须有效。 */
        {.name = "Span",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kSpanFields,
         .fieldCount = ZR_ARRAY_COUNT(kSpanFields),
         .methods = kSpanMethods,
         .methodCount = ZR_ARRAY_COUNT(kSpanMethods),
         .metaMethods = kSpanMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kSpanMetaMethods),
         .documentation = "Mutable non-owning contiguous view.",
         .allowValueConstruction = ZR_TRUE,
         .allowBoxedConstruction = ZR_FALSE,
         .constructorSignature = "Span<T>()",
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                         ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_MUTABLE)},
        /* 声明ReadOnlySpan<T>连续非拥有只读视图，复用区间字段并接入slice/read回调。 单T无额外约束；STRUCT、允许值构造禁止装箱，REF_LIKE|CONTIGUOUS_VIEW_READONLY协议；无SET_ITEM，不冻结其他持有者可写来源。 */
        {.name = "ReadOnlySpan",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kSpanFields,
         .fieldCount = ZR_ARRAY_COUNT(kSpanFields),
         .methods = kReadOnlySpanMethods,
         .methodCount = ZR_ARRAY_COUNT(kReadOnlySpanMethods),
         .metaMethods = kReadOnlySpanMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kReadOnlySpanMetaMethods),
         .documentation = "Read-only non-owning contiguous view.",
         .allowValueConstruction = ZR_TRUE,
         .allowBoxedConstruction = ZR_FALSE,
         .constructorSignature = "ReadOnlySpan<T>()",
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                         ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_READONLY)},
};

/* 描述符是 parser、原生绑定与运行时共享的类型/协议契约来源。 */
/* v1 运行时 provider 静态描述符；借用表和回调必须活到 registry 最后一次使用。 */
static const ZrLibModuleDescriptor g_container_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.container",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = ZR_NULL,
        .functionCount = 0,
        .types = g_container_types,
        .typeCount = ZR_ARRAY_COUNT(g_container_types),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Built-in generic container and interface module.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = ZR_NULL,
        .minRuntimeAbi = 0,
        .requiredCapabilities = 0,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
        .publicContractHash = "zr.container:v1:container-span-protocols",
};

/** @brief 返回进程静态 zr.container 描述符借用指针；宿主/registry 不应释放，插件卸载前须停止使用其中回调。 */
const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void) {
    return &g_container_module_descriptor;
}

/** @brief 转交 pooling provider 的静态描述符；不注册、不创建池，注册消费者仍须先具备容器契约。 */
const ZrLibModuleDescriptor *ZrVmLibContainer_GetPoolingModuleDescriptor(void) {
    return ZrVmLibContainer_Pooling_GetModuleDescriptor();
}

/**
 * ## Summary
 * 在模块注册时为内建ARRAY原型安装具名永久native闭包。
 *
 * ## Parameters
 * @param[in,out] state 有效主线程状态及所属global；基本类型原型已初始化。
 * @param[in] methodName 调用期间有效的NUL结尾字段名，当前caller为getIterator。
 * @param[in] nativeFunction 非NULL的native函数入口指针。
 *
 * ## Returns
 * @return 空参数、缺原型或闭包分配普通失败false；同名字段非NULL，或void SetFieldCString调用返回后均true。
 *
 * ## Constraints and lifecycle
 * 注册阶段调用，不承诺并发注册或事务回滚。TODO: 同名检查只确认字段存在，没有验证callable/Enumerator契约。引用Register已审双阶段静态BUG：原型缺getIterator且getter与setter各自ignore扩容都满足拒绝条件时，getter失败被看成缺字段，后续void setter可静默失败，而本helper及Register仍成功；完整条件沿Register/SetFieldCString文档，不把单次setter拒绝当作完整证明。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7826;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7844;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7848;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7857;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7858;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7908;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7910;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1383;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1408
 */
static TZrBool zr_container_install_basic_array_method(SZrState *state,
                                                       const TZrChar *methodName,
                                                       FZrNativeFunction nativeFunction) {
    SZrObjectPrototype *prototype;
    SZrObject *prototypeObject;
    SZrClosureNative *closure;
    SZrTypeValue closureValue;

    if (state == ZR_NULL || state->global == ZR_NULL || methodName == ZR_NULL || nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    prototypeObject = &prototype->super;
    if (ZrLib_Object_GetFieldCString(state, prototypeObject, methodName) != ZR_NULL) {
        return ZR_TRUE;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    ZrCore_Value_InitAsRawObject(state, &closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue.isNative = ZR_TRUE;
    ZrLib_Object_SetFieldCString(state, prototypeObject, methodName, &closureValue);
    /* BUG: SetFieldCString 是 void，分配或写入失败时可提前返回；此处仍报成功，
     * Register 可在内建 array 缺少 getIterator 时返回 true。证据见
     * native_binding_dispatch.c 的 SetFieldCString 失败路径；需用故障注入覆盖。
     */
    return ZR_TRUE;
}

/**
 * ## Summary
 * 把内建ARRAY迭代协议接到本模块native桥接，作为Register末端适配步骤。
 *
 * ## Parameters
 * @param[in,out] global 已初始化且持有有效mainThreadState的global；在其主状态执行适配。
 *
 * ## Returns
 * @return global/mainThreadState缺失false；否则透传install_basic_array_method结果。
 *
 * ## Constraints and lifecycle
 * 固定传入getIterator及native_array_get_iterator_native，仅在Register前序登记成功后调用。原型资格、既存非callable字段与void setter失败均由helper契约界定；引用其已有双阶段BUG/TODO，不增加重复身份。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7926;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7881;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7886;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7888;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7922
 */
static TZrBool zr_container_install_basic_array_adapter(SZrGlobalState *global) {
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_install_basic_array_method(global->mainThreadState,
                                                   "getIterator",
                                                   zr_container_native_array_get_iterator_native);
}

/* 先注册迭代依赖，再注册 Span 和引用其契约的池，最后桥接内建 array 迭代。 */
/**
 * ## Summary
 * 为宿主的标准模块启动链登记 iteration、container、pooling，再给内建 ARRAY 安装 getIterator 桥接。
 * 类型依赖先于池和基本数组适配，确保普通容器、Span、池和原生 ARRAY 可接入同一迭代协议。
 *
 * ## Parameters
 * @param[in,out] global 宿主拥有、已初始化主线程及基本 ARRAY 原型的有效 VM；调用期间借用，不接管或释放。
 * native registry 可尚未存在，iteration/RegisterModule 链会按需 Attach；不用由宿主预先初始化 registry。
 *
 * ## Returns
 * @return global 为空或任一登记/适配阶段拒绝时为 ZR_FALSE；正常完成全部阶段时为 ZR_TRUE。
 * 前序成功登记在后续失败时保留，没有事务回滚；返回 true 不证明已有 getIterator 字段满足调用协议。
 *
 * ## Constraints and lifecycle
 * 沿主 VM 初始化窗口串行调用；不会切换 provider phase。静态描述符、嵌套表和回调仍依赖 provider 存续。
 * TODO: 已有同名非 callable 字段也被当作适配完成，需沿 install_basic_array_method 的 presence 检查核对。
 * BUG: void setter 不能反馈字段写失败。合法已初始化 global、ARRAY 原型未 ignored、ignored 表已满且无 getIterator 时，
 * 若先拒绝 getter pin 的 ignore 扩容，允许中间闭包创建，再拒绝 setter pin 的扩容，适配与 Register 仍可 true 而字段缺失。
 * 这是一条完整静态可达链，未运行故障注入；仅拒绝一次 setter 扩容不能替代该双阶段条件。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/project/project.c:368 消费 bool；tests/container/container_test_common.c:60 调用但未检查返回。
 * zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c:21 及 zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:739 按需 Attach；本文件 zr_vm_lib_container/src/zr_vm_lib_container/module.c:7922 至 zr_vm_lib_container/src/zr_vm_lib_container/module.c:7926 顺序登记/适配。
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:7844 检查已有字段，zr_vm_lib_container/src/zr_vm_lib_container/module.c:7857 写入，zr_vm_lib_container/src/zr_vm_lib_container/module.c:7886 进入基本 ARRAY 适配。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1429 与 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385 是 getter/setter pin 入口。
 */
TZrBool ZrVmLibContainer_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrVmLibIteration_Register(global) &&
           ZrLibrary_NativeRegistry_RegisterModule(global, &g_container_module_descriptor) &&
           ZrLibrary_NativeRegistry_RegisterModule(
                   global, ZrVmLibContainer_Pooling_GetModuleDescriptor()) &&
           zr_container_install_basic_array_adapter(global);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 共享库加载器按 v1 符号取得描述符；不等同 Register，不在此注册依赖或安装内建 array 桥接。 */
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibContainer_GetModuleDescriptor();
}
#endif
