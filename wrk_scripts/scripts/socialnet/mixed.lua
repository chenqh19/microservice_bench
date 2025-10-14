local json = require("cjson")
local socket = require("socket")
math.randomseed(socket.gettime()*1000); math.random(); math.random(); math.random()

local function compose()
  local usernames = {"alice","bob","carol","dave","erin","frank","grace"}
  local u = usernames[math.random(#usernames)]
  local body = json.encode({ username = u, text = "hi" })
  local headers = { ["Content-Type"] = "application/json" }
  return wrk.format("POST", "/compose", headers, body)
end

local function follow()
  local uid = math.random(1, 1000)
  local tid = math.random(1, 1000)
  while tid == uid do tid = math.random(1, 1000) end
  local body = json.encode({ userId = uid, targetUserId = tid, action = "follow" })
  local headers = { ["Content-Type"] = "application/json" }
  return wrk.format("POST", "/follow", headers, body)
end

local function user_timeline()
  local uid = math.random(1, 1000)
  return wrk.format("GET", string.format("/user_timeline?userId=%d&start=0&limit=10", uid))
end

local function home_timeline()
  local uid = math.random(1, 1000)
  return wrk.format("GET", string.format("/home_timeline?userId=%d&start=0&limit=10", uid))
end

request = function()
  local coin = math.random()
  if coin < 0.4 then
    return compose()
  elseif coin < 0.6 then
    return follow()
  elseif coin < 0.8 then
    return user_timeline()
  else
    return home_timeline()
  end
end


