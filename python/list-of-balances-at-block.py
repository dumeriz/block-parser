import sys
import redis

red = redis.Redis()

def get_last_block():
    return int(red.get("znn:blocks:top"))

def get_all_utxos():
    return red.smembers("znn:utxos")

def get_blocks_referencing_utxo(utxo):
    return red.smembers(f"znn:blocks:{utxo}")

def get_balance_change_for(utxo, height):
    return int(red.get(f"znn:change:{utxo}:{height}")) # all numbers are in given in zats (no decimals)

# Returns the sum of balance changes for a given utxo until a given block.
# Returns -1 if the utxo had no transactions until that block.
def get_balance_for_utxo_at_block(utxo, block):
    heights = sorted(int(height) for height in get_blocks_referencing_utxo(utxo))
    if heights[0] > block:
        return -1
    changes = [get_balance_change_for(utxo, height) for height in heights if height <= block]
    return sum(changes)

def gen_snapshot(blockheight, outputfile):
    all_utxos = get_all_utxos()
    print(f"Looking at {len(all_utxos)} candidate addresses...")

    utxos_sorted = sorted([utxo.decode("utf-8") for utxo in all_utxos])
    balances = [get_balance_for_utxo_at_block(utxo, blockheight) for utxo in utxos_sorted]

    # associate utxos with balances while removing those that did not have a transaction
    # until blockheight
    utxo_balance_pairs = [(utxo, balance) for utxo, balance in zip(utxos_sorted, balances)
                          if balance > -1]

    print(f"Producing snapshot for {len(utxo_balance_pairs)} relevant addresses.")

    with open(outputfile, "w") as f:
        for pair in utxo_balance_pairs:
            f.write(f"{pair[0]}:{pair[1]}\n")

if __name__ == "__main__":
    try:
        blockheight = int(sys.argv[1])
    except:
        print("Please provide a blockheight as input")
        exit(0)

    available = get_last_block()
    if blockheight > available:
        print(f"Highest block seen is {available}")
        exit(0)

    gen_snapshot(blockheight, "snapshot.txt")
