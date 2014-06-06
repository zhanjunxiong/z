local z = require "z"

local login_srv = "login_srv"
local db_srv = "db_srv"
local proto_name = "lua"

local socket_manager = socket_manager or {}
local player_manager = player_manager or {}

local CMD = {}
function CMD.open(id)
	print("open id: " .. id)

	local player = {
		-- 状态
		state = 0,
	}

	socket_manager[id] = player
end

function CMD.data(id, cmd, msg)
	local player = socket_manager[id]
	if player == nil then
		error(string.format("some error, id(%d)", id))
	end

	local mod, func = string.match(cmd, "(%a+)%.(%a+)")
	-- 未验证
	if player.state == 0 then
		-- 验证中
		player.state = 1
		local uid = msg.uid
		local ret, errno = z.call(proto_name, login_srv, "login", msg)
		msg.errno = errno
		if ret then
			print("uid: " .. uid .. " login succ")
			-- 验证成功
			-- 开始加载数据
			local ret, user = z.call(proto_name, db_srv, "load", uid)
			if not ret then
				return 
			end
			
		else
			print("uid: " .. uid .. " login fail")
		end
		return msg
	else
		func(player, msg)
	end

	-- 在验证中...
	if player.state == 1 then
		error(string.format("id(%d) authing", id))
		return
	end

	
end


function CMD.close(id)
	print("close id: " .. id)
	socket_manager[id] = nil
end

z.start(
	function()
		-- load login server
		z.load_service(login_srv, "login_srv")

		z.dispatch("lua", 
							--@param from 来源地址
							--		 session 服务回话id
							--		 	id 	连接id
							--		 	seq  连接序列号
							function(from, session, cmd, id, seq, subcmd, msg)
								local func = CMD[cmd]
								assert(type(func) == 'function')
								local ret = func(id, subcmd, msg)
								if ret then
									proto.tcpserver_send("gateway", id, subcmd, seq, ret)
								end
						 	end)
	end)

