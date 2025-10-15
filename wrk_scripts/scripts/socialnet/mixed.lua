local socket = require("socket")
math.randomseed(socket.gettime()*1000); math.random(); math.random(); math.random()

local function compose()
  local usernames = {"seeduser1", "seeduser2", "seeduser3", "seeduser4", "seeduser5", "seeduser6", "seeduser7"}
  local u = usernames[math.random(#usernames)]
  return wrk.format("GET", string.format("/compose?username=%s&text=%s", u, "hi"))
end

local function follow()
  local uid = math.random(1, 1000)
  local tid = math.random(1, 1000)
  while tid == uid do tid = math.random(1, 1000) end
  return wrk.format("GET", string.format("/follow?userId=%d&targetUserId=%d&action=follow", uid, tid))
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
  if coin < 0.05 then
    return user_timeline()
  elseif coin < 0.95 then
    return compose()
  else
    return home_timeline()
  end
end


