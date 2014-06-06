local z = require "z"
local zmysql = require "zmysql"
local pb = require "protobuf"
local loader = require "loader"

loader.load_proto()

local mysql_srv = {
	{host = "127.0.0.1", dbname="cs", user="kbcs", passwd="123456"},
	{host = "127.0.0.1", dbname="cs", user="kbcs", passwd="123456"},
}

local descriptors = {
	'user' = 'playerdata.user',
}

local mysql_connections = mysql_connections or {}
local mysql_count = mysql_count or 0

local function select_connection(uid)
	local pos = math.fmod(uid, mysql_count) + 1
	return mysql_connections[pos]
end

local function save_to_mysql(uid, table_name, table_data)
	local mysql = select_connection(uid)
	if not mysql then
		return false
	end

	local timenow = os.time()
	local descriptor = descriptors[table_name]
	if descriptor then
		local ret = pb.encode(descriptor, table_data, 
					function(buffer, len)
						local sql_str = 
							string.format("repleace into %s(uid, table_bin, update_time)values(%d, '%s', %d)", 
								table_name, uid, mysql:escape_string(buffer, len), timenow)

						if not mysql:command(sql_str) then
							return false
						end
						return true

					end)

		return ret
	end

	local sql_str = string.format("repleace into %s(uid", table_name)
	for k, v in pairs(table_data) do
		sql_str = sql_str .. "," .. k
	end

	sql_str = sql_str .. ",update_time)values("
	for k, v in pairs(table_data) do
		if type(v) == "number" then
			sql_str = sql_str .. v .. ","
		elseif type(v) == "string" then
			sql_str = sql_str .. mysql:escape_string(v) .. ","
		else
			error("type(%s) not support!", type(v))
		end
	end
	sql_str = sql_str .. timenow .. ")"
	if not mysql:command(sql_str) then
		return false
	end
	return true
end

local function select_from_mysql(uid, table_name)
	local mysql = select_connection(uid)
	if not mysql then
		return false
	end
	
	local sql_str = string.format("select * from %s where uid=%d", table_name, uid)
	local cursor = mysql:select(sql_str)
	if not cursor then
		return false
	end
	
	local result = cursor[1] or nil
	if not result then
		return false
	end
	
	local descriptor = descriptors[table_name]
	if descriptor then
		local table_bin = result.table_bin
		local ret, strerr = pb.decode(descriptor, table_bin, string.len(table_bin))
		if not ret then
			error(string.format("uid(%d) table_name(%s) pb decode error (%s)", uid, table_name, strerr))
			return false
		end
		return true, ret
	end

	return true, result
end

local CMD = {}
function CMD.load(uid, tables)
	local player = {}
	for _, table_name in pairs(tables) do
		local ret, data = select_from_mysql(uid, table_name)
		if ret then
			player[table_name] = data
		else
			return false
		end
	end

	return true, player
end

function CMD.save(uid, tables)
	for name, data in pairs(tables) do
		if not save_to_mysql(uid, name, data) then
			return false
		end
	end
	return true
end

local function main()
	-- 连接数据库
	for pos, cfg in pairs(mysql_srv) do
		mysql_count = mysql_count + 1
		local mysql = zmysql.create()	
		if mysql:connect(cfg.host, cfg.dbname, cfg.user, cfg.passwd) then
			mysql_connections[pos] = mysql
		else
			error(string.format("connect to %s %s %s %s fail", cfg.host, cfg.dbname, cfg.user, cfg.passwd))
		end
	end
	
	z.dispatch("lua", 
					--@param from 来源地址
					--		 session 服务回话id
					function(from, session, cmd, msg)
						local func = CMD[cmd]
						assert(type(func) == 'function')
						local ret = func(msg)
						z.ret(z.lua_pack(ret))
					end)
end

z.start(
	function()
		main()
	end)


