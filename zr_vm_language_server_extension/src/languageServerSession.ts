import { isBenignLanguageClientStopError } from './languageClientLifecycle';

/** 每次启动的清理动作；session 允许同步资源和异步客户端共用逆序释放队列。 */
type Cleanup = () => void | Promise<void>;
/** 宿主交给 session 托管的最小 VS Code 资源契约。 */
type Disposable = { dispose(): void };
/** 只依赖 SDK 的公开生命周期接口，避免控制器读取客户端内部状态。 */
type ManagedClient = {
    start(): Promise<void>;
    dispose(timeout?: number): Promise<void>;
    readonly diagnostics?: Disposable;
};

/** 告诉两个宿主本次启动已被替换或关闭；入口据此抑制误导性的启动错误提示。 */
export class StartupCancelled extends Error {
    constructor() {
        super('Language server startup was cancelled.');
    }
}

/**
 * 给启动或停止的等待设置期限，并在会话退休时尽快返回。
 * 期限只结束本层等待，不取消传入 promise；启动路径须另行处理迟到结果及资源归属。
 */
async function waitFor<T>(promise: Promise<T>, timeoutMs: number, message: string, signal?: AbortSignal): Promise<T> {
    let timer: ReturnType<typeof setTimeout> | undefined;
    let onAbort: (() => void) | undefined;
    try {
        return await Promise.race([
            promise,
            new Promise<T>((_, reject) => {
                timer = setTimeout(() => reject(new Error(message)), timeoutMs);
                onAbort = () => reject(new StartupCancelled());
                signal?.addEventListener('abort', onAbort, { once: true });
                if (signal?.aborted) { onAbort(); }
            }),
        ]);
    } finally {
        if (timer !== undefined) { clearTimeout(timer); }
        if (onAbort) { signal?.removeEventListener('abort', onAbort); }
    }
}

/**
 * 管理桌面或 Web 的单次启动尝试。两个宿主在获取资源后立即登记清理，
 * 退休后才完成的获取由 addCleanup 或宿主获取点单独释放，避免旧尝试污染新客户端。
 */
export class LanguageServerSession {
    // abort 先于任何异步清理发出；cleanups 仅保存退休前已登记的资源。
    private readonly abort = new AbortController();
    private readonly cleanups: Cleanup[] = [];
    private disposal: Promise<void> | undefined;
    private runningStartup = false;

    /** 停止预算独立于启动预算；宿主通常由 controller 为每次尝试构造此对象。 */
    constructor(private readonly stopTimeoutMs: number, private readonly startupTimeoutMs = 30000) {}

    /** Web fetch 等可中止工作使用同一退休信号。 */
    get signal(): AbortSignal { return this.abort.signal; }
    /** SDK 的关闭回调在决定是否自动恢复时也读取此值。 */
    get retired(): boolean { return this.signal.aborted; }

    /** 异步边界之后调用，阻止已退休尝试继续发布客户端或创建 Worker。 */
    assertActive(): void {
        if (this.retired) { throw new StartupCancelled(); }
    }

    /** 把同步 VS Code 资源纳入本次尝试；成功返回后由 session 负责释放。 */
    own<T extends Disposable>(resource: T): T {
        this.addCleanup(() => resource.dispose());
        return resource;
    }

    /**
     * 以获取顺序登记释放动作；退休后登记会安排迟到资源释放并拒绝继续启动。
     * 迟到动作的错误只记日志，因为当前 dispose 无法等待未来才到达的获取。
     */
    addCleanup(cleanup: Cleanup): void {
        if (this.retired) {
            void Promise.resolve().then(cleanup).catch(reportCleanupError);
            throw new StartupCancelled();
        }
        this.cleanups.push(cleanup);
    }

    /** 初次启动先登记客户端，再调用 SDK；失败和取消都由 controller 退休本会话。 */
    async startClient(client: ManagedClient): Promise<void> {
        this.assertActive();
        this.addCleanup(() => this.disposeClient(client));
        await client.start();
        this.assertActive();
    }

    /**
     * 两个宿主的公开 start 覆盖在初次启动和 SDK 自动恢复时均进入此处。
     * 每次恢复都有独立期限；超时后仍观察原始 promise，防止迟到的 SDK 成功复活旧客户端。
     */
    async observeClientStart(client: ManagedClient, start: () => Promise<void>): Promise<void> {
        this.assertActive();
        const started = start();
        // 退休后的首次清理可能早于 SDK 初始化完成；真正结算时再回收一次客户端。
        const releaseLateClient = async (): Promise<void> => {
            if (this.retired) { await this.disposeClient(client); }
        };
        // A deadline does not cancel the SDK's initialization promise. Clean after its real settlement too.
        void started.then(releaseLateClient, releaseLateClient).catch(reportCleanupError);
        try {
            await waitFor(started, this.startupTimeoutMs, 'Timed out while starting the Zr language server.', this.signal);
        } catch (error) {
            // The controller cleans the initial attempt after reporting its original error.
            // Automatic SDK recovery has no enclosing controller operation.
            if (!this.runningStartup) { await this.dispose().catch(reportCleanupError); }
            throw error;
        }
        this.assertActive();
    }

