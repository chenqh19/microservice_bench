-- use it with:
-- taskset -c 32-63 ../wrk2/wrk -t 100 -c 100 -D exp -d30s -L -s wrk_compress.lua http://localhost:50060 -R 1600

local socket = require("socket")
math.randomseed(socket.gettime() * 1000)
math.random(); math.random(); math.random()

-- wrk will be invoked with the base URL, e.g.:
-- wrk -t2 -c64 -d30s -s compression/wrk_compress.lua http://localhost:50060

-- If REQUEST_SIZE (bytes) is set, use it for every request; else random 400-401KB
local fixed_size = os.getenv("REQUEST_SIZE") and tonumber(os.getenv("REQUEST_SIZE"))

local function compress_request()
  local method = "GET"
  local size = fixed_size or math.random(400 * 1024, 401 * 1024)
  local path = "/compress?size=" .. tostring(size)
  return wrk.format(method, path, nil, nil)
end

request = function()
  return compress_request()
end


