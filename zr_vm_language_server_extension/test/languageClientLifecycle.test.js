const test = require('node:test');
const assert = require('node:assert/strict');

const {
    CLOSE_ACTION_RESTART,
    ERROR_ACTION_CONTINUE,
    LANGUAGE_CLIENT_STATE_RUNNING,
    LANGUAGE_CLIENT_STATE_STOPPED,
    createTransportAwareLanguageClientLifecycle,
    isBenignLanguageClientStopError,
    isTransportDestroyedError,
} = require('../out/languageClientLifecycle.js');

// 通过结构接口模拟 SDK 的委托和状态事件，无需 VS Code 宿主或真实传输。
// 此替身仅验证适配层策略，宿主回归也使用固定重启动作的 FakeClient。
// TODO: 用真实 SDK 和故障注入核查连接关闭顺序及崩溃重启预算，当前测试未覆盖。
class FakeClient {
    constructor() {
        this.state = LANGUAGE_CLIENT_STATE_RUNNING;
        this.stopCalls = 0;
        this.listeners = [];
        this.defaultErrorHandler = {
            error: async () => ({ action: 99 }),
            closed: async () => ({ action: CLOSE_ACTION_RESTART }),
        };
    }

    createDefaultErrorHandler() {
        // 固定策略让用例能够区分适配层自己的决定与交给 SDK 的决定。
        return this.defaultErrorHandler;
    }

    onDidChangeState(listener) {
        // 返回的租约对应单次订阅，允许覆盖重绑定和 dispose 的责任边界。
        this.listeners.push(listener);
        return {
            dispose: () => {
                this.listeners = this.listeners.filter((entry) => entry !== listener);
            },
        };
    }

    async stop() {
        // 保留客户端接口的停止能力；当前用例不会要求适配层主动停止客户端。
        this.stopCalls += 1;
        this.transition(LANGUAGE_CLIENT_STATE_STOPPED);
    }

    transition(nextState) {
        // 用显式事件分离故障标记与 SDK 状态；测试决定何时进入新的观察窗口。
        const previous = this.state;
        this.state = nextState;
        for (const listener of [...this.listeners]) {
            listener({ oldState: previous, newState: nextState });
        }
    }
}

// 同时锁定 Node 错误码和跨边界文本兼容入口，防止退出清理把已知状态当作新故障。
test('detects destroyed transport errors from code and message text', () => {
    assert.equal(isTransportDestroyedError({ code: 'ERR_STREAM_DESTROYED' }), true);
    assert.equal(isTransportDestroyedError(new Error('Cannot call write after a stream was destroyed')), true);
    assert.equal(isTransportDestroyedError(new Error('different failure')), false);
    assert.equal(isBenignLanguageClientStopError(new Error("Client is not running and can't be stopped. It's current state is: stopped")), true);
});

// 已毁流的写错误只记入观察状态，恢复决策留给后续连接关闭通知。
test('marks transport as broken and continues on destroyed transport errors', async () => {
    const lifecycle = createTransportAwareLanguageClientLifecycle();
    const client = new FakeClient();

    lifecycle.attachClient(client);

    const result = await lifecycle.errorHandler.error(
        Object.assign(new Error('Cannot call write after a stream was destroyed'), { code: 'ERR_STREAM_DESTROYED' }),
        undefined,
        undefined,
    );

    assert.equal(result.action, ERROR_ACTION_CONTINUE);
    assert.equal(lifecycle.isTransportBroken(), true);
});

// TODO: 本例只检查标记最终清除，未断言 closed 的委托结果或 STOPPED 当下的值；
// 拆分这两处观察后才能分别证明重启策略与停止/运行状态的清除边界。
test('delegates close handling and clears transport-broken state after running again', async () => {
    const lifecycle = createTransportAwareLanguageClientLifecycle();
    const client = new FakeClient();

    lifecycle.attachClient(client);
    await lifecycle.errorHandler.closed();
    assert.equal(lifecycle.isTransportBroken(), true);

    client.transition(LANGUAGE_CLIENT_STATE_STOPPED);
    client.transition(LANGUAGE_CLIENT_STATE_RUNNING);

    assert.equal(lifecycle.isTransportBroken(), false);
});

// retirement 闭包必须在回调时读取，防止旧 session 被 SDK 自动恢复重新激活。
test('a retired session never delegates automatic restart after connection closure', async () => {
    let retired = false;
    const lifecycle = createTransportAwareLanguageClientLifecycle(undefined, () => retired);
    const client = new FakeClient();
    lifecycle.attachClient(client);
    assert.equal((await lifecycle.errorHandler.closed()).action, CLOSE_ACTION_RESTART);
    retired = true;
    assert.equal((await lifecycle.errorHandler.closed()).action, 1);
    lifecycle.dispose();
});
