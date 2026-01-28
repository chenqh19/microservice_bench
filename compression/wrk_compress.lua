-- use it with:
-- taskset -c 32-63 ../wrk2/wrk -t 50 -c 50 -d30s -L -s wrk_compress.lua http://localhost:50060 -R 10000

local socket = require("socket")
math.randomseed(socket.gettime() * 1000)
math.random(); math.random(); math.random()

-- wrk will be invoked with the base URL, e.g.:
-- wrk -t2 -c64 -d30s -s compression/wrk_compress.lua http://localhost:50060

local function compress_request()
  local method = "GET"
  -- Random size between 64KB and 512KB (bytes)
  local size = math.random(600 * 1024, 2000 * 1024)
  local path = "/compress?size=" .. tostring(size)
  return wrk.format(method, path, nil, nil)
end

request = function()
  return compress_request()
end


