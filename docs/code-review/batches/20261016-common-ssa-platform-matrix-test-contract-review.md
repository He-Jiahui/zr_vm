---
doc_type: code-review-batch
related_code:
  - tests/core/test_ssa_platform_matrix.c
implementation_files:
  - tests/core/test_ssa_platform_matrix.c
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/plans/ssa/10-jit-platforms/03-platform-matrix.md
tests:
  - tests/core/test_ssa_platform_matrix.c
  - tests/cmake/ssa-tests.cmake
---

# Common SSA 平台矩阵测试契约

本批逐项审查24个函数及一个NDEBUG/assert编译协议块，共25个单元：20个static case、3个返回值夹具助手和ordinary main。当前493份台账的精确键、历史basename与专用符号检索均未发现已有定义键；只在原Common台账追加25键，不重复登记已交付Common107的定义。决策为commented24、no-comment1，没有未审项或新增BUG/TODO。

新增24段中文Doxygen、5个@param、4个@return，逐项说明测试目的、调用时机、合成值前提、首个拒绝分支、断言失败与资源生命周期。已有首行说明及紧邻预处理指令准确表达撤销NDEBUG的顺序，无需重复注释，单独登记其TU范围。原24764B/525LF程序与旧注释字节保持，测试case顺序、断言、API、配置和行为均未改，也未新增测试。

## 调用与夹具边界

三个助手只构造自动存储期的值记录，固定triple、ABI/数值/布局及callback哈希是人工样本，不是宿主、目标设备或真实回调探测。不同target/architecture需要的字段由具体case在消费前改写；例如WASM32 case调整指针宽度和对齐之后才验证。输入记录及内嵌字符串同步借用，不保存供异步使用；当前main串行执行，无共享可变全局、锁、线程、堆池或GC根。

完整24函数正文、527个记录span、58个本TU直接调用、57个Common API调用、22处strcpy及68个assert表达式已核；20个case由main显式依次调用，无Unity、RUN_TEST、setUp/tearDown、回调表或动态发现入口。找不到外TU实际同符号调用不表示测试死代码；CMake目标和普通C main构成程序入口。

assert在当前TU中于包含assert.h前取消NDEBUG，失败会abort进程；不返回Unity失败计数，也不保证继续执行后续case。返回结构体助手不借用局部对象地址，所有case仅使用栈值；失败没有堆、设备或线程资源的本地回收承诺。main正常返回0只表示此次静态场景的assert未失败。

## 首失败与有限测试证据

各case按当前校验器真实先后顺序标注到达分支。Android/iOS/WASM JIT策略、CROSS_COMPILE的运行声称、ABI/哈希漂移、来源与后端/dispatch/outcome等是值记录门禁；IsRuntimeAcceptance是单记录自洽谓词，不能认证记录来源或真实运行。Artifact的需求子集检查可能先于后面的JIT FORBIDDEN分支。测试中的ARM、移动目标、PMU、线程、JIT和native callback标签不代表相应硬件或引擎已执行，不能将后续未达分支算作覆盖。

9个当前完整依赖pin与3618个含EOL行pin支持原定义、调用、断言、构建及已交付Common注释上下文。Common H/C原程序前缀与Root53段EOF文档桥已逐字核对，作为本测试契约证据，不授Common定义或其它模块重复信用。tests/cmake/ssa-tests.cmake:970–978注册既有zr_vm_ssa_platform_matrix_test及CTest ssa_platform_matrix。

## 修改前后验证范围

旧Common AFTER之后两个外部CMake配置变化已重新采集。本批actual fresh BEFORE在当前原测试代码上构建exit0、CTest1/1、直接main exit0，完整stdout/stderr为空，10个锁定输入与3601个有限观察输入在此次运行首尾稳定。复用E盘Windows Clang19.1.5 Debug构建；两个实际源编译命令均无NDEBUG，不将旧运行倒灌为当前配置证据。

严格词法比较通过，测试原程序token、flags和原坐标相同，两次stdin raw lexer完整消费stderr并退出0。原代码BEFORE与文档AFTER构建均exit0、CTest均1/1、直接main均exit0；完整stdout/stderr逐字相同且为空，三个argv、20项cache及两个实际源编译命令相同且无NDEBUG。10个锁定输入、3601个有限观察输入在各次运行首尾稳定，前后只有本测试的24段EOF注释变化。实际source/leaf与stage独审、串行提交待完成。

既有配置/编译告警按原日志保留，不声明W0。未执行真实Android/iOS/WASM设备、PMU、线程、JIT、native callback或VM行为验证，也不声明逐case trace、全部分支、完整target/SDK依赖闭包、原子checkout、隔离暂存树、Linux或全库通过。源码/台账与实际暂存独审、串行提交另有精确证据。
