local z = require "z"

local CMD = {}
function CMD.login(uid, passwd)
	if uid == tonumber(passwd) then
		return true
	end
	return false
end

local function main()
	z.dispatch("lua", 
			--@param from 来源地址
			--		 session 服务回话id
			function(from, session, cmd, ...)
				local func = CMD[cmd]
				assert(type(func) == 'function')
				z.ret(func(...))
			end)
end

z.start(
	function()
		main()
	end)


