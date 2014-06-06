
local z = require "z"

local CMD = {}
function CMD.login(msg)
	local uid = msg.uid
	local password = tonumber(msg.password)
	if uid == password then
		return true
	else
		return false
	end
end

z.start(
	function()
		z.dispatch("lua", 
							--@param from 来源地址
							--		 session 服务回话id
							function(from, session, cmd, msg)
								local func = CMD[cmd]
								assert(type(func) == 'function')
								local ret = func(msg)
								z.ret(z.lua_pack(ret))
						 	end)
	end)


