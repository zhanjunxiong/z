local dir = require "dir"

local modules = {}

local function load(path, fileset, modulename, reload)
	modulename = modulename or pathname
	local oldmodule = modules[modulename]
	if not oldmodule then
		local newmodule = {}
		setmetatable(newmodule, {__index = _G})

		for _, filename in pairs(fileset) do
			local func, err = loadfile(path .. "/" .. filename, "bt", newmodule)
			if not func then
				print("ERROR: " .. err)
				return nil, err
			end

			func()
		end

		--print("modulename: " .. modulename)
		modules[modulename] = newmodule
		return newmodule
	end

	if not reload then
		return oldmodule
	end

	local oldcache = {}
	for k, v in pairs(oldmodule) do
		if type(v) == "table" then
			oldcache[k] = v
		end
		oldmodule[k] = nil
	end

	local newmodule = oldmodule
	for _, filename in pairs(fileset) do
		local func, err = loadfile(path .. "/" .. filename, "bt", newmodule)
		if not func then
			print("ERROR: " .. err)
			return nil, err
		end
		
		func()
	end

	for k, v in pairs(oldcache) do
		local mt = getmetatable(newmodule[k])
		if mt then setmetatable(v, mt) end

		newmodule[k] = v
	end
	return newmodule
end


local function get_fileset(pathname, filesets)
	for fname, ftype in dir.open(pathname) do
		if string.sub(fname, 1, 1) == "." then
		else
			if ftype == "file" then
				if string.format(fname, ".lua$") then
					table.insert(filesets, fname)
				end
			else
				filesets[fname] = filesets[fname] or {}
				get_fileset(pathname .. "/" .. fname, filesets[fname])
			end
		end
	end
end

function load_mod(path, modname, reload)
	local pathname = path .. "/" .. modname
	local files = {}
	for fname, ftype in dir.open(pathname) do
		if string.sub(fname, 1, 1) == "." then
		else
			if ftype == "file" then
				if string.format(fname, ".lua$") then
					table.insert(files, fname)
				end
			end
		end
	end

	if #files == 0 then
		return
	end
			
	load(pathname, files, modname, reload)
end

function load_mods(root_path, pathname, reload)
	local modname = pathname
	pathname = root_path .. "/" .. modname

	local filesets = {}
	get_fileset(pathname, filesets)

	for modname, v in pairs(filesets) do
		local files = v
		local path = pathname .. "/" .. modname
		if type(files) ~= "table" then
			files = {v}
			path = pathname .. "/"
		end
		load(path, files, modname, reload)
	end
end

return modules

