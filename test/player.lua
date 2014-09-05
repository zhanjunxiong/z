local z = require "z"

local srv_addr = "127.0.0.1"
local srv_port = 10001

local gm_port = 8001

local M = {}
function M:new(o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self
	return o
end

function M:gm_login()
	self.mm_pid = z.spawn("zlua", "mm")
	local ret = z.call("lua", self.mm_pid, "start", srv_addr, gm_port)
	if not ret then
		print("load fail")
		return false
	end

	return true
end

function M:login(uid, passwd)
	self.mm_pid = z.spawn("zlua", "mm")
	local ret = z.call("lua", self.mm_pid, "start", srv_addr, srv_port)
	if not ret then
		print("load fail")
		return
	end

	local login_msg = {
		uid = uid,
		password = tonumber(passwd),
	}
	local ret, msg = self:call("login.LOGIN", login_msg)
	if not ret then
		print("call faile")
		os.exit()
	end

	if msg.errno == nil then
		print("login succ")
		return
	end

	if msg.errno == -1 then
		print("login fail")
		os.exit()
	end

	if msg.errno == 1 then
		print("login succ")
		return
	end

	print("msg.errno is: " .. msg.errno)
	os.exit()
end

function M:call(cmd, msg)
	return z.call("lua", self.mm_pid, "forward", cmd, msg)
end

function M:close()
end

return M


