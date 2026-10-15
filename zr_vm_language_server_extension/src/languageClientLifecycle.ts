/**
 * ## Summary
 * 桌面与 Web 入口在不导入宿主 SDK 的适配层中共用 LanguageClient 状态值。
 * 这里只使用 STOPPED 与 RUNNING 来划分传输故障观察窗口。
 *
 * ## Value model
 * 数值对应 vscode-languageclient 的 State 枚举；适配层绑定时另行清零，
 * 不从客户端读取初始状态，也不把其他状态解释成健康。
 *
 * ## Constraints and lifecycle
 * TODO: 升级 vscode-languageclient 时需对照该版本导出的 State 值独立核验；
 * 当前 Node FakeClient 复用本常量，无法发现双方一起漂移。
 *
 * ## References
 * LanguageClientLike.onDidChangeState 消费状态转换；extension.ts 与 browser.ts
 * 在绑定 lifecycle 时使用同一适配器。
 */
export const LANGUAGE_CLIENT_STATE_STOPPED = 1;
export const LANGUAGE_CLIENT_STATE_RUNNING = 2;

/**
 * ## Summary
 * 表示连接错误处理器选择继续运行的 SDK 动作值；断流写错误使用此分支，
 * 让后续 closed 通知负责决定是否恢复。
 *
 * ## Value model
 * 数值对应 vscode-languageclient 的 ErrorAction.Continue。
 *
 * ## Constraints and lifecycle
 * TODO: 升级 SDK 时独立核对导出枚举；本地 FakeClient 复用此常量，不能作为漂移检查。
 *
 * ## References
 * createTransportAwareLanguageClientLifecycle.errorHandler.error 返回该动作。
 */
export const ERROR_ACTION_CONTINUE = 1;

/**
 * ## Summary
 * 表示连接关闭后可让 SDK 尝试重启客户端的动作值。
 *
 * ## Value model
 * 数值对应 vscode-languageclient 的 CloseAction.Restart。
 *
 * ## Constraints and lifecycle
 * 本动作只给 SDK 一个恢复决定；实际重启调度、等待和预算由 SDK 控制。
 * TODO: 升级 SDK 时独立核对导出枚举；FakeClient 使用相同常量，不能发现漂移。
 *
 * ## References
 * createTransportAwareLanguageClientLifecycle.errorHandler.closed 在未退休且
 * 尚未绑定委托时返回此动作。
 */
export const CLOSE_ACTION_RESTART = 2;

/**
 * ## Summary
 * 要求 SDK 不因本次关闭通知自动重启客户端，供已退休 session 阻断迟到恢复。
 *
 * ## Value model
 * 数值对应 vscode-languageclient 的 CloseAction.DoNotRestart。
 *
 * ## Constraints and lifecycle
 * 此值只影响 SDK 处理当前关闭通知的决策，不取消已经开始的初始化或其他异步工作。
 * TODO: 升级 SDK 时独立核对导出枚举；FakeClient 使用相同常量，不能发现漂移。
 *
 * ## References
 * createTransportAwareLanguageClientLifecycle.errorHandler.closed 在 isRetired
 * 返回 true 时返回此动作。
 */
export const CLOSE_ACTION_DO_NOT_RESTART = 1;

/**
 * ## Summary
 * 描述适配层观察客户端状态转换所需的最小事件形状。
 *
 * ## Fields
 * oldState 与 newState 是 SDK 转换通知中的前后状态数值；本适配层只读取 newState。
 *
 * ## Invariants and lifecycle
 * 状态订阅只报告后续转换，不在注册时补发当前状态；适配层不保存事件对象。
 *
 * ## References
 * LanguageClientLike.onDidChangeState 注册 listener，状态回调交给
 * resetTransportStateIfHealthy 更新故障观察标记。
 */
type StateChangeEventLike = {
    /** SDK 事件记录的转换前状态；当前适配器不读取该字段。 */
    oldState: number;
    /** SDK 事件记录的转换后状态，适配器据此结束传输故障观察窗口。 */
    newState: number;
};

