local source = debug.getinfo(1, "S").source
local current_dir = source:match("^@(.+[\\/])") or "./"
package.path = current_dir .. "../../../common/lua/?.lua;" .. package.path

local runner = require("benchmark_runner")
runner.run_main("numeric_loops", arg)
