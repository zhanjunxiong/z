local z = require "z"
local loader = require "loader"
local modules = require "modules"
local log = require "logger"
local logger = log.get_logger("hall", "hall")

require "tools"

g_player_manager = {}

local function socket_close(sockfd)
	z.socket_close(sockfd)
end

local CMD = {}
function CMD.data(sockfd, uid, seq, cmd, msg)
	local player = g_player_manager[uid]
	if player == nil then
		player = modules.player.PlayerClass:new(sockfd, uid, E_SRV_STATE_HALL)	
		if not player then
			logger:error("CMD.data PlayerClass:new return nil.")
			return
		end

		g_player_manager[uid] = player
	end

	-- 重复登录, 踢掉旧的连接
	if player.sockfd and player.sockfd ~= sockfd then
		socket_close(sockfd)
		-- 绑定新的sockfd	
		player.sockfd = sockfd
	end

	logger:log(string.format("<<<< [RECV] <<<< uid[%d] sockfd[%d] seq[%d] %s", 
						uid,
						sockfd,
						seq,
						table2str(msg, cmd)))
	-- 玩家消息处理
	return player:on_message(seq, cmd, msg)
end

function CMD.close(sockfd, uid)
	local player = g_player_manager[uid]
	if not player then
		return
	end

	player:save_data()
	g_player_manager[uid] = nil
end

local SYSCMD = {}
function SYSCMD.reload(mod_name)
	load_mod("game/hall_srv", mod_name, true)
	return 0
end

function SYSCMD.gm(fun_name, uid)
	return 0
end

local function main()
	-- load hall_srv mod
	load_mods("game", "hall_srv", true)

	z.dispatch("sys", function(from, session, cmd, ...)
							local func = SYSCMD[cmd]
							assert(type(func) == 'function')
							z.ret(func(...))
						end)
	
	z.dispatch("lua", 
				--@param from 来源地址
				--		 session 服务回话id
				function(from, session, cmd, sockfd, uid, seq, subcmd, msg)
					local func = CMD[cmd]
					assert(type(func) == 'function')
					local reply_msg = func(sockfd, uid, seq, subcmd, msg)
					if reply_msg then
						z.send("lua", 
								from, 
								"forward", 
								sockfd, 
								uid,
								seq, 
								subcmd, 
								reply_msg)
					end
				end)
end

z.start(
	function()
		main()
	end)

