local c = require "z.c"

local z = setmetatable({
	-- see src/message.h
	-- 协议组
	MSG_TYPE_RESPONSE = 0x001,
	MSG_TYPE_PROTO_LUA = 0x02,
	MSG_TYPE_PROTO_SYS = 0x03,
	MSG_TYPE_PROTO_SOCKET = 0x04,
	MSG_TYPE_PROTO_ERROR = 0x05,

	-- see src/define.h
	HEAD_PROTO_TYPE=1,
	HTTP_PROTO_TYPE=2,
}, {__gc = function(p)
		   end
   })



local proto = {}
local watching_session = {}
local watching_key = {}
local watching_service = {}
local error_queue = {}

local session_id_coroutine = {}
local session_coroutine_id = {}
local session_coroutine_address = {}
local session_coroutine_prototype = {}

function z.register_protocol(class)
	local protoname = class.protoname
	local prototype = class.prototype
	assert(type(protoname) == 'string')
	assert(type(prototype) == 'number')
	proto[protoname] = class
	proto[prototype] = class
end


function z.dispatch(prototype, func)
	local class = proto[prototype]
	assert(class)
	assert(type(func) == 'function')
	class.dispatch = func
end

function z.command(...)
	return c.command(...)
end

function z.lua_pack(...)
	return c.pack(...)
end

function z.lua_unpack(...)
	return c.unpack(...)
end

