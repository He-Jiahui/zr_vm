const path = require('path');

/**
 * 供显式调用 webpack 的开发流程把桌面扩展入口打成 CommonJS 资产。
 * VS Code 宿主提供 vscode 模块；浏览器入口及 WASM Worker 另走各自构建链。
 * TODO: 当前 package.json 的 compile/prepublish 仅调用 tsc 和 Worker 的 esbuild，
 * 仓内未发现 webpack 入口；核查是否仍有仓外打包消费者及本配置的维护责任。
 */
module.exports = {
  target: 'node',
  mode: 'development',
  entry: './src/extension.ts',
  output: {
    path: path.resolve(__dirname, 'out'),
    filename: 'extension.js',
    libraryTarget: 'commonjs2'
  },
  externals: {
    vscode: 'commonjs vscode'
  },
  resolve: {
    extensions: ['.ts', '.js']
  },
  module: {
    rules: [
      {
        test: /\.ts$/,
        exclude: /node_modules/,
        use: [
          {
            loader: 'ts-loader'
          }
        ]
      },
      {
        test: /\.wasm$/,
        type: 'asset/resource'
      }
    ]
  },
  devtool: 'source-map',
  infrastructureLogging: {
    level: "log"
  }
};
