local c = require "z.c"
local tcpserver = require "tcpserver"
local tcpclient = require "tcpclient"

local z = {
	-- see src/message.h
	-- 协议组
	MSG_TYPE_PROTO_PB = 0x01,
	MSG_TYPE_PROTO_LUA = 0x02,
	-- 内部消息
	MSG_TYPE_NOTCOPYDATA = 0x010000,	
	MSG_TYPE_COPYDATA = 0x020000,
	MSG_TYPE_RESPONSE = 0x040000,
	MSG_TYPE_NEWSESSION = 0x80000,	
	
	-- tcp msg type
	-- see src/tcp_message.h
	-- same tcp_message type  
	TCP_MSG_TYPE_SEND=1,
	TCP_MSG_TYPE_CLOSE=2,
}

local proto = {}
local fork_queue = {}
local watching_session = {}
local watching_service = {}

local session_id_coroutine = {}
local session_coroutine_id = {}
local session_coroutine_address = {}

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

--@param 
--		protoname 转发协议名字
--		from 来源地址
--		to 目标地址
--		可选参数
--			(str) or (data, sz)
function z.redirect(prototype, from, to, ...)
	assert(type(to) == "string")
	assert(type(from) == "string")
	return c.redirect(from, to, 0, prototype, ...)
end

--@param 
--		protoname 发送协议名字
--		to 目标地址
--		可选参数
--			(str) or (data, sz)
function z.send(protoname, to, ...)
	local class = proto[protoname]
	assert(class)
	assert(type(to) == "string")
	return c.send(to, 0, class.prototype, class.pack(...))
end

local coroutine_pool = {}
local function co_create(func)
	assert(type(func) == 'function')

	local co = table.remove(coroutine_pool)
	if co == nil then
		co = coroutine.create(function(...)
					func(...)
					while true do
						func = nil
						coroutine_pool[#coroutine_pool+1] = co
						func = coroutine.yield("EXIT")
						func(coroutine.yield())
					end
				end)
	else
		coroutine.resume(co, func)
	end
	return co
end 

local function yield_call(service, session)
	watching_session[session] = service
	local succ, msg, sz = coroutine.yield("CALL", session)
	watching_session[session] = nil
	assert(succ, "capture an error")
	return msg, sz
end

function z.ret(msg, sz)
	msg = msg or ""
	coroutine.yield("RETURN", msg, sz)
end

--@berif  
--@param 
--		protoname 调用协议
--		addr 服务地址
--		可选参数
--			(str) or (data, sz)
function z.call(protoname, addr, ...)
	local class = proto[protoname]
	assert(class)
	--[[
	-- 暂时不加监控
	if watching_service[addr] = false then
		error("Service is dead")
	end
	--]]
	local session = c.send(addr, 0, bit32.bor(class.prototype,z.MSG_TYPE_NEWSESSION),class.pack(...))
	if session == nil then
		error("call to invalid address: " .. addr)
	end
	return class.unpack(yield_call(addr, session))
end

function z.timeout(time, nomore, func)
	local str = c.command("TIMEOUT", string.format("%s %s", time, nomore))
	assert(type(str) == "string")
	local session, id = string.match(str, "(%d+)%s+(%d+)")
	session = tonumber(session)
	local co = co_create(func)
	assert(session_id_coroutine[session] == nil)
	session_id_coroutine[session] = co
	return tonumber(id)
end

function suspend(co, result, cmd, param, size)
	if not result then
		local session = session_coroutine_id[co]
		local addr = session_coroutine_address[co]
		if session and session ~= 0 then
			print("some error, session: " .. session .. " addr: " .. addr)
		end
		session_coroutine_id[co] = nil
		session_coroutine_address[co] = nil
		error(debug.traceback(co, tostring(cmd)))
	end

	if cmd == "CALL" then
		session_id_coroutine[param] = co
	elseif cmd == "RETURN" then
		local co_session = session_coroutine_id[co]
		local co_address = session_coroutine_address[co]
		if param == nil then
			error(debug.traceback(co))
		end
		c.send(co_address, co_session, z.MSG_TYPE_RESPONSE, param, size) 
		return suspend(co, coroutine.resume(co))
	elseif cmd == "EXIT" then
		-- coroutine exit
		session_coroutine_id[co] = nil
		session_coroutine_address[co] = nil
	else
		error("unknown cmd: " .. cmd .. "\n" .. debug.traceback(co))
	end
end

local function raw_dispatch_message(prototype, from, session, msg, sz, ...)
	-- response
	if prototype == z.MSG_TYPE_RESPONSE then
		local co = session_id_coroutine[session]
		if co then
			session_id_coroutine[session] = nil
			suspend(co, coroutine.resume(co, true, msg, sz))
		else
			error("session: " .. session)
			error("unknow response, prototype: " .. prototype .. " from: " .. from)
		end
	else
		prototype = bit32.band(prototype, 0x0000FFFF)
		local class = assert(proto[prototype], prototype)	
		local f = class.dispatch
		if f then
			local co = co_create(f)
			session_coroutine_id[co] = session
			session_coroutine_address[co] = from
			suspend(co, coroutine.resume(co, from, session, class.unpack(msg, sz, ...)))
		else
			error('unkown request')
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

function z.tcpserver_send(name, id, ...)
	tcpserver.send(name, id, z.TCP_MSG_TYPE_SEND, ...)
end

function z.tcpserver_close(name, id)
	tcpserver.send(name, id, z.TCP_MSG_TYPE_CLOSE)
end

function z.tcpclient_send(name, ...)
	tcpclient.send(name, z.TCP_MSG_TYPE_SEND, ...)
end

function z.tcpclient_close(name, id)
	tcpclient.send(name, z.TCP_MSG_TYPE_CLOSE)
end

function z.net_pack(...)
	return c.net_pack(...)
end

function z.net_unpack(...)
	return c.net_unpack(...)
end

--@berif
--@param name  服的名字
-- 		 boot_file  启动文件
function z.load_service(name, boot_file)
	assert(type(name) == "string")
	assert(type(boot_file) == "string")
	local srv_name = z.command("LAUNCH", string.format("%s zlua %s", name, boot_file))
	assert(type(srv_name) == "string", string.format("load service(%s) fail, boot file(%s)", name, boot_file))
	return srv_name
end

-- 日志等级类型
-- see service_logger.c
local MSG_TYPE_LOG_ERROR=0
local MSG_TYPE_LOG_WARN=1
local MSG_TYPE_LOG_LOG=2
local MSG_TYPE_LOG_MSG=3
local MSG_TYPE_LOG_DEBUG=4
local MSG_TYPE_LOG_STAT=5
local MSG_TYPE_LOG_LEN=6

function z.print(logger, from, level, text)
	assert(type(logger) == 'string')
	assert(type(text) == 'string')
	z.redirect(level, from, logger, text)
end

function z.error(logger, from, text)
	z.print(logger, from, MSG_TYPE_LOG_ERROR, text)
end

function z.warn(logger, from, text)
	z.print(logger, from, MSG_TYPE_LOG_WARN, text)
end

function z.log(logger, from, text)
	z.print(logger, from, MSG_TYPE_LOG_LOG, text)
end

function z.msg(logger, from, text)
	z.print(logger, from, MSG_TYPE_LOG_MSG, text)
end

function z.debug(logger, from, text)
	z.print(logger, from, MSG_TYPE_LOG_DEBUG, text)
end

--
-- 注册协议
--
z.register_protocol(
	{
		protoname = "lua",
		prototype = z.MSG_TYPE_PROTO_LUA,
		pack = z.lua_pack,
		unpack = z.lua_unpack,
	})

return z

