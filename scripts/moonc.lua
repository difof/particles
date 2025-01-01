local moonparse = require("moonscript.parse")
local mooncompile = require("moonscript.compile")

return function(buffer)
    local tree, err = moonparse.string(buffer)
    if not tree then
        error(err)
    else
        local lua_code, err = mooncompile.tree(tree)
        if not lua_code then
            error(err)
        else
            return lua_code
        end
    end
end
