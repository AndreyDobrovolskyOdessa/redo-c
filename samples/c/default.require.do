#!/usr/bin/env lua


--------- Editable ---------

local Compiler = "gcc"

----------------------------


local BName = arg[1]:gsub("%.require$", "") -- = arg[2]

local CName = BName .. ".c"
local OName = BName .. ".o"

local TDir, TName = BName:match("(.*/)(.*)")

if not TDir then
  TDir = ""
  TName = BName
end

local FName = TDir .. "local.cflags"

assert(os.execute("redo-ifchange " .. FName))

local f = assert(io.open(FName))
local Cflags = assert(f:read())
local Deps = assert(f:read())
f:close()


local INames = {}
local RNames = {}
local RNamesShort = {}


local DName = BName .. ".d"

assert(os.execute(Compiler .. " -MD " .. Cflags .. " -o " .. OName .. " -c " .. CName))

local f = assert(io.open(DName))

for l in f:lines() do
  for w in l:gmatch("[^%s]+") do
    local First = w:sub(1,1)
    local Last = w:sub(-1)
    if First ~= "/" and (not ([[:c\]]):find(Last)) then
      table.insert(INames, w)
      if w:match(".*%.h") and w ~= BName .. ".h" then
        local wb = w:gsub("%.h$", "")
        local c = wb .. ".c"
        if os.execute("test -e " .. c) then
          local r = wb .. ".require"
          table.insert(RNames, r)
          table.insert(RNamesShort, (r:gsub(TDir, "", 1)))
        end
      end
    end
  end
end

f:close()

os.execute("rm " .. DName)

local IList = table.concat(INames, " ")
local RList = table.concat(RNames, " ")
local AllDeps = table.concat({CName, OName, IList, RList}, " ")

assert(os.execute("redo-ifchange " .. AllDeps))


f = io.open(arg[3], "w")

f:write(TName, ".o\n")

for d in Deps:gmatch("[^%s]+") do
  f:write(d, "\n")
end


for i, rn in ipairs(RNames) do
  local RDir = RNamesShort[i]:match(".*/") or ""
  local fr = io.open(rn)
  if fr then
    for r in fr:lines() do
      if r:match("%.o$") then
        f:write(RDir)
      end
      f:write(r, "\n")
    end
    fr:close()
  end
end

f:close()


