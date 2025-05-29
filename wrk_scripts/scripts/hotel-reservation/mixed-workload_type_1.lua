local socket = require("socket")
math.randomseed(socket.gettime()*1000)
math.random(); math.random(); math.random()

-- URL will be passed as a parameter to wrk2
local url = ""

local function get_user()
  local id = math.random(0, 500)
  local user_name = "Cornell_" .. tostring(id)
  local pass_word = "1111111111"  -- Changed to fixed password
  return user_name, pass_word
end

local function search_hotel() 
  local method = "GET"
  local path = "/search?customerName=John&inDate=2023-12-01&outDate=2023-12-02&latitude=37.7749&longitude=-122.4194&locale=en"
  local headers = {}
  return wrk.format(method, path, headers, nil)
end

local function recommend()
  local method = "GET"
  local path = "/recommend?latitude=37.7749&longitude=-122.4194&require=near_city&locale=en"
  local headers = {}
  return wrk.format(method, path, headers, nil)
end

local function reserve()
  local method = "GET"
  local path = "/reservation?customerName=John&hotelId=1&inDate=2023-12-01&outDate=2023-12-02&roomNumber=1&username=Cornell_1&password=1111111111"
  local headers = {}
  return wrk.format(method, path, headers, nil)
end

local function user_login()
  local method = "GET"
  local path = "/user?username=Cornell_1&password=1111111111"
  local headers = {}
  return wrk.format(method, path, headers, nil)
end

request = function()
  cur_time = math.floor(socket.gettime())
  local search_ratio      = 0.4  -- 40% for search
  local recommend_ratio   = 0.3  -- 30% for recommend
  local user_ratio       = 0.15 -- 15% for user
  local reserve_ratio    = 0.15 -- 15% for reservation

  local coin = math.random()
  if coin < search_ratio then
    return search_hotel()
  elseif coin < search_ratio + recommend_ratio then
    return recommend()
  elseif coin < search_ratio + recommend_ratio + user_ratio then
    return user_login()
  else 
    return reserve()
  end
end
