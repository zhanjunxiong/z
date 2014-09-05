local z = require "z"
local config = require "config"
local modules = require "modules"
local log = require "logger"
local logger = log.get_logger("hall", "player")

local proto_name = "lua"
local E_SRV_STATE_HALL = 0
local E_SRV_STATE_CHANNEL = 1
local E_SRV_STATE_ROOM = 2

local db_srv_addr
local function select_db_srv(uid) 
	if not db_srv_addr then
		db_srv_addr = {}
		local db_srv_list = config.services.db_srv
		for _, name in pairs(db_srv_list) do
			local pid = z.get_pid(name)
			assert(pid)
			table.insert(db_srv_addr, pid)
		end
	end

	assert(#db_srv_addr > 0)
	local pos = math.fmod(uid, #db_srv_addr) + 1
	return db_srv_addr[pos]
end

PlayerClass = {}
PlayerClass.__index = PlayerClass

function PlayerClass:new(sockfd, uid, srv_state)
	local o = {
		-- sockfd
		sockfd = sockfd,
		-- 登陆状态
		state = 0,
		-- 服务器状态机
		srv_state = srv_state,
		-- 服务编号
		srv_id = 0,
		-- 玩家数据
		data = {},
		-- 玩家uid
		uid = uid,
	}

	setmetatable(o, self)
	return o
end

local agent_srv_addr = nil
local function select_agent_srv(uid)
	if not agent_srv_addr then
		agent_srv_addr = {}
		local agent_num = config.gate_srv.agent_num
		for i=1, agent_num do
			local name = string.format("agent_srv%d", i)
			local pid = z.get_pid(name)
			assert(pid)
			table.insert(agent_srv_addr, pid)
		end
	end

	assert(#agent_srv_addr > 0)
	local pos = math.fmod(uid, #agent_srv_addr) + 1
	return agent_srv_addr[pos]
end


function PlayerClass:send(cmd, msg)
	local addr = select_agent_srv(self.uid)
	z.send(proto_name, 
			addr, 
			"forward",
			self.sockfd, 
			self.uid,
			0,
			cmd,
			msg)
end

function PlayerClass:broadcast(list, cmd, msg)
	assert(type(list) == "table")

	local addr = select_agent_srv(self.uid)
	z.send(proto_name, 
			addr,
			"forward",
			list, 
			self.uid,
			0,
			cmd,
			msg)
end

function PlayerClass:load_data()
	if not user_data_table then
		user_data_table = {}
		for k, _ in pairs(config.db_tables["cs"]) do
			table.insert(user_data_table, k);
		end
	end

	local uid = self.uid
	
	-- 开始加载数据
	local ret, data = z.call(proto_name, 
							select_db_srv(uid),
							"load",
							"cs",
							uid,
							user_data_table
							)
	if not ret then
		return -1
	end

	if data.user == nil then
		return 1
	end

	-- 登陆成功
	for k, v in pairs(data) do
		self.data[k] = v
	end

	local player_data = self.data
	player_data.user.last_login_time = os.time()
	return 0
end

function PlayerClass:save_data()
	if self.srv_state == E_SRV_STATE_CHANNEL then
		z.send(proto_name,
				modules["channel"]["select_channel_srv"](self.srv_id),
				proto_name,
				"post",
				self.uid,
				"channel.offline",
				{}
				)
	elseif self.srv_state == E_SRV_STATE_ROOM then
		z.send(proto_name,
				modules["room"]["select_room_srv"](self.srv_id),
				proto_name,
				"post",
				self.uid,
				"room.offline",
				{}
				)
	end

	local uid = self.uid
	local ret = z.call(proto_name,
						select_db_srv(uid),
						"save",
						"cs",
						uid,
						self.data
						)
	if not ret then
		logger:log(string.format("uid(%d) save data error", uid))
	end
end

function PlayerClass:on_message(seq, cmd, msg)
	-- 玩家还没有加载数据
	if self.state == 0 then
		-- 在加载数据中
		self.state = 1
		local ret = self:load_data()
		if ret < 0 then
			logger:log(string.format("uid(%d) load data error", self.uid))
			msg.errno = -2
			self.state = 0
			return msg
		end

		if ret == 1 then
			logger:log(string.format("uid(%d) is newhand", self.uid))
			msg.errno = 2
			self.state = 3
			return msg
		end

		msg.errno = 0
		self.state = 2
		return msg
	end
	
	local mod, func = string.match(cmd, "([%a_]+)%.([%a_]+)")
	if self.state == 2 then
		local succ, msg = pcall(modules[mod][func], self, cmd, seq, msg)
		if not succ then
			logger:log(tostring(msg))
			return
		end
		return msg
	end

	-- 新手玩家
	if self.state == 3 then
		local succ, msg = pcall(modules[mod][func], self, cmd, seq, msg)
		if not succ then
			logger:log(tostring(msg))
			return
		end
		return msg
	end
end


