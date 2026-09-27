/** 与 package.json 的 debugger contribution 共用类型名，连接 provider 与内联 adapter。 */
export const ZR_DEBUG_TYPE = 'zr';
/** zrdbg/1 当前以一个 DAP 线程展示运行状态，线程列表与 stopped 事件必须使用同一 ID。 */
export const ZR_DEBUG_MAIN_THREAD_ID = 1;
/** 单线程调试模型在 VS Code 线程视图中的展示名称。 */
export const ZR_DEBUG_MAIN_THREAD_NAME = 'ZR Main';
/** 编辑器 .zrp 标题菜单及命令面板入口；须与 package.json 声明一致。 */
export const ZR_DEBUG_CURRENT_PROJECT_COMMAND = 'zr.debugCurrentProject';
/** 项目树、状态栏及命令面板共享的调试入口，项目由工作区选择规则决定。 */
export const ZR_DEBUG_SELECTED_PROJECT_COMMAND = 'zr.debugSelectedProject';
/** 连接外部已启动调试目标的命令；桌面和 Web 均注册，Web 给出不可用提示。 */
export const ZR_DEBUG_ATTACH_COMMAND = 'zr.attachDebugEndpoint';
/** TCP 客户端和 DAP 适配器核对 initialize 响应的协议版本，拒绝不兼容运行时。 */
export const ZRDBG_PROTOCOL = 'zrdbg/1';
