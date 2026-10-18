---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_object_conf.h
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - tests/core/test_gc_root_leases.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/frame_slot_layout_initialization_tests.inc
  - tests/core/test_ssa_platform_matrix.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_object_conf.h
plan_sources:
  - "user: 全仓首方全量审查、按需补中文契约注释；保持程序行为和公开 API。"
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_gc_root_leases.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/frame_slot_layout_initialization_tests.inc
  - tests/core/test_ssa_platform_matrix.c
doc_type: module-detail
---

# Object 对象标记与 GC 配置契约审查

本批全量复核 zr_object_conf.h 的25个旧单元：10个宏、14个枚举和1个结构，新增覆盖键为0。21项已注释、1项既有TODO保留、3项有证据地无需新增注释；22段中文Doxygen说明职责、使用时机、值模型、不变量、必要约束、生命周期和引用。每项定义、调用方、入口、测试证据、契约及无需注释理由见[九列表](../coverage/zr_vm_common.tsv)。

GC配置宏提供默认值或编译开关，不独立执行收集或提供保活、pin和线程同步。debug检查还受运行、immediate和concurrent major状态限制。scope的NONE表示没有锚；增量A/B周期、young/old堆代、region类别和storage类别分别解释。对象标记的root/pin计数须遵守共享域修改协议；失败只保证本次新增引用和token未发布，不承诺registry扩容或marking事务回滚。释放自身pin来源不能撤销其他持有者的来源。

原forwardingRefLocation疑点仍为同一TODO：当前有限检索只见字段声明、四处NULL写入和旧注释，未见有效引用位置写入或读取协议；核查入口为gc_cycle搬迁和引用重写路径。不据此判定死代码、可删除或已复现BUG。原TODO字面标签1处，新Doxygen镜像1处，当前共2处，新增问题身份为0。

当前调用补证在3975成员、132符号的既定检索域重建原2552与当前2557出现序列，原命中全部保留，只有5处新增代码引用及8处物理坐标迁移。新引用均比较extern原型的STRUCT/ENUM标签及声明、字段或metadata身份，没有引入本批GC契约变化。原17文件的115条引用EOL锚和3个补充整文件保持；八个外域whole变化另列证据，不声称原3017文件全部当前相同。词法命中并不自动代表函数调用、运行覆盖、预处理后语义或仓外入口缺失。

源文件只在EOF追加22段批准文档：原10708B、252行、所有程序和字符串字节、原行号、UTF8列、EOL与绝对偏移均原样保留；去掉精确23769B后缀可恢复原件。公开声明及程序行为不变。

修改前基线采用Common真实AFTER-r2的已构建、已执行端点。Common55083ca与两份workflow文档元数据daee312提交后，Root以23209e0f93ddf94a6fec026fce88c703a2e6b5778e23cc29547ff27b6b6da50b桥接重新核2019输入、1171头成员、17工具、21生成项、203cache、12目标契约、316有效依赖记录、897实际输入及331运行产物；没有补造一次Object BEFORE运行。两阶段有限输入只改变Object头，其余2018项保持。所选对象中300个实际消费该头，不能将消费者数或316闭包数当作实际重编译数量。

应用注释后实际重新构建四root，并运行原GC7 selector、Frame39 Unity项及Matrix20场景函数，全部退出0且完整等待。Math只获静态编译证据。Frame保留原断言、2000000/20000迭代数和39个PASS ID/footer；配对仅归一5条sample和1条median的dense/sparse tick共12个数字，其他运行流原始字节比较。Matrix20不是平台数；Frame中的内存布局样例不授文件解码兼容性信用。每阶段331运行身份独立冻结，不要求obj、archive、EXE或PDB跨阶段字节相等。

证据：

- common-scalar-io-gc-config-current-contract-review-r1.json、原独审19项typed更正及Object47补证/独审：有效25合同与22文档。
- common-object-header-current-caller-supplement-r1.json及独审da36197d50fa4fd21692132504891aa3325fc74aeda618c15a2c6a1d6dd82444：当前调用与普通清理边界。
- common-object-header-CommonAF-baseline-bridge-r1.json及source-comment-application-r1.json：真实基线采用、精确EOF应用和逆向证明。
- common-object-header-native-after-r1的frozen plan、唯一driver独审、results/process/selected-input-closure和Root真实outer终态。
- common-object-header-native-final-acceptance-r1.json：fc367a761d460bc925634edb9fab4e8e7513b000847aee5dee3f59c7f54adb8e，只接受本批有限注释配对，保留Common旧r1守门失败原件，不转成运行成功。

check-batch和暂存空白检查要求实际成功。全仓inventory原有陈旧状态的相同失败不称全仓通过。共享leaf仅更新25旧键，Common60及全部外围441物理行保留；只提交Object头、这些台账、本批说明、所属inventory三格和导航。未新增重复实现的测试。仅Windows clang-cl/MSVC Debug有限证据，无Linux/WSL/CTest或全平台通过结论。产物生命周期OPEN，保留复用E构建与必要失败证据，源码副本0，手工释放0B。
