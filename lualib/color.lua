
local color = {
	r = "\27[1;31m",
	g = "\27[1;32m",
	y = "\27[1;33m",
	b = "\27[1;34m",
	p = "\27[1;35m"
}

local cn = "\27[0m"

local function print_red(str)
	print(string.format("%s%s%s", color.r, str, cn))
end

local function print_green(str)
	print(string.format("%s%s%s", color.g, str, cn))
end

local function print_blue(str)
	print(string.format("%s%s%s", color.b, str, cn))
end

return 
{
	cn = cn,

	r = color.r,
	g = color.g,
	y = color.y,
	b = color.b,
	p = color.p,


	print_red = print_red,
	print_green = print_green,
	print_blue = print_blue,
}

