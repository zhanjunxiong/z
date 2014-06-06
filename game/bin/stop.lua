#! /usr/local/bin/lua

local file = io.open('proc/pid', 'r')
local pid = file:read()
os.execute('kill -15 ' .. pid)
os.exit(1)
