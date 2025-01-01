function print_table(t, indent, visited)
    indent = indent or ""
    visited = visited or {}

    for k, v in pairs(t) do
        if type(v) == "table" and not visited[v] then
            visited[v] = true
            print(indent .. tostring(k) .. ":")
            print_table(v, indent .. "  ", visited)
        else
            print(indent .. tostring(k) .. ": " .. tostring(v))
        end
    end

    local mt = getmetatable(t)
    if mt and not visited[mt] then
        visited[mt] = true
        print(indent .. "metatable:")
        print_table(mt, indent .. "  ", visited)
    end
end

return {
    print_table = print_table
}