--全局表
Config = {
	-- 网关配置
	gate_srv = {
		host = "0.0.0.0",
		port = 10001,
		agent_num = 4,
	},

	--  日志配置
	log_srv = {
		modname = 'logger',
		filename = "fps",
		rotate_dir = 'game/log',
		max_lines = 10000,
	},

	-- services list
	services = {
		hall_srv = {
			"hall_srv1",
		},
		login_srv = {
			"login_srv1",
			"login_srv2",
		},
		db_srv = {
			"db_srv1",
		--	"db_srv2",
		},
	},

	-- gm 后台配置
	gm_srv = {
		host = "0.0.0.0",
		port = 8001,
	},

	-------------------- db_srv -------------------
	db_srv = {
		mysql_srv = {
			{host = "127.0.0.1", dbname="cs", user="root", passwd="123456"},
			--{host = "127.0.0.1", dbname="cs", user="root", passwd="123456"},
		},
	},

	-- 数据库表
	db_tables = {
		-- key is dbname
		cs = {
			user = 'playerdata.user.User',
			item_package = 'playerdata.item.ItemPackage',
		},
	},

	db_expand_tables = {
		cs = {
			user = true,
		}
	},
}

return Config