    /** SDK 在 Starting/StartFailed 时也可能拒绝 dispose；仍需触发 Node 进程 finalizer 并清理 diagnostics。 */
    private async disposeClient(client: ManagedClient): Promise<void> {
        try {
            // Invoke even for Starting/StartFailed: Node terminates its process in stop's finally.
            await waitFor(Promise.resolve().then(() => client.dispose(this.stopTimeoutMs)),
                this.stopTimeoutMs + 100, 'Timed out while disposing the Zr language client.');
        } catch (error) {
            if (!isBenignLanguageClientStopError(error)) { throw error; }
        } finally {
            // SDK 8.1 can reject dispose before releasing its diagnostic collection.
            client.diagnostics?.dispose();
        }
    }

    /** 仅在 controller 的初次尝试期间为整条启动链设置上限，失败清理由 controller 负责。 */
    async run(startup: (session: LanguageServerSession) => Promise<void>, timeoutMs: number): Promise<void> {
        this.runningStartup = true;
        const work = Promise.resolve().then(() => {
            this.assertActive();
            return startup(this);
        });
        try {
            await waitFor(work, timeoutMs, 'Timed out while starting the Zr language server.', this.signal);
            this.assertActive();
        } finally {
            this.runningStartup = false;
        }
    }

    /**
     * 同步标记退休并启动逆序清理；多次调用共享同一个结果。
     * 即使一个清理失败也继续执行其余动作，最后向宿主报告首个错误。
     */
    dispose(): Promise<void> {
        if (this.disposal) { return this.disposal; }
        this.abort.abort();
        const cleanups = this.cleanups.splice(0).reverse();
        this.disposal = (async () => {
            let failure: unknown;
            for (const cleanup of cleanups) {
                try { await cleanup(); } catch (error) { failure ??= error; }
            }
            if (failure !== undefined) { throw failure; }
        })();
        return this.disposal;
    }
}

/**
 * 桌面和 Web 宿主共用的重启队列。对调用者保留本次错误，队列自身则吸收失败，
 * 使激活、手动重启和配置更改不会被前一次失败永久阻断。
 */
export class LanguageServerController {
    // current 仅代表仍可发布的尝试；stopCurrent 在等待清理前先撤销它。
    private tail: Promise<void> = Promise.resolve();
    private current: LanguageServerSession | undefined;
    private closed = false;

    /** 正常宿主使用默认预算；测试替身缩短预算以覆盖挂起与迟到完成。 */
    constructor(private readonly startupTimeoutMs = 30000, private readonly stopTimeoutMs = 1000) {}

    /** 宿主把一次完整创建链传入；旧会话释放完成后才运行新尝试。 */
    restart(startup: (session: LanguageServerSession) => Promise<void>): Promise<void> {
        const operation = this.tail.then(async () => {
            if (this.closed) { return; }
            await this.stopCurrent();
            if (this.closed) { return; }
            const session = new LanguageServerSession(this.stopTimeoutMs, this.startupTimeoutMs);
            this.current = session;
            try {
                await session.run(startup, this.startupTimeoutMs);
            } catch (error) {
                if (this.current === session) { this.current = undefined; }
                await session.dispose().catch(reportCleanupError);
                throw error;
            }
        });
        this.tail = operation.then(() => {}, () => {});
        return operation;
    }

    /** 扩展停用时立即退休当前尝试，然后等待队列中已排队的操作自然退出。 */
    async dispose(): Promise<void> {
        this.closed = true;
        // Abort synchronously so a pending fetch/initialization cannot block deactivation.
        const cleanup = this.stopCurrent().then(() => ({ error: undefined }), (error: unknown) => ({ error }));
        await this.tail;
        const result = await cleanup;
        if (result.error !== undefined) { throw result.error; }
    }

    /** 先撤销当前句柄再释放，避免迟到回调仍被视为可发布的会话。 */
    private stopCurrent(): Promise<void> {
        const session = this.current;
        this.current = undefined;
        return session?.dispose() ?? Promise.resolve();
    }
}

/** 迟到清理或启动失败后的附带清理不能替换原始错误，但需要留下可诊断记录。 */
function reportCleanupError(error: unknown): void {
    console.warn('[zr-extension] language client cleanup failed:', error);
}
