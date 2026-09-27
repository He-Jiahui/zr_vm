// 与 vscode-languageclient 的公开 State/ErrorAction/CloseAction 值保持一致，
// 使桌面、Worker 和无需加载 VS Code 的 Node 测试共用此适配层。
// TODO: 升级 SDK 时核对枚举兼容性；当前 FakeClient 复用这些值，无法发现上游值变化。
export const LANGUAGE_CLIENT_STATE_STOPPED = 1;
export const LANGUAGE_CLIENT_STATE_RUNNING = 2;

export const ERROR_ACTION_CONTINUE = 1;
export const CLOSE_ACTION_RESTART = 2;
export const CLOSE_ACTION_DO_NOT_RESTART = 1;

/** 只消费 SDK 的状态通知；订阅本身不补发当前状态。 */
type StateChangeEventLike = {
    oldState: number;
    newState: number;
};

/** 绑定客户端时取得的监听器租约，由适配层释放。 */
type DisposableLike = {
    dispose(): void;
};

/** 保留 SDK 的动作和呈现提示，使委托结果能够原样回到 SDK。 */
type ErrorHandlerResultLike = {
    action: number;
    message?: string;
    handled?: boolean;
};

/** SDK 可同步或异步给出恢复决策；封装层必须接受两种委托形式。 */
type ErrorHandlerLike = {
    error(error: Error, message: unknown, count: number | undefined): ErrorHandlerResultLike | Promise<ErrorHandlerResultLike>;
    closed(): ErrorHandlerResultLike | Promise<ErrorHandlerResultLike>;
};

/**
 * 桌面与浏览器客户端共有的结构接口，避免此文件依赖某个宿主入口。
 * 本适配层使用默认错误策略和状态订阅；客户端的启动、停止归 session 管理。
 */
export interface LanguageClientLike {
    state: number;
    stop(): Promise<void>;
    createDefaultErrorHandler(maxRestartCount?: number): ErrorHandlerLike;
    onDidChangeState(listener: (event: StateChangeEventLike) => unknown): DisposableLike;
}

/** 一次 session 的错误策略与状态观察器；由 session.own 接管释放责任。 */
export interface TransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike> {
    /** 构造 LanguageClient 前放入 clientOptions，以接管 SDK 的连接回调。 */
    readonly errorHandler: ErrorHandlerLike;
    /** 构造后、启动前绑定；替换绑定只撤销旧订阅，不停止旧客户端。 */
    attachClient(client: TClient): void;
    /** 最近连接故障的暂态标记；STOPPED 也会清除，不能用作请求可用性判断。 */
    isTransportBroken(): boolean;
    /** 释放本层订阅和委托引用；不代替 session 对客户端的异步清理。 */
    dispose(): void;
}

/**
 * 识别断流后继续写入的错误，供连接恢复策略和退出清理共同分类。
 * 接受未知异常对象；文本匹配是缺少 Node 错误码时的兼容路径，不证明资源已释放。
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
 * 仅供 LanguageServerSession.disposeClient 在退出阶段忽略已断流或已停止错误。
 * 调用方仍须执行独立资源清理；其他错误应继续报告，不能据此判定启动成功。
 */
export function isBenignLanguageClientStopError(error: unknown): boolean {
    if (isTransportDestroyedError(error)) {
        return true;
    }

    return isLanguageClientNotRunningError(error);
}

/**
 * 识别 SDK 8.1 在 Starting/StartFailed 等非运行状态拒绝 stop 的已知错误。
 * 两个宿主的 stop 覆盖和 session 清理均使用此兼容边界，依赖 SDK 的错误文本。
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
 * 为一个 session 创建共享错误处理器，允许宿主先构造 clientOptions 再绑定客户端。
 * 绑定后复用 SDK 默认错误计数及崩溃重启预算；重复绑定会重建该预算。
 * @param maxRestartCount 交给 SDK 验证和解释；两个宿主传 undefined，采用 SDK 默认值。
 * @param isRetired 每次连接关闭时读取 session 的退休状态，阻止旧会话重新启动。
 * @returns 先安装 errorHandler、再 attachClient、最后 start；生命周期由 session 持有。
 */
export function createTransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike>(
    maxRestartCount?: number,
    isRetired: () => boolean = () => false,
): TransportAwareLanguageClientLifecycle<TClient> {
    // 委托和监听器属于同一次绑定；transportBroken 是观察值，不驱动宿主请求路由。
    let delegate: ErrorHandlerLike | undefined;
    let stateDisposable: DisposableLike | undefined;
    let transportBroken = false;

    // 停止和重新运行均结束上一段故障观察窗口；false 不代表已建立新连接。
    const resetTransportStateIfHealthy = (state: number): void => {
        if (state === LANGUAGE_CLIENT_STATE_RUNNING || state === LANGUAGE_CLIENT_STATE_STOPPED) {
            transportBroken = false;
        }
    };

    const errorHandler: ErrorHandlerLike = {
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
        async closed() {
            transportBroken = true;

            // session 在释放客户端前同步退休，因此关闭通知不能复活已替换的会话。
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
        attachClient(client) {
            // 宿主每个 session 只绑定一次；此处的重绑定清理不转移客户端所有权。
            stateDisposable?.dispose();
            delegate = client.createDefaultErrorHandler(maxRestartCount);
            transportBroken = false;
            stateDisposable = client.onDidChangeState((event) => {
                resetTransportStateIfHealthy(event.newState);
            });
        },
        isTransportBroken() {
            return transportBroken;
        },
        dispose() {
            // session 按逆序先释放客户端，再释放此观察器；这里只回收本层持有的资源。
            stateDisposable?.dispose();
            stateDisposable = undefined;
            delegate = undefined;
            transportBroken = false;
        },
    };
}
