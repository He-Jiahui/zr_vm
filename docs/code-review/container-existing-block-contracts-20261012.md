# 容器现有风险语句块的调用契约复核

日期：2026-10-12。此批细化八条已有源码注释的逐单元台账；未解决的行为 TODO 继续保留。

## 范围

- 当前工作树源码 `zr_vm_lib_container/src/zr_vm_lib_container/module.c` SHA256 `cf386294386438827ce096a97f41aaaa0c2b09b1d9a0dcc3525b2f163de6b448`。既有Doxygen的职责、调用前提和失败边界已逐项复核。
- 当前源码另有目录迁移产生的引用路径改动，尚未由目录迁移负责人提交；本批只提交自己的台账、清单和本文。
- 主文件详细审查从228推进到236/357，剩121，文件仍为pending；module.h的12项结论保持。全台账有16个meaningful-block，本批是指定剩余10项中的8项。
- values_compare函数、其numeric-order块、kArrayMetaMethods与installer的void-setter块仍待处理。未应用的五函数及40元数据候选不计入本批。
- 本批没有源码改动、新BUG身份、构建或测试运行；此前单TU编译是迁移前的历史证据，不证明当前目录布局可构建。

## zr_container_hash_set_clear_reuse_storage::pool-reset-without-release

为Array/Map/Set clear共用的internal ARRAY backing清空HashSet桶及pool使用游标，保留分配以供后续写入；本层不是逐值析构接口。

已初始化的internal ARRAY backing，功能调用面包括Array/Map/Set的共享clear路径；无效set可不操作，caller维持宿主和节点值生存期。clear丢链而不逐值Release owner或独立非pool Pair。既有窄BUG的合法C Shared写入链与零初始capacity加insert链只限正常Array、VM存活期；Map/Set owner和VM销毁后永久泄漏未被证明。TODO: 明确值owner、非pool Pair分配与池复用/解构的成对清理职责。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:3544`。

现有Doxygen：3491–3506，含物理换行的SHA256 `b337226ee2cfd06f6b44bd8a33ad10a46ef9e65997cd75e91c641c74cae8e4fa`。

结论保留 `TODO`。两条既有静态合法Array清理链及复用职责已在当前Dox写明；现有Array<int> fixture只核内容不可见与capacity保留，未跑owner/分配清理复现，不增BUG身份。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_storage_push_gc_value_dense_pair_pool_fast::dense-copy-publication

为Array.add以预留PairPool快速追加GC值，协调值复制、bucket/count/表示版本发布和写屏障。

同域GC借用载荷及initialized非RAW backing；false可能已扩桶/池，外层仍回退generic；true后公开length发布失败也不回滚。Void Copy/Barrier结果和pool扩容时backing地址稳定尚未验收。TODO: 对照Core Copy/Barrier及Array.add同步长度的失败、可表示容量和借用生存契约。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:5801`。

现有Doxygen：4179–4196，含物理换行的SHA256 `5eb697258509473011a52e14263e292c8ade61b3c64f05dad5e81b3a13aac999`。

结论保留 `commented`。现有Dox已解释发布顺序、域前提及两种失败时序；没有完整合法失败链或运行证明，保留原commented及具体TODO，不增BUG。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_storage_insert::unchecked-right-shift

为generic Array.insert逐项右移并写插入值；原生整数raw表示走独立快路，包装length由外层提交。

index不超过实际backing长度；generic借用source槽，移位setter逐次结果丢弃，只以最终插入setter决定返回；无快照、事务或回滚。TODO: 对中途拒写/线程错误、最终bool及公开length关系作失败注入，核借用槽跨STRUCT复制/分配的有效期。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:5846`。

现有Doxygen：4391–4409，含物理换行的SHA256 `74e9754dd92be73f3db63e6d7e3d8a0cf433035e6d39c9aeb31eedc92c39e493`。

结论保留 `TODO`。函数Dox和局部TODO已明确未检查的右移边界；失败注入未运行，不能写成已观测部分损坏或另增BUG。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_storage_remove_at::unchecked-left-shift

承接Array/Map/Set generic backing删除的左移与末项移除；外层据返回值更新length/count。

index有效且整数键可表示；raw单独处理。generic移位setter的结果未逐次传播，仅返回末项删除结果，无回滚。TODO: 核错误状态与包装计数同步。函数已有正常Array Shared末项owner脱链BUG属于另一身份，不据此将本移位TODO升BUG或泛化Map/Set ownership。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:5882;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6367;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6881`。

