import { isBenignLanguageClientStopError } from './languageClientLifecycle';

type Cleanup = () => void | Promise<void>;
type Disposable = { dispose(): void };
type ManagedClient = {
    start(): Promise<void>;
    dispose(timeout?: number): Promise<void>;
    readonly diagnostics?: Disposable;
};

export class StartupCancelled extends Error {
    constructor() {
        super('Language server startup was cancelled.');
    }
}

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

/** Owns resources from one attempt, including acquisitions that finish after retirement. */
export class LanguageServerSession {
    private readonly abort = new AbortController();
    private readonly cleanups: Cleanup[] = [];
    private disposal: Promise<void> | undefined;
    private runningStartup = false;

    constructor(private readonly stopTimeoutMs: number, private readonly startupTimeoutMs = 30000) {}

    get signal(): AbortSignal { return this.abort.signal; }
    get retired(): boolean { return this.signal.aborted; }

    assertActive(): void {
        if (this.retired) { throw new StartupCancelled(); }
    }

    own<T extends Disposable>(resource: T): T {
        this.addCleanup(() => resource.dispose());
        return resource;
    }

    addCleanup(cleanup: Cleanup): void {
        if (this.retired) {
            void Promise.resolve().then(cleanup).catch(reportCleanupError);
            throw new StartupCancelled();
        }
        this.cleanups.push(cleanup);
    }

    async startClient(client: ManagedClient): Promise<void> {
        this.assertActive();
        this.addCleanup(() => this.disposeClient(client));
        await client.start();
        this.assertActive();
    }

    /** Called by the hosts' public start overrides, including SDK automatic recovery. */
    async observeClientStart(client: ManagedClient, start: () => Promise<void>): Promise<void> {
        this.assertActive();
        const started = start();
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

/** Serializes restarts while keeping a failed operation out of the next operation's dependency chain. */
export class LanguageServerController {
    private tail: Promise<void> = Promise.resolve();
    private current: LanguageServerSession | undefined;
    private closed = false;

    constructor(private readonly startupTimeoutMs = 30000, private readonly stopTimeoutMs = 1000) {}

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

    async dispose(): Promise<void> {
        this.closed = true;
        // Abort synchronously so a pending fetch/initialization cannot block deactivation.
        const cleanup = this.stopCurrent().then(() => ({ error: undefined }), (error: unknown) => ({ error }));
        await this.tail;
        const result = await cleanup;
        if (result.error !== undefined) { throw result.error; }
    }

    private stopCurrent(): Promise<void> {
        const session = this.current;
        this.current = undefined;
        return session?.dispose() ?? Promise.resolve();
    }
}

function reportCleanupError(error: unknown): void {
    console.warn('[zr-extension] language client cleanup failed:', error);
}