/**
 * ## Summary
 * 以最小结构表达状态订阅返回的释放租约。
 *
 * ## Invariants and lifecycle
 * attachClient 保存租约，并在再次绑定或 lifecycle.dispose 时调用 dispose；
 * 此租约只撤销本层监听，不取得客户端所有权，也不负责停止客户端。
 *
 * ## References
 * LanguageClientLike.onDidChangeState 返回此形状；适配器保存于 stateDisposable。
 */
type DisposableLike = {
    /**
     * ## Summary
     * 释放与本租约对应的状态监听。
     *
     * ## Returns
     * 不返回值；释放效果由订阅方观察。底层释放异常会由调用方接收。
     *
     * ## Constraints and lifecycle
     * 适配层在替换订阅或自身释放时调用；该操作不停止或释放 LanguageClient。
     *
     * ## References
     * createTransportAwareLanguageClientLifecycle.attachClient 与 .dispose。
     */
    dispose(): void;
};

/**
 * ## Summary
 * 保留错误策略的动作与 SDK 可选提示字段，使默认处理器结果可原样传回 SDK。
 *
 * ## Fields
 * action 属于当前回调对应的 SDK 动作域；message 与 handled 是可选的展示/处理提示。
 * 两种回调共享结构，但 ErrorAction 与 CloseAction 的数值不能互换解释。
 *
 * ## Invariants and lifecycle
 * 该对象由委托处理器创建并由异步回调返回；适配层不保存结果，也不复制或改写字段。
 *
 * ## References
 * ErrorHandlerLike 的 error/closed 以及 SDK 默认错误处理器。
 */
type ErrorHandlerResultLike = {
    /** 必填动作值；其含义由 error 或 closed 所属的 SDK 动作域决定。 */
    action: number;
    /** 可选的用户可见提示，由 SDK 决定如何呈现。 */
    message?: string;
    /** true 表示该结果已处理相关提示，具体解释由 SDK 执行路径决定。 */
    handled?: boolean;
};

/**
 * ## Summary
 * 表达 vscode-languageclient 默认错误处理策略的两个回调及其返回值。
 *
 * ## Invariants and lifecycle
 * SDK 允许同步或异步结果；适配器的 errorHandler 以 Promise 形式交付结果，
 * 并在活跃绑定中保留同一委托以沿用 SDK 自己的错误计数与重启预算。
 *
 * ## References
 * LanguageClientLike.createDefaultErrorHandler 取得委托；
 * createTransportAwareLanguageClientLifecycle.errorHandler 调用它。
 */
type ErrorHandlerLike = {
    /**
     * ## Summary
     * 为非传输销毁类连接错误取得 SDK 默认动作。
     *
     * ## Parameters
     * @param error SDK 报告的原始异常；适配层仅分类，不接管其所有权。
     * @param message SDK 附带的提示值，原样传给委托。
     * @param count SDK 提供的错误次数，原样传给委托。
     *
     * ## Returns
     * 同步或异步给出 SDK 错误动作；调用方须保留动作所属域。
     *
     * ## Constraints and lifecycle
     * 回调在 SDK 处理连接错误时调用；参数不被持有。适配器对断流写错误短路，
     * 因而该类错误不会进入此委托。
     *
     * ## References
     * createTransportAwareLanguageClientLifecycle.errorHandler.error。
     */
    error(error: Error, message: unknown, count: number | undefined): ErrorHandlerResultLike | Promise<ErrorHandlerResultLike>;

    /**
     * ## Summary
     * 为连接关闭事件取得 SDK 的默认恢复动作。
     *
     * ## Returns
     * 同步或异步给出 action 属于 CloseAction 的结果对象，由 SDK 消费。
     *
     * ## Constraints and lifecycle
     * SDK 关闭处理期间调用；委托保留同一客户端的重启预算。适配层退休检查为真时
     * 不调用它。
     *
     * ## References
     * createTransportAwareLanguageClientLifecycle.errorHandler.closed。
     */
    closed(): ErrorHandlerResultLike | Promise<ErrorHandlerResultLike>;
};

