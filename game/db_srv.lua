local z = require "z"
local mysqls = require "mysqls"
local pb = require "protobuf"
local loader = require "loader"
local config = require "config"
local log = require "logger"
local logger = log.get_logger("db", "db")

local db_tables_config = config.db_tables
local db_expand_tables_config = config.db_expand_tables

local function save_to_mysql(uid, dbname, table_name, table_data)
	local mysql = mysqls.select_connection(dbname, uid)
	if not mysql then
		print("ERROR: mysql is nil")
		return false
	end

	local timenow = os.time()

	local db_expand_tables = db_expand_tables_config[dbname]
	local descriptor = db_tables_config[table_name]
	if not db_expand_tables[table_name] then
		local ret = pb.encode(descriptor, table_data, 
					function(buffer, len)
						local sql_str = 
		   string.format("replace into %s(uid, table_bin, update_time)values(%d, '%s', %d)", 
								table_name, 
								uid, 
								mysql:escape_string(buffer, len), 
								timenow)

						if not mysql:command(sql_str) then
							return false
						end
						return true
					end)

		return ret
	end

	local sql_str = string.format("replace into %s(", table_name)
	for k, v in pairs(table_data) do
		if k == "update_time" then
		else
			sql_str = sql_str .. k .. ","
		end
	end
	
	sql_str = sql_str .. "update_time)values(" 
	for k, v in pairs(table_data) do
		--print(string.format("k(%s), v(%s)", k, v))
		if k == "update_time" then
		else
			if type(v) == "number" then
				sql_str = sql_str .. v .. ","
			elseif type(v) == "string" then
				sql_str = sql_str .. "'" .. mysql:escape_string(v) .. "',"
			else
				error("type(%s) not support!", type(v))
			end
		end
	end
	sql_str = sql_str .. timenow .. ")"
	logger:log(string.format("[save][%d][%s]", uid, sql_str))
	if not mysql:command(sql_str) then
		return false
	end
	return true
end

local function select_from_mysql(uid, dbname, table_name)
	--print("select from mysql, uid: " .. uid .. " table name: " .. table_name)
	local mysql = mysqls.select_connection(dbname, uid)
	if not mysql then
		print("error, mysql is nil")
		return false
	end
	
	local sql_str = string.format("select * from %s where uid=%d", table_name, uid)
	logger:log(string.format("[select][%d][%s]", uid, sql_str))
	local cursor = mysql:select(sql_str)
	if not cursor then
		return false
	end
	
	local result = cursor[1] or nil
	if not result then
		return true
	end

	local db_expand_tables = db_expand_tables_config[dbname]
	local descriptor = db_tables_config[table_name]
	if not db_expand_tables[table_name] then
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
function CMD.load(dbname, uid, tables)
	local player = {}
	for _, table_name in pairs(tables) do
		local ret, data = select_from_mysql(uid, dbname, table_name)
		if ret then
			player[table_name] = data
		else
			return false
		end
	end

	return true, player
end

function CMD.save(dbname, uid, tables)
	for name, data in pairs(tables) do
		if not save_to_mysql(uid, dbname, name, data) then
			return false
		end
	end
	return true
end

function CMD.start()
	-- 连接数据库
	mysqls.main(config.db_srv)
	return true
end

local function main()
	-- 加载proto文件	
	loader.load_proto()
end

-- 注册回调函数	
z.dispatch("lua", 
				--@param from 来源地址
				--		 session 服务回话id
				function(from, session, cmd, dbname, uid, msg)
					local func = CMD[cmd]
					assert(type(func) == 'function')
					z.ret(func(dbname, uid, msg))
				end)

z.start(
	function()
		main()
	end)


