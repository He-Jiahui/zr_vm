const test = require('node:test');
const assert = require('node:assert/strict');
const { setImmediate: nextTurn } = require('node:timers/promises');
const { deferred, loadExtensionHost } = require('./helpers/extensionHost');

// 宿主重启回归执行真实桌面/Web 入口，仅将 VS Code 与 language-client 外部边界替换为可控资源。
// 覆盖退休、超时和并发重启后旧尝试不能夺回请求客户端，资源必须按归属释放。

/** 给预期能立即退休的操作设测试上限，防止回归变成无限挂起的测试进程。 */
async function within(promise) {
    let timer;
    try {
        return await Promise.race([promise, new Promise((_, reject) => {
            timer = setTimeout(() => reject(new Error('Lifecycle did not settle after cancellation')), 500);
        })]);
    } finally {
        clearTimeout(timer);
    }
}

/** 对同一次宿主尝试的公开资源、监听器和客户端发布状态做共同验收。 */
function assertAttemptReleased(host) {
    for (const resource of host.resources.filter((item) => ['watcher', 'channel', 'diagnostics'].includes(item.kind))) {
        assert.ok(resource.disposals > 0, `${resource.kind} was not released`);
        if (resource.kind !== 'diagnostics') { assert.equal(resource.disposals, 1, resource.kind); }
    }
    for (const client of host.clients) { assert.equal(client.listeners.length, 0); }
    for (const worker of host.workers) { assert.equal(worker.terminations, 1); }
    for (const count of host.urls.values()) { assert.equal(count, 1); }
    assert.equal(host.requestClient(), undefined);
}

for (const kind of ['native', 'web']) {
    // SDK 自动恢复不经过 controller.restart，退役后的迟到完成仍须由 start 覆盖收尾。
    test(`${kind} retirement also releases a pending SDK automatic restart`, async () => {
        const host = loadExtensionHost(kind);
        await host.activate(host.extensionContext);
        const client = host.clients[0];
        const restart = deferred();
        client.state = 1;
        client.plan = restart.promise;
        // TODO: 这里手动模拟 SDK 恢复；真实连接关闭触发覆盖层后的时序与清理仍需故障注入验证。
        const restarted = client.start();
        const settled = restarted.catch(() => {});
        await nextTurn(); // Acquire native transports before retirement.
        await within(host.deactivate());
        restart.resolve();
        await settled;
        await nextTurn();
        assert.equal(client.state, 1, 'late SDK recovery must be stopped');
        assert.ok(client.disposeCalls >= 2);
        assertAttemptReleased(host);
    });

    // 自动恢复也必须受独立启动期限约束；挂起的 SDK promise 在期限后仍可能结算。
    test(`${kind} a hanging SDK automatic restart has its own startup deadline`, async () => {
        const host = loadExtensionHost(kind, { startupTimeoutMs: 20 });
        await host.activate(host.extensionContext);
        const client = host.clients[0];
        const restart = deferred();
        client.state = 1;
        client.plan = restart.promise;
        await assert.rejects(within(client.start()), /Timed out while starting/);
        assertAttemptReleased(host);
        restart.resolve();
        await nextTurn();
        await host.deactivate();
    });

    // 激活入口会呈报错误但保留重启命令，使用户能在初次失败后重试。
    test(`${kind} activation reports failure and a subsequent restart succeeds`, async () => {
        const host = loadExtensionHost(kind, { starts: [new Error('startup failed')] });
        await assert.doesNotReject(host.activate(host.extensionContext));
        assert.equal(host.errors.length, 1);
        assertAttemptReleased(host);
        await host.commands.get('zr.restartLanguageServer')();
        assert.equal(host.clients.length, 2);
        assert.equal(host.requestClient(), host.clients[1]);
        await host.deactivate();
        assertAttemptReleased(host);
    });

    // 一次拒绝不能污染 controller 的队列尾部；第二次命令应获得新的会话。
    test(`${kind} concurrent restarts remain serialized after a rejection`, async () => {
        const host = loadExtensionHost(kind, { starts: [undefined, new Error('restart failed'), undefined] });
        await host.activate(host.extensionContext);
        const restart = host.commands.get('zr.restartLanguageServer');
        await Promise.allSettled([restart(), restart()]);
        assert.equal(host.clients.length, 3);
        assert.equal(host.requestClient(), host.clients[2]);
        await host.deactivate();
        assertAttemptReleased(host);
    });

    // 停用不能等待永不完成的初始化，迟到的客户端也不能保留订阅或重新启动。
    test(`${kind} deactivation retires pending startup and its late completion`, async () => {
        const startup = deferred();
        const host = loadExtensionHost(kind, { starts: [startup.promise] });
        const activation = host.activate(host.extensionContext);
        while (host.clients.length === 0) { await nextTurn(); }
        await within(host.deactivate());
        await within(activation);
        assertAttemptReleased(host);
        startup.resolve();
        await nextTurn();
        await nextTurn();
        assertAttemptReleased(host);
        assert.ok(host.clients[0].disposeCalls >= 2, 'late running client must also be disposed');
        assert.equal((await host.clients[0].options.errorHandler.closed()).action, 1);
    });

    // SDK 停止错误会传给调用方，但 session 应继续释放其他独立拥有的资源。
    test(`${kind} stop errors still release every owned resource`, async () => {
        const host = loadExtensionHost(kind, { stopError: new Error('shutdown failed') });
        await host.activate(host.extensionContext);
        await assert.rejects(host.deactivate(), /shutdown failed/);
        assertAttemptReleased(host);
    });

    // 已超时的旧 promise 结算后不得覆盖下一次成功尝试的请求客户端。
    test(`${kind} a timed out startup cannot replace the client from a later retry`, async () => {
        const startup = deferred();
        const host = loadExtensionHost(kind, { starts: [startup.promise], startupTimeoutMs: 20 });
        await within(host.activate(host.extensionContext));
        assert.match(host.errors[0], /Timed out/);
        assertAttemptReleased(host);
        await host.commands.get('zr.restartLanguageServer')();
        const replacement = host.clients[1];
        assert.equal(host.requestClient(), replacement);
        startup.resolve();
        await nextTurn();
        await nextTurn();
        assert.equal(host.requestClient(), replacement);
        assert.ok(host.clients[0].disposeCalls >= 2);
        await host.deactivate();
        assertAttemptReleased(host);
    });

    // 停止等待有界；停止 promise 未结算时仍应清空宿主自己的资源租约。
    test(`${kind} a hanging stop is bounded and still releases owned resources`, async () => {
        const stop = deferred();
        const host = loadExtensionHost(kind, { stopPending: stop.promise });
        await host.activate(host.extensionContext);
        await assert.rejects(within(host.deactivate()), /Timed out while disposing/);
        assertAttemptReleased(host);
        stop.resolve();
    });
}

