----------------------------------------------
-- Capture the build graph in symbolic form --
----------------------------------------------

local node = {}

local explore

explore = function(dep, target)
  for i, name in ipairs(dep) do
    local record = dep[i + 1]
    if type(record) == "table" then
      if not node[name] then
        node[name] = {record.err}
      end
      if record.err < node[name][1] then
        node[name][1] = record.err
      end
      if record.err == 0 or record.err == 2 then
        if target then node[name][target] = true end
        explore(record, name)
      else
        assert((record.err & 0xff) == 2, name .. ".error = " .. tostring(record.err))
      end
    end
  end
end

for i, logname in ipairs{...} do
  local f = assert(loadfile(logname), logname .. " failed")
  explore(f())
end


-----------------------
-- Enumerating nodes --
-----------------------

local dict = {}

for name, staff in pairs(node) do
  assert(staff[1] == 0, name .. " is busy")
  staff[1] = nil
  dict[#dict + 1] = name
  dict[name] = #dict
end


---------------------------------------
-- Store dependences in numeric form --
---------------------------------------

local status = {}
local children = {}
local child = {}

for i, name in ipairs(dict) do
  status[i] = 0
  children[i] = #child
  for k, v in pairs(node[name]) do
    child[#child + 1] = dict[k]
  end
end

children[#dict + 1] = #child


for i, j in ipairs(child) do
  status[j] = status[j] + 1
end


------------------------------------
-- Making the node names relative --
------------------------------------

local map_dir = os.getenv("MAP_DIR")

if map_dir then
  map_dir = map_dir .. "/"

  local find_slashes = function(s)
    local t = {}
    for i in s:gmatch("()/") do
      t[#t + 1] = i
    end
    return t
  end

  local map_dir_slash = find_slashes(map_dir)

  for i, name in ipairs(dict) do
    local name_slash = find_slashes(name)

    local eq_cnt, eq_pos

    for i, map_pos in ipairs(map_dir_slash) do
      if map_pos ~= name_slash[i] then break end
      if map_dir:sub(1, map_pos) ~= name:sub(1, name_slash[i]) then break end
      eq_cnt, eq_pos = i, map_pos
    end

    dict[i] = ("../"):rep(#map_dir_slash - eq_cnt) .. name:sub(eq_pos + 1, -1)
  end
end


-------------------------
-- Writing the roadmap --
-------------------------

io.write(("<char *>"):rep(#dict)) -- placeholders for name pointers

io.write("\n")

for i, name in ipairs(dict) do
  io.write(" ", ("%3d"):format(status[i]))
end

io.write("\n")

for i, offset in ipairs(children) do
  io.write(" ", ("%3d"):format(offset))
end

io.write("\n")

for i, j in ipairs(child) do
  io.write(" ", ("%3d"):format(j - 1))
end

for i, name in ipairs(dict) do
  io.write("\n", name)
end


