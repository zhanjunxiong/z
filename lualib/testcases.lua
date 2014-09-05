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

local test_cases = {}
local test_result = 0
local test_succ = 0
local test_fail = 0

--@param testcase
--			{
--			name = 'xxx',
--			func = function() ... end
--			}
local function regist_testcase(testcase)
	test_cases[#test_cases + 1] = testcase
	test_result = test_result + 1
end

local function run_testcase()
	for _, testcase in pairs(test_cases) do
		print("======================")
		print_blue("Run TestCase:" .. testcase.name)
		local ret = testcase.func()
		print_blue("End TestCase:" .. testcase.name)

		if ret then
			test_succ = test_succ + 1
		else
			test_fail = test_fail + 1
		end
	end

	print("Total TestCase: " .. test_succ + test_fail)
	print_green("Passed: " .. test_succ)
	print_red("Failed: " .. test_fail)
end

function print_r(obj, indent)
	local obj_type = type(obj)
	if obj_type == "number" then
		print(obj)
		return
	end

	if obj_type == "string" then
		print(string.format("%q", obj))
		return
	end

	indent = indent or 0
	if obj_type == "table" then
		for k, v in pairs(obj) do
			if type(k) == "string" then
				k = string.format("%q", k)
			end
			local suffix = ""
			if type(v) == "table" then
				suffix = "{"
			end
			local prefix = string.rep("\t", indent)
			formatting = prefix .. "[" .. k .. "]" .. "=" .. suffix
			if type(v) == "table" then
				print(formatting)
				print_r(v, indent + 1)
				print(prefix .. "},")
			else
				local value = ""
				if type(v) == "string" then
					value = string.format("%q", v)
				else
					value = tostring(v)
				end
				print(formatting .. value .. ",")
			end
		end
	end
end

function expect_eq(m, n)
	local m_type = type(m)
	local n_type = type(n)
	local ret = false
	if m_type == n_type then
		if m_type == 'number' or m_type == 'string' then
			if m == n then
				ret = true
			end
		elseif m_type == 'table' then
			ret = true
			for k, v in pairs(m) do
				if n[k] ~= v then
					ret = false
					break
				end
			end
		end
	end

	if not ret then
		print_red("Failed")
		print_red("Expect: " .. " type(m):" .. m_type)
		print_r(m)	
		print_red("Actual: " .. " type(n):" .. n_type)
		print_r(n)
	end
	return true
end

local function load_mod(mod_name)
	local mod = {}
	setmetatable(mod, {__index = _G})
	local func, err = loadfile(mod_name, "bt", mod)
	if not func then
		print_red(string.format("load mod(%s) fail, err(%s)", mod_name, err))
		return nil
	end
	func()
	return mod
end

local function main()
	if arg[1] == nil then
		print("please input test file name!\n")
		return
	end

	local file_name = string.gsub(arg[1], ".lua", "")
	local mod = load_mod(file_name .. ".lua")
	if not mod then
		return
	end

	for k, v in pairs(mod) do
		if type(v) == "function" and 
			string.sub(k, 1, 5) == "test_" then
			regist_testcase({name=k, func=v})
		end
	end
	run_testcase()
end

main()


