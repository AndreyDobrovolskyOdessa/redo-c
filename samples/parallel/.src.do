#!/usr/bin/env lua

if os.execute("test -f " .. arg[1]) then
  assert(os.rename(arg[1], arg[3]))
else
  io.open(arg[3], "w"):write(tostring(io.open("/dev/urandom"):read(1):byte()), "\n")
end

