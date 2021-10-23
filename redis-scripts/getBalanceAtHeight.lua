local getutxo = function(key)
  local prefix = "znn:blocks:"
  return string.sub(key, string.len(prefix), -1)
end

local convertkeys = function(keys)
  local converted = {}
  for i, key in ipairs(keys) do
    converted[i] = getutxo(key)
  end
  return converted
end

return convertkeys(redis.call("smembers", "znn:utxos:*"))
