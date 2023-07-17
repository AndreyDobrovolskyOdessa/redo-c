local targets = math.floor(tonumber(arg[1]) or 30)
local sources = math.floor(tonumber(arg[2]) or 10)
local maxdeps = math.floor(tonumber(arg[3]) or 5)
local window  = math.floor(tonumber(arg[4]) or sources + maxdeps)

if maxdeps < 1 then maxdeps = 1 end
if sources < maxdeps then sources = maxdeps end
if targets < sources then targets = sources end
if window < sources then window = sources end

local parents = {}
local children = {}

for i = 1, targets do
  parents[i] = {}
  children[i] = {}
end

for i = sources + 1, targets do
  local deps = math.random(maxdeps)
  local candidate = {}
  for j = math.max(1, i - window), i - 1 do
    candidate[#candidate + 1] = j
  end
  local p = parents[i]
  for j = 1, deps do
    local new_candidate = math.random(#candidate)
    local new_parent = candidate[new_candidate]
    local c = children[new_parent]
    c[#c + 1] = i
    p[#p + 1] = candidate[new_candidate]
    table.remove(candidate, new_candidate)
  end
end

local text

local append = function(s)
  text[#text + 1] = s
end

for i = sources + 1, targets do
  text = {}
  append("DEPS='")
  for j, k in ipairs(parents[i]) do
    append(" t") append(tostring(k))
    if k <= sources then
      append(".src")
    end
  end
  append("'\n")
  append("DELAY=") append(tostring(math.random())) append("\n")
  append(". ./reciept\n")

  local f = assert(io.open("t" .. tostring(i) .. ".do", "w"))
  assert(f:write(table.concat(text)))
  f:close()
end


text = {}
append("DEPS='")
for i, ch in ipairs(children) do
  if #ch == 0 then
    append(" t")  append(tostring(i))
    if i <= sources then
      append(".src")
    end
  end
end
append("'\n")
append("DELAY=0\n")
append(". ./reciept\n")

local f = assert(io.open("t.do", "w"))
assert(f:write(table.concat(text)))
f:close()

