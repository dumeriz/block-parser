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

def get_balance_for_utxo_at_block(utxo, block):
    heights = sorted(int(height) for height in get_blocks_referencing_utxo(utxo))
    changes = [get_balance_change_for(utxo, height) for height in heights if height <= block]
    return sum(changes)

def gen_snapshot(blockheight, outputfile):
    all_utxos = get_all_utxos()
    utxos_sorted = sorted([utxo.decode("utf-8") for utxo in all_utxos])
    print(f"Producing snapshot for {len(utxos_sorted)} utxos")
    balances = [get_balance_for_utxo_at_block(utxo, blockheight) for utxo in utxos_sorted]

    with open(outputfile, "w") as f:
        for line in zip(utxos_sorted, balances):
            f.write(f"{line[0]}:{line[1]}\n")

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
