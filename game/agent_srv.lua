local z = require "z"
local config = require "config"
local pb = require "protobuf"
local loader = require "loader"
local log = require "logger"
local logger = log.get_logger("hall", "hall")

local proto_name = "lua"
require "tools"

local function send(sockfd, seq, cmd, msg)
	return pb.encode(cmd, msg, 
								function(buffer, len)
									local b, blen = z.net_pack(seq, cmd, buffer, len)
									z.socket_send(sockfd, b, blen)
								end)
end

local hall_srv_addr = nil
local function get_hall_srv_addr()
	if hall_srv_addr then
		return hall_srv_addr
	end

	local hall_srv_name = config.services.hall_srv[1]		
	local pid = z.get_pid(hall_srv_name)
	assert(pid)	
	hall_srv_addr = pid
	return pid
end

local login_srv_addr = nil
local function select_login_srv(sockfd)
	if not login_srv_addr then
		login_srv_addr = {}
		local login_srv_list = config.services.login_srv	
		for _, name in pairs(login_srv_list) do
			local pid = z.get_pid(name)
			assert(pid)
			table.insert(login_srv_addr, pid)
		end
	end

	assert(#login_srv_addr > 0)
	local pos = math.fmod(sockfd, #login_srv_addr) + 1
	return login_srv_addr[pos]
end

local socket_manager = {}
local function on_message(sockfd, seq, cmd, msg)
	-- socket = 
	-- {
	-- 		check = xx
	-- 		uid = xx
	-- }
	local socket = socket_manager[sockfd]
	-- 验证sockfd有效性 
	if socket.check == nil then
		--@cmd login.LOGIN
		--@msg #login.LOGIN
		if not msg.uid then
			return
		end

		-- 在校验中
		socket.check = false
		local ret = z.kcall(sockfd, 
							proto_name, 
							select_login_srv(sockfd),
							"login",
							msg.uid,
							msg.password)
		if ret then
			-- 登陆校验成功
			socket.check = true
			socket.uid = msg.uid

			--msg.errno = 1
			--send(sockfd, seq, cmd, msg)
			local addr = get_hall_srv_addr()
			z.send(proto_name, addr, "data", sockfd, socket.uid, seq, cmd, msg)  
			return
		end

		socket.check = nil
		msg.errno = -1
		send(sockfd, seq, cmd, msg)
		return
	end

	if socket.check then
		--local mod, func = string.format(cmd, "([%a_]+)%.([%a_]+)")
		-- 大厅消息转发给大厅
		local addr = get_hall_srv_addr()
		z.send(proto_name, addr, "data", sockfd, socket.uid, seq, cmd, msg)  
	end
end

local socket_handle = {}
socket_handle.data = function(sockfd, data, sz)
					local result, seq, cmd, msg, sz	= z.net_unpack(data, sz)
					if not result then
						print("error: " .. seq)
						return
					end

					local result, strerr = pb.decode_all(cmd, msg, sz)
					if not result then
						print(string.format("pb decode error (%s)", strerr))
						return
					end
					
					socket_manager[sockfd] = socket_manager[sockfd] or {}
					on_message(sockfd, seq, cmd, result)
					end

socket_handle.close = function(sockfd)

					local socket = socket_manager[sockfd]
					if not socket then
						return
					end

					z.resume_kcall(sockfd)
					if socket.check then
						local addr = get_hall_srv_addr()
						z.send(proto_name, addr, "close", sockfd, socket.uid)  
					end

					socket_manager[sockfd] = nil
				   end

local CMD = {}
function CMD.open(sockfd)
	z.socket_start(sockfd)
end

function CMD.forward(sockfd, uid, seq, cmd, msg)
	if type(sockfd) == "number" then
		logger:log(string.format(">>>> [SEND] >>>> uid[%d] sockfd[%d] seq[%d] %s",
				uid,
				sockfd,
				seq,
				table2str(msg, cmd)))
	elseif type(sockfd) == "table" then
		logger:log(string.format(">>>> [BROARD] >>>> uid[%d] sockfd[%s] seq[%d] %s",
				uid,
				table2str(sockfd),
				seq,
				table2str(msg, cmd)))
	end
	send(sockfd, seq, cmd, msg)
end

function CMD.change_channel(sockfd, channel_id)
end

local function main(...)

	z.socket_dispatch(socket_handle)	
	z.dispatch("lua", 
				--@param from 来源地址
				--		 session 服务回话id
				function(from, session, cmd, ...)
					local func = CMD[cmd]
					assert(type(func) == 'function')
					z.ret(func(...))
				end)

end

-- 加载proto
loader.load_proto()

z.start(function()
			main()
		end)


