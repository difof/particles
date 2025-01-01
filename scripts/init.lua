local moonc = require('moonc')

local function load(modulename)
    local errMsg = ''
    local modulepath = string.gsub(modulename, "%.", "/")

    for path in string.gmatch(package.path, "([^;]+)") do
        local filename = string.gsub(path, "%?", modulepath)
        local file = io.open(filename, "rb")
        if file then
            local source = file:read("*a")
            file:close()
            if string.sub(filename, -5) == ".moon" then
                local lua = moonc(source)
                return assert(loadstring(lua, filename))
            end
        end
    end
end

-- Install the loader so that it's called just before the normal Lua loader
table.insert(package.loaders, 2, load)