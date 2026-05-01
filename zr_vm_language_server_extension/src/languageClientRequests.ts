type RequestCapableClient = {
    sendRequest<TResult>(method: string, params?: unknown): Thenable<TResult> | Promise<TResult>;
};

type ChangeListener = () => unknown;
type Disposable = {
    dispose(): void;
};

let currentClient: RequestCapableClient | undefined;
const changeListeners = new Set<ChangeListener>();

export function onDidChangeLanguageClient(listener: ChangeListener): Disposable {
    changeListeners.add(listener);
    return {
        dispose: () => {
            changeListeners.delete(listener);
        },
    };
}

export function setLanguageClientRequestClient(client: RequestCapableClient | undefined): void {
    currentClient = client;
    for (const listener of [...changeListeners]) {
        listener();
    }
}

export async function sendLanguageServerRequest<TResult>(
    method: string,
    params?: unknown,
): Promise<TResult | undefined> {
    if (!currentClient) {
        return undefined;
    }

    try {
        return await currentClient.sendRequest<TResult>(method, params);
    } catch (error) {
        if (isIgnorableLanguageServerRequestError(error)) {
            return undefined;
        }

        throw error;
    }
}

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