现有Doxygen：4445–4462，含物理换行的SHA256 `d6559854466ac0910e511a2085787dd3d90a0869f36b85375458585c464ed60c`。

结论保留 `TODO`。当前Dox完整区分移位失败待核与已有Array owner-delete BUG；保留原TODO，未跑失败注入、未重复计算缺陷。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_array_ensure_capacity::growth-overflow

在Array.add/insert前更新包装对象capacity的增长政策；此字段声明容量不预约实际backing空间。

requiredLength来自backing长度加一；caller保证非负int64/TZrSize可表示边界。signed倍增和caller加法未统一检查上界，字段增大后存储失败不回滚。TODO: 核极大合法C输入、倍增终止与配置初值；尚无合法可达溢出证明。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:5798;zr_vm_lib_container/src/zr_vm_lib_container/module.c:5845`。

现有Doxygen：5093–5110，含物理换行的SHA256 `7ec4cfbf1576ae5a9e117d0798434c6eb4a32c909b46cf07565fdd7a8bd12271`。

结论保留 `TODO`。现有Dox及极值TODO解释包装元数据和真实分配的职责；普通容量测试未证明上限，不升BUG、无极值运行信用。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_iterator_make::source-root-edge

给Array、内建ARRAY、Map、Set及LinkedList迭代入口建立Enumerator，发布source、index与nextNode。

各caller维持来源标签和来源含义一致；实例字段成功发布后提供GC可达边，非RootHandle或owner转移；不记长度/版本快照。失败可留部分实例/字段。TODO: 核构造期borrow跨分配、移动GC地址稳定和来源单独保活边界。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:6102;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6153;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6447;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6961;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7387`。

现有Doxygen：5172–5193，含物理换行的SHA256 `3fb8e035aefbf3dc7538075f7f627d64efedd5ccd07c6a56590741c3bc2d19e2`。

结论保留 `commented`。当前Dox已明确来源映射和对象图发布边；未证明构造期地址稳定或UAF，不增BUG，未运行遍历/GC边界，保留commented及TODO。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_map_set_item_core::self-after-allocation

普通Map SET经此核心处理更新或新增；readonly无结果SET在缺键时才调用它新增，已有Pair由其快路直接更新；新Pair分配后重新解析self/entries再追加。

普通帧或inline stack根维护selfValue；Pair创建后重新取self/entries体现必要的跨分配解析。追加后仍使用借用entries更新count，失败无事务回滚。TODO: 核追加后地址稳定及count/结果发布。make_pair首own槽OOM pin是既有另一unit BUG，不重复计数。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:6689;zr_vm_lib_container/src/zr_vm_lib_container/module.c:6742`。

现有Doxygen：6585–6604，含物理换行的SHA256 `f7b4d3ab395a988e678cf998e8f653a7f9b6f191c8a2db7b8b0584ecb1f3401c`。

结论保留 `commented`。现有Dox覆盖分配后重新解析职责及外层root前提；追加后borrow/count仍待核，不把既有构造pin BUG重复计数，不授运行信用。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。

## zr_container_linked_list_unlink_node::unlink-partial-writes

从List摘除当前候选节点，更新邻接/端点/count并清节点链接，保留node.value供外部持有者使用。

caller提供同域有效List/Node及当下membership；void helper不自行验证成员身份，连续setter结果丢弃且无rollback。正常已有own槽不能单凭多setter证明Add OOM。TODO: 核equals后membership与部分写/跨GC借用；clear/addLast同表重入可观察条件尚无API组合保证。

调用位置：`zr_vm_lib_container/src/zr_vm_lib_container/module.c:7232;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7272;zr_vm_lib_container/src/zr_vm_lib_container/module.c:7307`。

现有Doxygen：7011–7028，含物理换行的SHA256 `fbee0650a5fd44d2494c02243112fb0f92cd74811cb3030f17d1e0c5acbe2adf`。

结论保留 `TODO`。当前Dox已解释membership前提及脱链保留value；重入/失败边界未形成完整合法失败链，仍为TODO，不将回调可重入推成嵌套操作组合承诺，不授运行信用。 当前仅详细闭合此块，未关闭其行为TODO；8项拟合计236/357、余121，源码文件仍pending。
