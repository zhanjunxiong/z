local z = require "z"
local config = require "config"
local loader = require "loader"
local log = require "logger"
local logger = log.get_logger("fps", "fps")


local filename
if select("#", ...) > 0 then
	filename = select(1, ...)
end

z.dispatch("lua", function(from, session, ...)
				  end)

local function main()
	-- 编译proto
	loader.build_pb()
	-- 启动日志服
	local pid 
	if filename then
		pid = z.spawn(config.log_srv.modname, 
						filename, 
						config.log_srv.max_lines, 
						config.log_srv.rotate_dir)
	else
		pid = z.spawn(config.log_srv.modname) 
	end
	assert(pid)
	z.regist("logsrv", pid)

	logger:log("start logsrv succ")
	-- 启动基础服务
	for k, v in pairs(config.services) do
		for _, name in pairs(v) do
			local pid = z.spawn("zlua", k)
			if k == "db_srv" then
				local ret = z.call("lua", pid, "start")
			end
			z.regist(name, pid)
			logger:log(string.format("start (%s) succ", name))
		end	
	end
	-- 启动gm
	local pid = z.spawn("zlua", "gm_srv")
	assert(pid)

	-- 启动对外网关
	local pid = z.spawn("zlua", "gate_srv")
	assert(pid)
	z.regist("gate_srv", pid)
	logger:log("start gatesrv succ")
	z.exit()
end

z.start(function()
			main()
		end)


