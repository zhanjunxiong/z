--@berif: 全局表

Config = {
	proto_path = 'proto',
	-------------------- db_srv -------------------
	db_srv = {
		mysql_srv = {
			{host = "127.0.0.1", dbname="cs", user="root", passwd="123456"},
			{host = "127.0.0.1", dbname="cs", user="root", passwd="123456"},
		},
	},

	------------------- user_table -----------------
	user_table_map = {
		user = 'playerdata.user.User',
	},
	------------------- expand_tabls ---------------
	expand_tables = {
		user = 1,
	},
}

return Config



