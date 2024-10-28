----------------------------------------------
-- Capture the build graph in symbolic form --
----------------------------------------------

local ERRORS = 0xff
local BUSY = 75
local IMMEDIATE_DEPENDENCY = 1024

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
      if record.err == 0 or record.err == BUSY then
        if target then node[name][target] = true end
        explore(record, name)
      else
        assert((record.err & ERRORS) == BUSY, name .. ".error = " .. tostring(record.err))
      end
    end
  end
end

for i, logname in ipairs{...} do
  local f = assert(loadfile(logname), logname .. " failed")
  explore(f())
end


----------------
-- Find locks --
----------------

local lock = {}

for name, staff in pairs(node) do
  if staff[1] == (IMMEDIATE_DEPENDENCY + BUSY) then
    local TDir, TName = name:match("(.-)([^/]*)$")
    lock[#lock + 1] = TDir .. ".do...do.." .. TName
  end
end

if #lock > 0 then
  print(table.concat(lock, "\n"))
  os.exit(1)
end