/**
 * ## Summary
 * 描述桌面与 Worker LanguageClient 共有的最小结构，令恢复策略不依赖某个宿主 SDK
 * 入口或 VS Code 模块。
 *
 * ## Invariants and lifecycle
 * 本适配层只调用 createDefaultErrorHandler 与 onDidChangeState；state 和 stop 是
 * 兼容客户端形状的一部分，但此实现不读取或调用它们。创建、启动、停止和释放客户端
 * 由各宿主及 LanguageServerSession 管理。
 *
 * ## References
 * extension.ts 与 browser.ts 分别提供桌面和 Web 客户端；FakeClient 在 Node 测试中
 * 只模拟此结构的适配层所需部分。
 */
export interface LanguageClientLike {
    /** SDK 当前状态；本适配层不轮询或读取该值。 */
    state: number;

    /**
     * ## Summary
     * 表达宿主管理客户端停止的 SDK 能力。
     *
     * ## Returns
     * 返回客户端停止操作完成的 Promise；错误由发起清理的 session 或宿主处理。
     *
     * ## Constraints and lifecycle
     * 本适配器不调用 stop；实际客户端停止归 LanguageServerSession，调用方不能因
     * lifecycle.dispose 而认为客户端已停止。
     *
     * ## References
     * LanguageServerSession.disposeClient 执行真实客户端清理。
     */
    stop(): Promise<void>;

    /**
     * ## Summary
     * 创建与该客户端绑定的 SDK 默认错误处理策略。
     *
     * ## Parameters
     * @param maxRestartCount 可选重启预算；undefined 的解释及合法范围由 SDK 决定。
     *
     * ## Returns
     * 返回随客户端生命周期使用的 ErrorHandlerLike；SDK 决定其计数及恢复结果。
     *
     * ## Constraints and lifecycle
     * attachClient 调用并保存结果；重复绑定会向 SDK 再取一个处理器。SDK 对非法预算
     * 的同步异常会向 attachClient 调用方传播。
     *
     * ## References
     * createTransportAwareLanguageClientLifecycle.attachClient；本地依赖版本记录于
     * zr_vm_language_server_extension/package-lock.json。
     */
    createDefaultErrorHandler(maxRestartCount?: number): ErrorHandlerLike;

    /**
     * ## Summary
     * 订阅客户端之后发生的状态转换。
     *
     * ## Parameters
     * @param listener 接收一次转换事件的回调；适配层只读取 newState，不保存事件对象。
     *
     * ## Returns
     * 返回可撤销该次订阅的租约；订阅方负责在生命周期结束时释放。
     *
     * ## Constraints and lifecycle
     * 注册不补发当前状态；listener 保持注册直到租约释放。SDK 状态变化的触发顺序由
     * 客户端管理，本接口不承诺额外排队或重放。
     *
     * ## References
     * attachClient 注册 listener，并把租约保存在 stateDisposable。
     */
    onDidChangeState(listener: (event: StateChangeEventLike) => unknown): DisposableLike;
}

/**
 * ## Summary
 * 为一个 LanguageServerSession 暴露 SDK 错误处理器和传输状态观察能力。
 *
 * ## Invariants and lifecycle
 * 宿主先把 errorHandler 放进 clientOptions，再构造客户端并调用 attachClient，之后才启动。
 * session.own 接管此对象的 dispose；客户端本身由 session.startClient 单独持有和停止。
 * dispose 只释放状态监听与委托引用，不使已暴露的 errorHandler 失效；其 closed 闭包仍捕获
 * isRetired。只要返回对象或 SDK 仍保留 errorHandler，该谓词在 dispose 后仍可能被调用，故宿主
 * 必须先退休会话并安排客户端清理，且谓词所读状态须覆盖迟到回调的生命周期。
 *
 * ## References
 * extension.ts 与 browser.ts 建立该对象；LanguageServerSession 以逆序释放客户端和观察器。
 */
