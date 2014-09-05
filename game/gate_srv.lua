local z = require "z"
local config = require "config"
local log = require "logger"
local logger = log.get_logger("gate", "gate")
-- 
local agent_num = assert(config.gate_srv.agent_num);

local function main()
	local agents = {}
	for i=1, agent_num do
		agents[i] = z.spawn("zlua", "agent_srv")
		logger:log(string.format("start agentsrv(%d) succ", i))

		z.regist(string.format("agent_srv%d", i), agents[i])
	end

	local succ, sockfd = z.socket_listen(z.HEAD_PROTO_TYPE,
										config.gate_srv.host,
										config.gate_srv.port)
	if not succ then
		logger:log("start server fail")
		return
	end

	local next_agent = 1
	z.socket_start(sockfd, 
						   function(newsockfd)
							   z.send("lua", agents[next_agent], "open", newsockfd)
							   next_agent = next_agent + 1
							   if next_agent > agent_num then
								   next_agent = 1
							   end
						   end)

	logger:log(string.format("start gate srv succ, listen on(%s:%d)", 
				config.gate_srv.host,
				config.gate_srv.port))
end

z.start(function()
			main()
		end)


