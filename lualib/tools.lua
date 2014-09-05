
function table2str(t, prefix, indent)
	assert(type(t) == "table")
	
	indent = indent or 0
	prefix = prefix or ""

	local table_str = {}
	
	local tab_prefix = string.rep("  ", indent)	
	local tab_prefix2 = string.rep("  ", indent + 1)
	local str = string.format("%s{", prefix)
	table.insert(table_str, str)
	for k, v in pairs(t) do
		local k_str = ""
		local v_str = ""
		if type(k) == "string" then
			k_str = k
		elseif type(k) == "number" then
			k_str = "[" .. tostring(k) .. "]"
		else
			k_str = ""
		end

		if type(v) == "string" then
			v_str = string.format("%q", v)
		elseif type(v) == "number" then
			v_str = tostring(v)
		elseif type(v) == "table" then
			v_str = table2str(v, "", indent + 1)
		end

		local str = string.format("%s%s = %s,", tab_prefix2, k_str, v_str)
		table.insert(table_str, str)
	end

	table.insert(table_str, string.format("%s}", tab_prefix))
	return table.concat(table_str, "\n")
end

--[[
local test_table = {
	a = {
			1, 
			2, 
			3
		},

	b = {
			hello = 1,
			world = 2,
			[4] = {4, 5, 6},
		},

	c = {
			t1 = 2,
			t2 = 3,
			t4 = {
					tt1 = b,
					tt2 = 5,
					tt3 = "helloworld",
					tt6 = {
							1, 2, 3, 
							g = 5,
						  }
				 },
		},
}

for k, v in pairs(test_table) do
	print(table2str(v, k .. "=" ))
end
--]]
--
function obj2str(obj, prefix)
	prefix = prefix or ""
	local obj_type = type(obj)
	if obj_type == "number" then
		return string.format("%s = %d", prefix, obj)
	end

	if obj_type == "string" then
		return string.format("%s = %q", prefix, obj)
	end

	if obj_type == "table" then
		return table2str(obj, prefix .. " = ")
	end
end

--[[
local test_cases = {
	1,
	"hello world",
	{
		1,
		2,
		{3, 4, 5, "hello world"},
	}
}

for k, v in pairs(test_cases) do
	print(obj2str(v, k))
end
--]]
--