export interface TransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike> {
    /**
     * ## Summary
     * 交给 LanguageClientOptions.errorHandler 的连接策略入口。
     *
     * ## Invariants and lifecycle
     * 工厂创建后即可安装，随后须在客户端 start 前 attachClient。SDK 可在本对象 dispose
     * 后仍持有此引用；会话退休谓词负责阻止旧客户端自动重启。
     *
     * ## References
     * extension.ts 与 browser.ts 将此值赋给 clientOptions.errorHandler。
     */
    readonly errorHandler: ErrorHandlerLike;

    /**
     * ## Summary
     * 将已构造客户端的默认错误策略和状态事件接入本生命周期对象。
     *
     * ## Parameters
     * @param client 已构造且尚未启动的客户端；其方法由本次调用期间同步借用。
     *
     * ## Returns
     * 不返回值；成功后本对象保存默认处理器和状态订阅租约。
     *
     * ## Constraints and lifecycle
     * 必须在 SDK 启动前调用。再次绑定先释放旧监听，再获取新默认处理器并注册新监听；旧客户端须停止经同一 handler 派发。
     * 不会停止或释放旧客户端。SDK 默认处理器创建或事件订阅若抛错，异常向调用方传播，
     * 已执行的步骤不会回滚，调用方应视为本次绑定未完成且不要启动客户端。
     *
     * ## References
     * extension.ts 与 browser.ts 在构造客户端后调用；工厂中的 attachClient 实现具体租约更新。
     */
    attachClient(client: TClient): void;

    /**
     * ## Summary
     * 查询连接错误或关闭通知后尚未被相关状态转换清除的观察标记。
     *
     * ## Returns
     * true 表示观察到断流写错误或 close 通知且之后尚未收到 STOPPED/RUNNING；
     * false 只表示标记当前未置位。
     *
     * ## Constraints and lifecycle
     * STOPPED 和 RUNNING 都会清标记，所以 false 不证明传输健康、客户端已启动或请求可用；
     * 该值不参与宿主请求路由。
     *
     * ## References
     * errorHandler.error、errorHandler.closed 与 resetTransportStateIfHealthy 更新此状态。
     */
    isTransportBroken(): boolean;

    /**
     * ## Summary
     * 释放本层持有的状态监听租约并清除委托及观察值。
     *
     * ## Returns
     * 不返回值；客户端停止由 session 的另一项清理负责。
     *
     * ## Constraints and lifecycle
     * 可重复调用；底层租约释放若抛错会中断后续字段清理。此操作不销毁客户端、
     * 不等待 SDK 回调，也不禁用 errorHandler；session 应先同步退休并清理客户端。
     *
     * ## References
     * LanguageServerSession.dispose 逆序释放客户端后再释放 lifecycle。
     */
    dispose(): void;
}

/**
 * ## Summary
 * 识别 Node 流已销毁后仍尝试写入的错误，供连接错误策略和退出清理共享分类。
 *
 * ## Parameters
 * @param error 任意异常值；只有对象上的精确错误码或消息文本会命中。
 *
 * ## Returns
 * 有精确 ERR_STREAM_DESTROYED 错误码，或字符串消息不区分大小写地包含
 * “stream was destroyed” 时返回 true；其他输入返回 false。
 *
 * ## Constraints and lifecycle
 * 纯分类，不修改、保存或释放输入，也不证明底层资源已经清理。消息匹配是没有稳定
 * Node 错误码时的兼容路径，可能受上游措辞变化影响。
 *
 * ## References
 * errorHandler.error 对命中错误短路；LanguageServerSession.disposeClient 在退出阶段
 * 使用 isBenignLanguageClientStopError 间接复用此分类。
 */
export function isTransportDestroyedError(error: unknown): boolean {
    if (!error || typeof error !== 'object') {
        return false;
    }

    const candidate = error as { code?: unknown; message?: unknown };
    if (candidate.code === 'ERR_STREAM_DESTROYED') {
        return true;
    }

    return typeof candidate.message === 'string' &&
        candidate.message.toLowerCase().includes('stream was destroyed');
}

