local targets = math.floor(tonumber(arg[1]) or 30)
local coef = tonumber(arg[2]) or 3.0

local targets_min = 10
local coef_min = 2.0
local coef_max = 10.0

if targets < targets_min then targets = targets_min end
if coef < coef_min then coef = coef_min end
if coef > coef_max then coef = coef_max end

-------------------
-- Randomization --
-------------------

local f = assert(io.open("/dev/urandom"))

local Seed = function(len)
  return tonumber((("%02x"):rep(len)):format((f:read(len)):byte(1, -1)), 16)
end

math.randomseed(Seed(8), Seed(8))

io.close(f)


local nodes = math.floor(targets * coef)

local node = {}

for i = 1, nodes do
  node[i] = {}
end

for i = nodes, 2, -1 do
  local high = math.floor((i - 1) / coef) + 1
  local low  = math.floor((high - 1) / coef) + 1

  if high < targets then high = targets end
  if high >= i then high = i - 1 end

  local parent = math.random(low, high)
  local t = node[parent]
  t[#t + 1] = i
end



local text

local append = function(s)
  text[#text + 1] = s
end

for i, n in ipairs(node) do
  if #n > 0 then
    text = {}
    append("DEPS='")
    for j, k in ipairs(n) do
      append(" t") append(tostring(k))
      if #node[k] == 0 then
        append(".src")
      end
    end
    append("'\n")
    append("DELAY=") append(tostring(math.random())) append("\n")
    append(". ./recipe\n")

    local f = assert(io.open("t" .. tostring(i) .. ".do", "w"))
    assert(f:write(table.concat(text)))
    f:close()
  end
end


text = {}
append("DEPS=t1\n")
append("DELAY=0\n")
append(". ./recipe\n")

local f = assert(io.open("t.do", "w"))
assert(f:write(table.concat(text)))
f:close()