local coroutine_pool = {}
local coroutine_yield = coroutine.yield
local function co_create(func)
	assert(type(func) == 'function')

	local co = table.remove(coroutine_pool)
	if co == nil then
		co = coroutine.create(function(...)
					func(...)
					while true do
						func = nil
						coroutine_pool[#coroutine_pool+1] = co
						func = coroutine_yield("EXIT")
						func(coroutine_yield())
					end
				end)
	else
		coroutine.resume(co, func)
	end
	return co
end 

z.redirect = assert(c.redirect)

--@param 
--		protoname 发送协议名字
--		to 目标地址
--		可选参数
--			(str) or (data, sz)
function z.send(protoname, to, ...)
	local class = proto[protoname]
	assert(class, "proto name not found")
	assert(type(to) == "number", "dest address must be number address type:" .. type(to))

	if watching_service[to] == false then
		error(string.format("service(%d) is dead", to))
	end

	return c.send(to, 0, class.prototype, class.pack(...))
end

z.rawsend = assert(c.send)

local function k_yield_call(key, session)
	watching_key[session] = key
	local succ, msg, sz = coroutine_yield("CALL", session)
	watching_key[session] = nil
	if succ then
		return msg, sz
	end
	if msg then
		error("capture an error: " .. msg)
	end
	assert(succ, "capture an error")
end

function z.resume_kcall(key)
	for session, k in pairs(watching_key) do
		if k == key then
			local co = session_id_coroutine[session]
			if co then
				suspend(co, coroutine.resume(co, false, "resume k call"))
				session_id_coroutine[session] = "BREAK"
			end
		end
	end
end

function z.kcall(key, protoname, addr, ...)
	if watching_service[addr] == false then
		error(string.format("service(%d) is dead", addr))
	end

	local class = proto[protoname]
	assert(class)

	local session = c.send(addr, 
							nil, 
							class.prototype,
							class.pack(...))

	if session == nil then
		error("call to invalid address: " .. addr)
	end
	return class.unpack(k_yield_call(key, session))
end



local function yield_call(service, session)
	watching_session[session] = service
	local succ, msg, sz = coroutine_yield("CALL", session)
	watching_session[session] = nil
	assert(succ, "capture an error")
	return msg, sz
end

function z.ret(...)
	if select("#", ...) > 0 then
		coroutine_yield("RETURN", 
							0, 
							...)
	end
end

--@berif  
--@param 
--		protoname 调用协议
--		addr 服务地址
function z.call(protoname, addr, ...)
	if watching_service[addr] == false then
		error(string.format("service(%d) is dead", addr))
	end

	local class = proto[protoname]
	assert(class)

	local session = c.send(addr, 
							nil, 
							class.prototype,
							class.pack(...))

	if session == nil then
		error("call to invalid address: " .. addr)
	end
	return class.unpack(yield_call(addr, session))
end

function z.timercancel(id)
	c.command("TIMECANCEL", string.format("%s ", id))
end

function z.timeout(time, nomore, func)
	local str = c.command("TIMEOUT", string.format("%s %s", time, nomore))
	assert(type(str) == "string")
	local session, id = string.match(str, "(%d+)%s+(%d+)")
	session = tonumber(session)
	local co = co_create(
						function()
							local flag = nomore
							repeat
								local succ, ret = pcall(func)
								if succ then
									if ret then
										coroutine_yield("TIMEOUT", session)
									else
										flag = 0
									end
								else
									error(tostring(ret))
									coroutine_yield("TIMEOUT", session)
								end
							until flag == 0

							if nomore == 1 then
								z.timercancel(tonumber(id))
							end
						end)
	assert(session_id_coroutine[session] == nil)
	session_id_coroutine[session] = co
	return tonumber(id)
end

-- suspend is function
local suspend = nil
local function dispatch_error_queue()
	local session = table.remove(error_queue, 1)
	if session then
		local co = session_id_coroutine[session]
		session_id_coroutine[session] = nil
		return suspend(co, coroutine.resume(co, false))
	end
end

local function suspend(co, result, cmd, session, ...)
	if not result then
		local co_session = session_coroutine_id[co]
		local co_addr = session_coroutine_address[co]
		if co_session then
			c.send(co_addr, co_session, z.MSG_TYPE_PROTO_ERROR, nil)
		end
		session_coroutine_id[co] = nil
		session_coroutine_address[co] = nil
		session_coroutine_prototype[co] = nil
		error(debug.traceback(co, tostring(cmd)))
	end

	if cmd == "CALL" then
		session_id_coroutine[session] = co
	elseif cmd == "RETURN" then
		local co_session = session_coroutine_id[co]
		if co_session > 0 then		
			local co_address = session_coroutine_address[co]
			local co_prototype = session_coroutine_prototype[co]
			local class = proto[co_prototype]
			assert(class, "proto name not found in return")
			-- 1 is z.MSG_TYPE_RESPONSE	
			c.send(co_address, co_session, 1, class.pack(...)) 
		else
		end
		return suspend(co, coroutine.resume(co))
	elseif cmd == "EXIT" then
		-- coroutine exit
		session_coroutine_id[co] = nil
		session_coroutine_address[co] = nil
		session_coroutine_prototype[co] = nil
	elseif cmd == "TIMEOUT" then
		session_id_coroutine[session] = co
		--return suspend(co, coroutine.resume(co))
	else
		error("unknown cmd: " .. cmd .. "\n" .. debug.traceback(co))
	end

	dispatch_error_queue()
end


local function _error_dispatch(error_from, error_session)
	if error_session == 0 then
		-- service id down
		-- Don't remove watching_service, because user may call dead service
		watching_service[error_from] = false
		for session, srv in pairs(watching_session) do
			if srv == error_from then
				table.insert(error_queue, session)
			end
		end
	else
		-- capture an error for error_session
		if watching_session[error_session] then
			table.insert(error_queue, error_session)
		end
	end
end

local function unknow_response(from, session)
	local str = string.format("unknow response, prototype(%d) from(%d) session(%d)",
				prototype,
				from,
				session)
	error(str)
end

local function unknow_request(class, from, session)
	error('unkown request name: ' .. class.protoname)
end

local function raw_dispatch_message(prototype, from, session, msg, sz, ...)
	-- prototype 1(z.MSG_TYPE_RESPONSE) is response
	if prototype == 1 then
		local co = session_id_coroutine[session]
		if co == "BREAK" then
			session_id_coroutine[session] = nil
		elseif co then
			session_id_coroutine[session] = nil
			suspend(co, coroutine.resume(co, true, msg, sz))
		else
			unknow_response(from, session)
		end
	else
		local class = assert(proto[prototype], "prototype error, prototype: " .. prototype)	
		local f = class.dispatch
		if f then
			local co = co_create(f)
			session_coroutine_id[co] = session
			session_coroutine_address[co] = from
			session_coroutine_prototype[co] = prototype
			suspend(co, coroutine.resume(co, from, session, class.unpack(msg, sz, ...)))
		else
			unknow_request(class, from, session)
		end
	end
end

local function dispatch_message(...)
	local succ, err = pcall(raw_dispatch_message, ...)
	assert(succ, tostring(err))
end

local init_func = {}
function z.init(f, name)
	assert(type(f) == "function")
	if init_func == nil then
		f()
	else
		if name == nil then
			table.insert(init_func, f)
		else
			assert(init_func[name] == nil)
			init_func[name] = f
		end
	end
end

local function init_all()
	local funcs = init_func
	init_func = nil
	for k,v in pairs(funcs) do
		v()
	end
end

local function init_template(start)
	init_all()
	init_func = {}
	start()
	init_all()
end

local function init_service(start)
	local ok, err = xpcall(init_template, debug.traceback, start)
	if not ok then
		print("init service failed:", err)
	end
end

function z.start(start_func)

	c.callback(dispatch_message)
	z.timeout(0, 0, function()
		init_service(start_func)
	end)
end

function z.net_pack(...)
	return c.net_pack(...)
end

function z.net_unpack(...)
	return c.net_unpack(...)
end

--@berif creates a process which will execute the mod 
--@param
function z.spawn(mod, ...)
	local mod = mod or "zlua"
	local parm = ""
	for i=1, select("#", ...) do
		parm = parm .. select(i, ...) .. " "
	end

	local command = string.format("%s %s\r\n", mod, parm)
	-- print(command)
	local pid = z.command("LAUNCH", command)
	-- print(string.format("pid(%d) command(%s)", pid, command))
	return tonumber(pid)
end

local service_name
function z.service_name()
	if service_name then
		return service_name
	end
	return "UnKnow"
end


function z.regist(name, pid)
	assert(type(name) == "string")
	assert(type(pid) == "number" or type(pid) == "nil")

	local ret = c.regist(name, pid)
	assert(ret == 0)
	if not pid then
		service_name = name
	end
end

function z.unregist(name)
	assert(type(name) == "string")

	local ret = c.unregist(name)
	assert(ret == 0)
	service_name = nil
end

function z.get_pid(name)
	assert(type(name) == "string")

	local pid = c.name2id(name)
	return pid
end

local self_handle
function z.self()
	if self_handle then
		return self_handle
	end
	self_handle = tonumber(c.self())
	return self_handle
end

function z.exit(pid)
	if not pid then
		pid = z.self()
	end
	assert(type(pid) == "number")
	c.command("EXIT", pid)
end

function z.qlen(pid)
	if not pid then
		pid = z.self()
	end
	assert(type(pid) == "number")
	local len = c.command("QLEN", pid)
	if len then
		return tonumber(len)
	end
	return 0
end


--
-- socket 
--
local socket_proto = {}
local socket_dispatch 
local socket_cmd = {}
local sockets = setmetatable(
	{}, 
	{ __gc = function(p)
				for id, v in pairs(p) do
					c.close_socket(id)
				end
			 end
	}
)

function z.socket_dispatch(func)
	socket_dispatch = func
end

-- MSG_TYPE_SOCK_OPEN
socket_cmd[1] = function(_, id)
					if socket_dispatch then
						socket_dispatch.open(id)
					end
				end
-- MSG_TYPE_SOCK_DATA
socket_cmd[2] = function(_, id, data, sz)
					if socket_dispatch then
						socket_dispatch.data(id, data, sz)
					end
				end
-- MSG_TYPE_SOCK_CLOSE 
socket_cmd[3] = function(_, id)
					if socket_dispatch then
						socket_dispatch.close(id)
					end
				end
-- MSG_TYPE_SOCK_ACCEPT
socket_cmd[4] = function(id, newid)
					local cb = socket_proto[id]
					if not cb then
						return
					end
					cb(newid)
				end
-- MSG_TYPE_SOCK_CONNECT
socket_cmd[5] = function(_, id)
					if socket_dispatch then
						socket_dispatch.connect(id)
					end
				end
socket_cmd[6] = function(_, id, data, sz)
					print("err: " .. c.socket_tostring(data, sz))
				end

function z.socket_start(id, func)
	if func then
		socket_proto[id] = func	
	end
	c.socket_start(id)
end

function z.socket_listen(proto_type, addr, port)
	assert(type(proto_type) == "number")
	assert(type(addr) == "string")
	assert(type(port) == "number")
	local id =  c.socket_listen(proto_type, addr, port)
	if id <= 0 then
		return false
	end

	sockets[id] = id
	return true, id;
end

function z.socket_connect(proto_type, addr, port)
	assert(type(proto_type) == "number")
	assert(type(addr) == "string")
	assert(type(port) == "number")
	local id =  c.socket_connect(proto_type, addr, port, retry)
	if id <= 0 then
		return false
	end

	sockets[id] = id
	return true, id
end

z.socket_send = assert(c.socket_send)
z.socket_close = assert(c.socket_close)
z.socket_tostring = assert(c.socket_tostring)

--
-- 注册协议
--
z.register_protocol(
	{
		protoname = "lua",
		prototype = z.MSG_TYPE_PROTO_LUA,
		pack = z.lua_pack,
		unpack = z.lua_unpack,
		dispatch = function(...) 
						error("please regist lua callback ctx id:" .. z.self())
					end
	})

z.register_protocol(
	{
		protoname = "sys",
		prototype = z.MSG_TYPE_PROTO_SYS,
		pack = z.lua_pack,
		unpack = z.lua_unpack,
		dispatch = function(from, session, cmd, ...) 
					end
	})


z.register_protocol(
	{
		protoname = "socket",
		prototype = z.MSG_TYPE_PROTO_SOCKET,
		unpack = function(...)
					return ...
				 end,
		dispatch = function(from, session, data, sz, cmd, sockfd)
					socket_cmd[cmd](session, sockfd, data, sz)
				   end
	})

z.register_protocol(
	{
		protoname = "error",
		prototype = z.MSG_TYPE_PROTO_ERROR,
		unpack = function(...) return ... end,
		
		dispatch = _error_dispatch,
	})

return z