/**
 * ## Summary
 * 判断 session 退出清理期间是否可以忽略客户端停止异常。
 *
 * ## Parameters
 * @param error dispose/stop 操作拒绝的异常值，不转移给本函数管理。
 *
 * ## Returns
 * 仅当断流分类命中，或错误文本表明客户端当前未运行时返回 true。
 *
 * ## Constraints and lifecycle
 * 此布尔值只供清理调用方吞掉已知停止错误；不能据此认定诊断集合等其他资源已释放，
 * 也不能吞掉未识别异常或推断客户端启动成功。
 *
 * ## References
 * LanguageServerSession.disposeClient 用 true 决定是否继续吞掉 stop 错误，仍在 finally
 * 独立释放 diagnostics。
 */
export function isBenignLanguageClientStopError(error: unknown): boolean {
    if (isTransportDestroyedError(error)) {
        return true;
    }

    return isLanguageClientNotRunningError(error);
}

/**
 * ## Summary
 * 识别当前 vscode-languageclient 在非运行状态拒绝停止客户端的已知错误文本。
 *
 * ## Parameters
 * @param error 任意异常值；仅检查对象上的字符串 message。
 *
 * ## Returns
 * message 区分大小写地包含 “Client is not running and can't be stopped” 时返回 true。
 *
 * ## Constraints and lifecycle
 * 这是基于 SDK 错误文本的兼容分类，不验证客户端状态，也不尝试停止或释放资源；上游
 * 改写措辞时需要复核。其他异常必须交回宿主或 session。
 *
 * ## References
 * extension.ts 与 browser.ts 的 stop 覆盖，以及 isBenignLanguageClientStopError。
 */
export function isLanguageClientNotRunningError(error: unknown): boolean {

    if (!error || typeof error !== 'object') {
        return false;
    }

    const candidate = error as { message?: unknown };
    return typeof candidate.message === 'string' &&
        candidate.message.includes("Client is not running and can't be stopped");
}

/**
 * ## Summary
 * 为一次 LanguageServerSession 建立可在 clientOptions 构造前安装的连接错误处理器，
 * 并在客户端创建后绑定 SDK 默认策略和状态订阅。它把断流写错误观察与关闭恢复决策
 * 放在共享适配层，桌面及 Worker 宿主都由各自 session 管理客户端。
 *
 * ## Parameters
 * @param maxRestartCount 可选重启预算，原样交给客户端的 SDK 默认处理器；undefined 时
 * 采用 SDK 自己的默认值。该值的范围和错误策略由绑定客户端决定。
 * @param isRetired 每次收到连接关闭回调时同步调用的退休状态读取函数。返回 handler 的
 * closed 闭包会捕获此函数；即使 lifecycle.dispose 清空内部字段，只要 handler 仍可达，谓词仍可能
 * 被调用。应读取可变 session 状态而非捕获一次性快照，并让该状态覆盖迟到回调。默认函数恒返回
 * false，因此独立使用时不会阻止 SDK 自动恢复。
 *
 * ## Returns
 * 返回仅管理错误回调和状态监听的 lifecycle。宿主先将 errorHandler 赋给
 * LanguageClientOptions，再创建客户端、调用 attachClient，最后才启动客户端；
 * LanguageServerSession.own 持有 lifecycle 的释放责任。工厂不创建、启动、停止或释放客户端。
 *
 * ## Constraints and lifecycle
 * attachClient 可重复调用，但会重建默认处理器及预算并先撤销旧订阅；旧客户端不得继续经同一 handler 派发，否则回调将读取新委托。
 * 若处理器创建或事件注册抛错，已执行步骤不会回滚，宿主不得继续启动该客户端。
 * errorHandler.error 与 closed 返回 Promise；委托拒绝会向 SDK 调用方传播。
 * dispose 只清本层引用，不禁止仍被 SDK 持有的 errorHandler 再次调用；其 closed 闭包仍捕获
 * isRetired，因此只要 handler 仍可达，dispose 后谓词仍可能被调用。要阻止退休客户端自动重启，
 * 谓词须在每次回调时读取当前 session 状态，并让该状态覆盖迟到回调。
 *
 * ## References
 * extension.ts 与 browser.ts 分别先 session.own 本对象、设置 clientOptions.errorHandler，
 * 构造客户端后 attachClient，再通过 session.startClient 启动。LanguageServerSession.dispose
 * 先同步退休，再按逆序释放客户端和 lifecycle。
 */
