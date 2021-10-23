local gettxconsuming = function(pk)
  local txs = redis.call("smembers", pk .. ":consumed:tx");
  return txs
end

local result = {error = "wrong argument"}

if KEYS[1] == "pk" then
  local pk = ARGV[1]

  if pk ~= nil and pk ~= "" then
    result = {txs = gettxconsuming(pk)};
  end
end

return cjson.encode(result)
