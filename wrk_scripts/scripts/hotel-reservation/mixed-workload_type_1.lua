local socket = require("socket")
math.randomseed(socket.gettime()*1000)
math.random(); math.random(); math.random()

-- change url here
local url = "http://localhost:50050"

local function get_user()
  local id = math.random(0, 500)
  local user_name = "Cornell_" .. tostring(id)
  local pass_word = "1111111111"  -- Changed to fixed password
  return user_name, pass_word
end

local function search_hotel() 
  local in_date = math.random(9, 23)
  local out_date = math.random(in_date + 1, 24)

  local in_date_str = tostring(in_date)
  if in_date <= 9 then
    in_date_str = "2023-12-0" .. in_date_str 
  else
    in_date_str = "2023-12-" .. in_date_str
  end

  local out_date_str = tostring(out_date)
  if out_date <= 9 then
    out_date_str = "2023-12-0" .. out_date_str 
  else
    out_date_str = "2023-12-" .. out_date_str
  end

  local lat = 37.7749 + (math.random(0, 481) - 240.5)/1000.0
  local lon = -122.4194 + (math.random(0, 325) - 157.0)/1000.0

  local method = "GET"
  local path = url .. "/search" -- ?customerName=John&inDate=2023-12-01&outDate=2023-12-02&latitude=37.7749&longitude=-122.4194&locale=en"
  local headers = {}
  -- headers["Content-Type"] = "application/json"
  -- local body = string.format(
  --   '{"customerName":"%s","inDate":"%s","outDate":"%s","latitude":%f,"longitude":%f,"locale":"en"}',
  --   "John", in_date_str, out_date_str, lat, lon
  -- )
  return wrk.format(method, path, headers, nil)
end

local function recommend()
  local coin = math.random()
  local req_param = ""
  if coin < 0.33 then
    req_param = "dis"
  elseif coin < 0.66 then
    req_param = "rate"
  else
    req_param = "price"
  end

  local lat = 38.0235 + (math.random(0, 481) - 240.5)/1000.0
  local lon = -122.095 + (math.random(0, 325) - 157.0)/1000.0

  local method = "POST"
  local path = url .. "/recommend"
  local headers = {}
  headers["Content-Type"] = "application/json"
  local body = string.format(
    '{"require":"%s","latitude":%f,"longitude":%f}',
    req_param, lat, lon
  )
  return wrk.format(method, path, headers, body)
end

local function reserve()
  local in_date = math.random(9, 23)
  local out_date = in_date + math.random(1, 5)

  local in_date_str = tostring(in_date)
  if in_date <= 9 then
    in_date_str = "2023-12-0" .. in_date_str 
  else
    in_date_str = "2023-12-" .. in_date_str
  end

  local out_date_str = tostring(out_date)
  if out_date <= 9 then
    out_date_str = "2023-12-0" .. out_date_str 
  else
    out_date_str = "2023-12-" .. out_date_str
  end

  local hotel_id = tostring(math.random(1, 80))
  local user_id, password = get_user()
  local cust_name = user_id

  local method = "POST"
  local path = url .. "/reservation"
  local headers = {}
  headers["Content-Type"] = "application/json"
  local body = string.format(
    '{"customerName":"%s","username":"%s","password":"%s","hotelId":"%s","inDate":"%s","outDate":"%s","roomNumber":1}',
    cust_name, user_id, password, hotel_id, in_date_str, out_date_str
  )
  return wrk.format(method, path, headers, body)
end

local function user_login()
  local user_name, password = get_user()
  local method = "POST"
  local path = url .. "/user"
  local headers = {}
  headers["Content-Type"] = "application/json"
  local body = string.format(
    '{"username":"%s","password":"%s"}',
    user_name, password
  )
  return wrk.format(method, path, headers, body)
end

request = function()
  cur_time = math.floor(socket.gettime())
  local search_ratio      = 1.0
  local recommend_ratio   = 0.0
  local user_ratio       = 0.0
  local reserve_ratio    = 0.0

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