export function createTransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike>(
    maxRestartCount?: number,
    /**
     * 这个默认箭头是固定 false 的简单谓词；需要退休保护的宿主应传入动态状态读取函数。
     * closed 闭包捕获所选函数；dispose 后只要 errorHandler 仍可达，它仍可能在迟到回调中被调用。
     */
    isRetired: () => boolean = () => false,
): TransportAwareLanguageClientLifecycle<TClient> {
    // 委托和监听器属于同一次绑定；transportBroken 是观察值，不驱动宿主请求路由。
    let delegate: ErrorHandlerLike | undefined;
    let stateDisposable: DisposableLike | undefined;
    let transportBroken = false;

    // 停止和重新运行均结束上一段故障观察窗口；false 不代表已建立新连接。
    /**
     * ## Summary
     * 在 SDK 明确报告停止或重新运行时结束上一段故障观察窗口。
     *
     * ## Parameters
     * @param state 来自状态转换事件的 newState 数值，不读取客户端当前状态。
     *
     * ## Returns
     * 不返回值；匹配 STOPPED 或 RUNNING 时把闭包标记置为 false。
     *
     * ## Constraints and lifecycle
     * STOPPED 与 RUNNING 都清标记；此标记不是连接健康或请求可用性证明。其余状态不改标记。
     *
     * ## References
     * attachClient 注册的状态 listener 调用此函数；isTransportBroken 暴露标记快照。
     */
    const resetTransportStateIfHealthy = (state: number): void => {
        if (state === LANGUAGE_CLIENT_STATE_RUNNING || state === LANGUAGE_CLIENT_STATE_STOPPED) {
            transportBroken = false;
        }
    };

    // 两个回调共享一次绑定状态，但 error 与 closed 分别沿不同 SDK 动作域返回决定。
    const errorHandler: ErrorHandlerLike = {
        /**
         * ## Summary
         * 将 SDK 的连接错误分成已毁流写错误与其余默认策略路径。
         *
         * ## Parameters
         * @param error SDK 给出的连接异常；仅在本回调期间读取和分类。
         * @param message SDK 附带的提示值；正常路径原样传给默认处理器。
         * @param count SDK 附带的错误计数；正常路径原样传给默认处理器。
         *
         * ## Returns
         * 返回 Promise，成功解析为 ErrorHandlerResultLike；拒绝沿 Promise 传播。断流写错误返回 Continue 并置位标记；
         * 其他错误返回默认处理器结果，尚未绑定时回退 Continue。
         *
         * ## Constraints and lifecycle
         * 错误对象和提示值不被保存。断流分支不调用 SDK delegate.error；委托拒绝不会被吞掉，
         * 将沿 Promise 传播给 SDK。回调只给出策略动作，不等待或发起连接重启。
         *
         * ## References
         * 工厂返回的 ErrorHandlerLike.error；SDK 从宿主 clientOptions.errorHandler 派发此回调。
         */
        async error(error, message, count) {
            if (isTransportDestroyedError(error)) {
                // 已毁流的写错误不触发 SDK 的错误次数停机；连接关闭回调负责恢复决策。
                transportBroken = true;
                return { action: ERROR_ACTION_CONTINUE };
            }

            if (delegate) {
                return delegate.error(error, message, count);
            }

            // 正常宿主在 start 前绑定；未绑定时尚无 SDK 策略可委托。
            return { action: ERROR_ACTION_CONTINUE };
        },

        /**
         * ## Summary
         * 处理 SDK 的连接关闭通知，并在调用默认重启策略前检查 session 是否退休。
         *
         * ## Returns
         * 返回 Promise，成功解析为含 CloseAction 的结果对象；拒绝沿 Promise 传播。检查时已退休则 action 为 DoNotRestart，未退休且已绑定时返回
         * SDK delegate.closed 的结果，尚未绑定时回退 Restart。
         *
         * ## Constraints and lifecycle
         * 每次调用先置位故障观察标记，再同步读取一次 isRetired；检查时已退休则跳过默认 delegate。
         * 活跃路径把预算决定交给 SDK；委托结果返回前不二次检查退休状态，也不控制 SDK 实际重启调度。
         * 退休读取或委托拒绝会使 Promise 拒绝并交回 SDK；不在此处恢复异常。
         *
         * ## References
         * 工厂返回的 ErrorHandlerLike.closed；extension.ts 与 browser.ts 安装此处理器，
         * LanguageServerSession.dispose 在客户端清理前同步标记退休。
         */
        async closed() {
            transportBroken = true;

            // 此处只读取一次退休值；后续退休不会撤回本次已经开始的异步委托结果。
            if (isRetired()) {
                return { action: CLOSE_ACTION_DO_NOT_RESTART };
            }

            // 在同一绑定内保留 SDK 的重启计数，让连续崩溃仍受默认预算约束。
            if (delegate) {
                return delegate.closed();
            }

            return { action: CLOSE_ACTION_RESTART };
        },
    };

    return {
        errorHandler,

        /**
         * ## Summary
         * 在客户端启动前绑定 SDK 默认处理器和状态事件监听。
         *
         * ## Parameters
         * @param client 已构造的客户端；仅在本次同步调用期间借用，不转移客户端所有权。
         *
         * ## Returns
         * 不返回值；成功后保存 SDK 默认错误策略与状态事件租约。
         *
         * ## Constraints and lifecycle
         * 替换绑定先释放旧事件租约，再取得本次默认处理器、清故障标记并注册 listener；
         * 不停止旧客户端；调用方须阻止其继续经同一 handler 派发。若策略创建或订阅抛错，异常传播且此前副作用不回滚，
         * 调用方须将绑定视为失败，不应启动客户端。此方法不校验 start 时序。
         *
         * ## References
         * extension.ts 与 browser.ts 在客户端构造后、session.startClient 前调用本方法。
         */
        attachClient(client) {
            // 宿主每个 session 只绑定一次；此处的重绑定清理不转移客户端所有权。
            stateDisposable?.dispose();
            delegate = client.createDefaultErrorHandler(maxRestartCount);
            transportBroken = false;
            /** 状态事件只借用 newState；释放撤销订阅，已进入派发快照的回调仍可能完成。 */
            stateDisposable = client.onDidChangeState((event) => {
                resetTransportStateIfHealthy(event.newState);
            });
        },

        /**
         * ## Summary
         * 读取闭包中最近一次断流写错误或连接关闭事件留下的观察值。
         *
         * ## Returns
         * 返回当前布尔快照；true 表示标记尚未被清除，false 不代表传输健康。
         *
         * ## Constraints and lifecycle
         * attachClient 会清零；状态回调在 RUNNING 或 STOPPED 时清零；dispose 也清零。
         * 此方法不查询客户端，不参与宿主请求分派。
         *
         * ## References
         * TransportAwareLanguageClientLifecycle.isTransportBroken 的实现。
         */
        isTransportBroken() {
            return transportBroken;
        },

        /**
         * ## Summary
         * 释放本适配器的状态订阅租约并清除闭包保留的委托与故障标记。
         *
         * ## Returns
         * 不返回值；客户端本身由 session 的独立清理停止和释放。
         *
         * ## Constraints and lifecycle
         * 可重复调用。租约 dispose 若抛错，后续清空语句不会执行；正常完成后不再保留客户端
         * 委托或监听器。errorHandler 对象仍可被 SDK 调用，且无已释放保护；其 closed 闭包仍捕获
         * isRetired，故 handler 可达期间谓词也可能在 dispose 后继续调用。宿主须先退休 session，
         * 并使动态谓词在迟到回调期间仍能读取有效状态，以阻止旧客户端自动重启。
         *
         * ## References
         * LanguageServerSession.dispose 先同步退休、逆序释放客户端，随后释放该对象。
         */
        dispose() {
            // session 按逆序先释放客户端，再释放此观察器；这里只回收本层持有的资源。
            stateDisposable?.dispose();
            stateDisposable = undefined;
            delegate = undefined;
            transportBroken = false;
        },
    };
}
