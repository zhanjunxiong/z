#! /usr/local/bin/lua

local file = io.open('proc/pid', 'r')
if not file then
	print('no start')
	os.exit(1)
end

local pid = file:read()
file:close()

local file = io.popen('ps auxw --sort=%%cpu | grep -E "%s && z" | grep -v "grep"', pid)
local str = file:read('*a')
file:close()
print(str)
os.exit(1)

