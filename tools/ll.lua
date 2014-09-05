
local function main(...)
	local cmd = string.format('pgrep -lo z')
	local file = io.popen(cmd)
	local str = file:read("*a")
	file:close()
	print(str)
	os.exit(0)
end

main()

