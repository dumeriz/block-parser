local gettxins = function(hash)
  local pubkeys = redis.call("smembers", hash .. ":input:from");
  return pubkeys
end

local result = {error = "wrong argument"}

if KEYS[1] == "hash" then
  local hash = ARGV[1]

  if hash ~= nil and hash ~= "" then
    result = {pubkeys = gettxins(hash)};
  end
end

return cjson.encode(result)
