local M = {}

local zmysql = require "zmysql"

local connections = setmetatable(
					{},
					{
						__gc = function(p)
								for _, v in pairs(p) do
									for k, mysql in pairs(v) do
										if type(k) == 'number' then
											mysql:close() 
										end
									end
								end
							   end,
					})

function M.select_connection(dbname, id)
	local con = connections[dbname]
	assert(con, string.format("dbname: %s", dbname))

	local pos = math.fmod(id, con.count) + 1
	local conn = con[pos]
	while not conn:ping() do
		print("mysql is dicconect ping fail")	
	end

	return conn
end

local usage = [[

need db_config,
for example:
	db_config = {
		mysql_srv = {
			{host="xxx", dbname="xxx", user="xxx", passwd="xxxx"},
		},
	}

]]

--@param db_cofig = {
--			mysql_srv = {
--				{host=xxx, dbname=xx, user=xxx, passwd=xxx},
--			}
--		 }
function M.main(db_config)

	assert(type(db_config) == "table", usage)
	-- 连接数据库
	for pos, cfg in pairs(db_config.mysql_srv) do
		connections[cfg.dbname] = connections[cfg.dbname] or {}	

		local con = connections[cfg.dbname]
		local count = con.count or 0
		local mysql = zmysql.connect(cfg.host, cfg.dbname, cfg.user, cfg.passwd) 
		if mysql then
			--[[
			print(string.format("connect to %s %s %s %s succ", 
					cfg.host, 
					cfg.dbname, 
					cfg.user, 
					cfg.passwd))
			]]
			con[pos] = mysql
			con.count = count + 1

		else
			error(string.format("connect to %s %s %s %s fail", 
					cfg.host, 
					cfg.dbname, 
					cfg.user, 
					cfg.passwd))
		end
	end
	return connections
end

return M

