premake.modules.install = {}
local m = premake.modules.install

local p = premake

local PREFIX = "./install"

local HEADERS_TO_EXPORT = {
  "./opfor/src/opfor/**.hpp",
  "./opfor/src/components/**.hpp",
}

newaction {
	trigger = "install",
	description = "Install script",

	onStart = function()
		print("Starting installation in " .. PREFIX)
	end,

  onProject = function(prj)
    print(prj.location)

	end,

	execute = function()
    os.mkdir(PREFIX)
    
    local LIBDIR = PREFIX .. "/lib"
    os.mkdir(LIBDIR)
    local files = os.matchfiles("./bin/**.[a,so,dll]")
    for _, v in pairs(files) do
      os.copyfile(v, LIBDIR)
    end

    local INCDIR = PREFIX .. "/include"
    os.mkdir(INCDIR)
    for _, dir in pairs(HEADERS_TO_EXPORT) do

      local files = os.matchfiles(dir)

      for _, v in pairs(files) do
        local basename = v:match("^.+/(.+)$")
        local target_path = INCDIR .. '/' .. v:match("^opfor/src/(.+)/.+$")
        os.mkdir(target_path)
        os.copyfile(v, target_path .. '/' .. basename)
      end
    end
    
    local files = os.matchfiles("./opfor/include/*.hpp")
    for _, v in pairs(files) do
        local basename = v:match("^.+/(.+)$")
        os.copyfile(v, INCDIR .. '/' .. basename)
    end


	end,

	onEnd = function()
		print("Installation complete")
	end
}

return m
