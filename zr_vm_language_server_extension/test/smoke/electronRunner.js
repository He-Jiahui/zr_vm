const { runSmokeSuite } = require('./smokeSuite.js');

async function run() {
    await runSmokeSuite({
        expectedMode: process.env.ZR_TEST_SERVER_MODE || 'native',
    });
}

module.exports = {
    run,
};
