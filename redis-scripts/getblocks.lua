local getblocks = function(height, amount)
  local first = math.max(0, height - amount)
  local last = height + amount

  local blockdatatable = {}
  for i = first, last do
      local blockhash = redis.call("get", "block:height:" .. i)
      local blockdata = redis.call("lrange", "block:hash:" .. blockhash, 0, -1)
	local txs = redis.call("smembers", blockhash .. ":txs")
      blockdatatable[blockhash] = { merkle = blockdata[1],
				      height = blockdata[4],
				      time = blockdata[3],
				      txs = txs }
  end

  return blockdatatable
end


local result = {error = "wrong argument"}

if KEYS[1] == "height" then
  local height = tonumber(ARGV[1])
  local amount = 5

  if KEYS[2] == "amount" then amount = tonumber(ARGV[2]) end

  if amount ~= nil and height ~= nil then
    result = {blocks = getblocks(height, amount)};
  end
end

return cjson.encode(result)
