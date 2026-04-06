export const LANGUAGE_CLIENT_STATE_STOPPED = 1;
export const LANGUAGE_CLIENT_STATE_RUNNING = 2;

export const ERROR_ACTION_CONTINUE = 1;
export const CLOSE_ACTION_RESTART = 2;

type StateChangeEventLike = {
    oldState: number;
    newState: number;
};

type DisposableLike = {
    dispose(): void;
};

type ErrorHandlerResultLike = {
    action: number;
    message?: string;
    handled?: boolean;
};

type ErrorHandlerLike = {
    error(error: Error, message: unknown, count: number | undefined): ErrorHandlerResultLike | Promise<ErrorHandlerResultLike>;
    closed(): ErrorHandlerResultLike | Promise<ErrorHandlerResultLike>;
};

export interface LanguageClientLike {
    state: number;
    stop(): Promise<void>;
    createDefaultErrorHandler(maxRestartCount?: number): ErrorHandlerLike;
    onDidChangeState(listener: (event: StateChangeEventLike) => unknown): DisposableLike;
}

export interface TransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike> {
    readonly errorHandler: ErrorHandlerLike;
    attachClient(client: TClient): void;
    isTransportBroken(): boolean;
    dispose(): void;
}

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

export function isBenignLanguageClientStopError(error: unknown): boolean {
    if (isTransportDestroyedError(error)) {
        return true;
    }

    if (!error || typeof error !== 'object') {
        return false;
    }

    const candidate = error as { message?: unknown };
    return typeof candidate.message === 'string' &&
        candidate.message.includes("Client is not running and can't be stopped");
}

export function createTransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike>(
    maxRestartCount?: number,
): TransportAwareLanguageClientLifecycle<TClient> {
    let currentClient: TClient | undefined;
    let delegate: ErrorHandlerLike | undefined;
    let stateDisposable: DisposableLike | undefined;
    let transportBroken = false;

    const resetTransportStateIfHealthy = (state: number): void => {
        if (state === LANGUAGE_CLIENT_STATE_RUNNING || state === LANGUAGE_CLIENT_STATE_STOPPED) {
            transportBroken = false;
        }
    };

    const errorHandler: ErrorHandlerLike = {
        async error(error, message, count) {
            if (isTransportDestroyedError(error)) {
                transportBroken = true;
                return { action: ERROR_ACTION_CONTINUE };
            }

            if (delegate) {
                return delegate.error(error, message, count);
            }

            return { action: ERROR_ACTION_CONTINUE };
        },
        async closed() {
            transportBroken = true;

            if (delegate) {
                return delegate.closed();
            }

            return { action: CLOSE_ACTION_RESTART };
        },
    };

    return {
        errorHandler,
        attachClient(client) {
            stateDisposable?.dispose();
            currentClient = client;
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
            stateDisposable?.dispose();
            stateDisposable = undefined;
            currentClient = undefined;
            delegate = undefined;
            transportBroken = false;
        },
    };
}

export async function stopLanguageClientSafely<TClient extends LanguageClientLike>(
    client: TClient,
    lifecycle: TransportAwareLanguageClientLifecycle<TClient>,
    waitForStateChangeMs = 500,
): Promise<void> {
    if (client.state !== LANGUAGE_CLIENT_STATE_RUNNING) {
        return;
    }

    if (!lifecycle.isTransportBroken()) {
        await client.stop();
        return;
    }

    await waitForClientToLeaveRunningState(client, waitForStateChangeMs);
}

async function waitForClientToLeaveRunningState<TClient extends LanguageClientLike>(
    client: TClient,
    timeoutMs: number,
): Promise<void> {
    if (client.state !== LANGUAGE_CLIENT_STATE_RUNNING) {
        return;
    }

    await new Promise<void>((resolve) => {
        let settled = false;
        let timeoutHandle: ReturnType<typeof setTimeout> | undefined;
        let stateDisposable: DisposableLike | undefined;

        const complete = (): void => {
            if (settled) {
                return;
            }

            settled = true;
            if (timeoutHandle !== undefined) {
                clearTimeout(timeoutHandle);
            }
            stateDisposable?.dispose();
            resolve();
        };

        stateDisposable = client.onDidChangeState((event) => {
            if (event.newState !== LANGUAGE_CLIENT_STATE_RUNNING) {
                complete();
            }
        });

        timeoutHandle = setTimeout(complete, timeoutMs);
    });
}
