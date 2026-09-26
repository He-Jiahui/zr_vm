export const LANGUAGE_CLIENT_STATE_STOPPED = 1;
export const LANGUAGE_CLIENT_STATE_RUNNING = 2;

export const ERROR_ACTION_CONTINUE = 1;
export const CLOSE_ACTION_RESTART = 2;
export const CLOSE_ACTION_DO_NOT_RESTART = 1;

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

    return isLanguageClientNotRunningError(error);
}

export function isLanguageClientNotRunningError(error: unknown): boolean {

    if (!error || typeof error !== 'object') {
        return false;
    }

    const candidate = error as { message?: unknown };
    return typeof candidate.message === 'string' &&
        candidate.message.includes("Client is not running and can't be stopped");
}

export function createTransportAwareLanguageClientLifecycle<TClient extends LanguageClientLike>(
    maxRestartCount?: number,
    isRetired: () => boolean = () => false,
): TransportAwareLanguageClientLifecycle<TClient> {
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

            if (isRetired()) {
                return { action: CLOSE_ACTION_DO_NOT_RESTART };
            }

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
            delegate = undefined;
            transportBroken = false;
        },
    };
}
