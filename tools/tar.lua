
local files = {
	"3rd",
	"src",
	"service-src",
	"lualib-src",
	"service",
	"lualib",
	"tools",
	".env",
	"game/mod/",
	"Makefile",
}

local function main(...)
	local tar_name = 'z.tar'
	if select("#", ...) >= 1 then
		tar_name = select(1, ...)
	end

	local cmd = string.format('tar -cf %s %s', 
							tar_name,
							table.concat(files, " "))

	print(cmd)
	os.execute(cmd)
	os.execute(string.format("3exe/sz %s", tar_name))
	os.exit(0)
end

main(...)