// Worker 构造在 URL 获取之后失败，先登记的 URL 清理必须支持后续重试。
test('web Worker construction failure revokes its Blob URL and permits another attempt', async () => {
    const options = { workerError: new Error('worker failed') };
    const host = loadExtensionHost('web', options);
    await assert.doesNotReject(host.activate(host.extensionContext));
    assert.equal(host.errors.length, 1);
    assertAttemptReleased(host);
    options.workerError = undefined;
    await host.commands.get('zr.restartLanguageServer')();
    assert.equal(host.requestClient(), host.clients[0]);
    await host.deactivate();
    assertAttemptReleased(host);
});

// 停用发出取消信号；模拟 fetch 即使忽略信号并迟到完成，也不得再构造 Worker。
test('web deactivation during worker fetch prevents late worker creation', async () => {
    const pendingFetch = deferred();
    const host = loadExtensionHost('web', { fetch: pendingFetch.promise });
    const activation = host.activate(host.extensionContext);
    await nextTurn();
    await within(host.deactivate());
    pendingFetch.resolve();
    await within(activation);
    await nextTurn();
    assert.equal(host.workers.length, 0);
    assertAttemptReleased(host);
});

for (const retirement of ['deactivation', 'timeout']) {
// Node SDK 可在异步工作目录检查后才创建传输，故释放不得等待初始化结算。
test(`native transport acquired after ${retirement} is released without waiting for initialization`, async () => {
    const transport = deferred();
    const initialization = deferred();
    const host = loadExtensionHost('native', {
        transportPending: transport.promise,
        starts: [initialization.promise],
        startupTimeoutMs: retirement === 'timeout' ? 20 : 30000,
    });
    const activation = host.activate(host.extensionContext);
    while (host.clients.length === 0) { await nextTurn(); }
    if (retirement === 'deactivation') { await within(host.deactivate()); }
    await within(activation);
    transport.resolve();
    await nextTurn();
    await nextTurn();
    for (const kind of ['native-process', 'reader', 'writer']) {
        const acquired = host.resources.filter((item) => item.kind === kind);
        assert.equal(acquired.length, 1, `${kind} acquisition should finish after retirement`);
        assert.equal(acquired[0].disposals, 1, `${kind} must be released before initialization settles`);
    }
    assertAttemptReleased(host);
    await within(host.deactivate());
});
}
