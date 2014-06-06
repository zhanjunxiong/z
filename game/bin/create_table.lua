#! /usr/local/bin/lua

local Config = require "config"
local zmysql = require "zmysql"
local luapb = require "luapb"
require "dir"

local abs_path = "proto/playerdata"
local path = os.getenv("PROTO_PATH") .. abs_path

local function load_pb(path, abs_path)
	for fname, ftype in dir(path) do
		if fname == "." or fname == ".." then
		else
			if ftype == "file" then
				if string.find(fname, ".proto$") then
					local file_name = fname
					luapb.import(abs_path .. "/" .. file_name)
				end
			else
				load_pb(path .. "/" .. fname, abs_path .. "/" .. fname)
			end
		end
	end
end

load_pb(path, abs_path)

function create_table(mysql_conn, dbname, table_name)
    local str = string.format('CREATE TABLE %s(uid INT NOT NULL, table_bin BLOB NOT NULL, update_time INT NOT NULL, PRIMARY KEY(`uid`)) ENGINE = INNODB DEFAULT CHARSET=utf8;', table_name)
    print(str)
    mysql_conn:command(str)
end

function alter_table(mysql_conn, dbname, table_name, pb_file)
    local descriptor = luapb.get_descriptor(pb_file)
    if not mysql_conn:command('USE information_schema') then
        return false
    end
    local str = string.format("SELECT table_schema, table_name, data_type, column_name FROM columns WHERE table_schema='%s' and table_name='%s'", dbname, table_name)
 
    local table_desc = mysql_conn:select(str)
    if not mysql_conn:command('USE '..dbname) then
        return false
    end
    for _, field in pairs(table_desc) do
        table_desc[field.column_name] = field
    end
    for field_name, field in pairs(descriptor) do
        if table_desc[field_name] == nil then
            --增加一个字段
            local sql = 'ALTER TABLE '..table_name
            local field_type = 'INT'
            if field.type == 'string' then 
                field_type = 'varchar(128)' 
            elseif field.type == 'message' then
                field_type = 'BLOB'
            end
            sql = sql..string.format(' ADD %s %s NOT NULL ', field_name, field_type)
            if type(field.default) == 'string'  then
                sql = sql..' DEFAULT \''..field.default..'\''
            elseif type(field.default) == 'number' then
                sql = sql..' DEFAULT '..field.default
            end
            print(sql)
            mysql_conn:command(sql)
        end
    end
    return true
end

--功能:创建展开表
function create_expand_table(mysql_conn, dbname, table_name, pb_file)
    local descriptor = luapb.get_descriptor(pb_file)
    local sql = string.format('CREATE TABLE %s (', table_name)
    for field_name, field in pairs(descriptor) do
        local field_type = 'INT'
        if field_name == 'bin' then
            field_type = 'BLOB'
        elseif field.type == 'string' then 
            field_type = 'varchar(128)' 
        elseif field.type == 'message' then
            field_type = 'BLOB'
        elseif field.type == 'float' then
            field_type = 'FLOAT'
        end
        sql = sql..string.format('%s %s NOT NULL ', field_name, field_type)
        if field_name == 'id' and field_type == 'INT' then
            sql = sql..' auto_increment '
        elseif type(field.default) == 'string'  then
            sql = sql..' DEFAULT \''..field.default..'\''
        elseif type(field.default) == 'number' then
            sql = sql..' DEFAULT '..field.default
        end
        
        sql = sql..','
    end
    if descriptor.update_time == nil then
        sql = sql..' update_time INT NOT NULL,'
    end
    if not descriptor['id'] then
        sql = sql..' PRIMARY KEY(`uid`)) ENGINE=INNODB DEFAULT CHARSET=utf8;'
    else
        sql = sql..' PRIMARY KEY(`id`)) ENGINE=INNODB DEFAULT CHARSET=utf8;'
    end
    print(sql)
    mysql_conn:command(sql)
end

--@berif: 表是否存在
function is_table_exist(mysql_conn, dbname, table_name)
    if not mysql_conn:command('USE information_schema') then
        os.exit(1)
    end
    local str = string.format("SELECT table_name FROM TABLES WHERE table_schema='%s'", dbname)
    local reply = mysql_conn:select(str)
    if not mysql_conn:command('USE '..dbname) then
        os.exit(1)
    end

    for _, row in pairs(reply) do
        if row.table_name == table_name then
            return true
        end
    end
    return false
end

for pos, cfg in pairs(Config.db_srv.mysql_srv) do
	local mysql_conn = zmysql.create()
	if mysql_conn:connect(cfg.host, cfg.dbname, cfg.user, cfg.passwd) then
        print(string.format('success connect to mysql %s', cfg.host))
	else
        print(string.format('connect to %s(%s) fail', cfg.host, cfg.dbname))
		os.exit(1)
	end

    local dbname = cfg.dbname
    for table_name, pb_file in pairs(Config.user_table_map) do
        local is_exist = is_table_exist(mysql_conn, dbname, table_name, pb_file)
        if Config.expand_tables[table_name] == nil and not is_exist then
            create_table(mysql_conn, dbname, table_name)
        elseif Config.expand_tables[table_name] ~= nil and is_exist then
            alter_table(mysql_conn, dbname, table_name, pb_file)
        elseif Config.expand_tables[table_name] ~= nil and not is_exist then
            create_expand_table(mysql_conn, dbname, table_name, pb_file)
        end
    end
end

os.exit(1)


