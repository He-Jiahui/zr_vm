/** 用最小请求接口兼容桌面与浏览器客户端，避免视图功能依赖具体传输实现。 */
type RequestCapableClient = {
    sendRequest<TResult>(method: string, params?: unknown): Thenable<TResult> | Promise<TResult>;
};

/** 视图在客户端切换时自行刷新；返回值不参与请求结果。 */
type ChangeListener = () => unknown;
type Disposable = {
    dispose(): void;
};

// 激活入口发布当前客户端，视图订阅同一切换事件；关闭期间允许暂时没有客户端。
let currentClient: RequestCapableClient | undefined;
const changeListeners = new Set<ChangeListener>();

/** 订阅客户端替换通知；持有方须在自身注销时释放返回的句柄。 */
export function onDidChangeLanguageClient(listener: ChangeListener): Disposable {
    changeListeners.add(listener);
    return {
        dispose: () => {
            changeListeners.delete(listener);
        },
    };
}

/** 将桌面或浏览器的新客户端公布给视图，清空表示当前会话已经关闭。 */
export function setLanguageClientRequestClient(client: RequestCapableClient | undefined): void {
    currentClient = client;
    // TODO: 监听器抛错会中断后续通知并传回激活入口；核实是否需要逐个隔离失败。
    for (const listener of [...changeListeners]) {
        listener();
    }
}

/**
 * 通过当前会话发送扩展请求；可选功能将未启动、方法缺失及连接关闭视为无结果。
 * strict 模式供需要区分协议失败的调用方使用，原始错误会继续向上传递。
 */
export async function sendLanguageServerRequest<TResult>(
    method: string,
    params?: unknown,
    options: { strict?: boolean } = {},
): Promise<TResult | undefined> {
    if (!currentClient) {
        if (options.strict) {
            throw new Error('Language client is not running');
        }
        return undefined;
    }

    try {
        return await currentClient.sendRequest<TResult>(method, params);
    } catch (error) {
        if (!options.strict && isIgnorableLanguageServerRequestError(error)) {
            return undefined;
        }

        throw error;
    }
}

/** 识别可选请求在服务端版本差异或客户端重启时可忽略的错误。 */
export function isIgnorableLanguageServerRequestError(error: unknown): boolean {
    const code = typeof error === 'object' && error !== null
        ? (error as { code?: unknown }).code
        : undefined;
    const message = String(
        typeof error === 'object' && error !== null && 'message' in error
            ? (error as { message?: unknown }).message
            : error ?? '',
    ).toLowerCase();

    if (code === -32601 || message.includes('method not found')) {
        return true;
    }

    return message.includes('connection got disposed') ||
        message.includes('pending response rejected since connection got disposed') ||
        message.includes('stream was destroyed') ||
        message.includes('connection is closed') ||
        message.includes('client is not running');
}
