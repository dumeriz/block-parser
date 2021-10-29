**Note**
If all you want is a snapshot of utxo balances for a certain blockheight, consider switching to the snapshot-without-db branch.

# Blockparser

This is a tool to read the blockfiles from Zenon Network's *legacy* blockchain. In its current iteration, it is mainly used to generate a snapshot of the UTXO set at any given blockheight.

https://zenon.network/legacy/

I am not affilitated with Zenon Network, which is an independent entity, but you can find me in the official telegram group or in the community chat. Join us:

https://t.me/zenonnetwork
https://t.me/zenon_community

### Used libraries and external code
- I'm using cpp_redis and tacopie to communicate with a Redis server from C++ (https://github.com/Cylix/cpp_redis). That code is included as submodule.
- The zenon-folder contains copies from a subset of Zenons legacy blockchain code (https://github.com/zenonnetwork/zenon). This is used for the cryptographic primitives, which I didn't dare to reimplement.

### Background
During development, the intended use case changed several times. Originally, it was planned to support a graphical transaction space explorer (which should work perfectly well with a useful frontend implementation). I used it as an aggregator of rewards a node receives over time. It was used to fix a database corruption during the early zenina days (early aliens might remember). It was also used to check if an address with a certain prefix exists in the UTXO set (again, aliens will know. It turns out it doesn't exist btw.)

Currently however, it's shaped to be part of an option for the ongoing judge panel voting. We would need a snapshot of all UTXO balances for a certain blockheight, and possibly also find all senders to a few certain addresses.

Since this tool extracts all relevant data from the available blocks, down to transaction inputs and outputs, into an in-memory database, retrieval of these information is then easily possible and fast.

If you want to do anything else with this code, most ideas I've had over the time are still in there but commented out or simply unused. Most might still compile, some will require some work.
The more data you store in the database, the more RAM or swap space you will need, obviously.

### Requirements
First, the code is currently not optimized in any way. Storage in Redis is quite ressource intensive also. The whole process currenly requires around 8-10 GB of RAM; a fast CPU with several cores doesn't hurt but is not the important part. Get a temporary machine instance on Vultr or DigitalOcean or any other VPS provider. I've been using a system with 4 cores and 8 GB RAM; an additional 4 GB are set up as virtual memory. Physical RAM speeds things up of course, but in any case, a total of at least 10 GB should be available for the program.

## Usage
There are 4 steps.  If you follow these instructions, you will end up with a Redis instance populated with the data required to generate a balance snapshot for every available block. See section [Scripts](#Scripts) for further instructions on how to produce a simple text file with `UTXO:balance`-pairs.

1. Setup a build and runtime environment (ideally you rent a VPS for that) and compile the code.
2. The program reads from the available block data of a Zenon legacy node; you have to provide that.
3. Run the tool. That will populate a Redis database.
4. Use a redis client to extract the required data.

The 3 initial steps are only required once. Please follow the outlined procedure exactly.
 
### Setup and build
These are instructions to setup a fresh installation of Ubuntu 20.04.
I'm assuming you are logged in to your machine with root priviledges, on a terminal.

#### Setting up a swapfile
Paste the following commands line by line into your terminal. Lines beginning with # are comments which can be skipped.
```
# First, we need to configure the system's virtual memory.
# If you have more than 4GB of free physical memory, skip this step
fallocate -l 4G /swapfile # adjust the amount as necessary here
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
echo "/swapfile swap swap defaults 0 0" >> /etc/fstab 
swapon --show
```
The last command should print:
> NAME      TYPE SIZE USED PRIO<br>
> /swapfile file   4G   0B   -2

#### Building the code
First, we need to install the required build tools. The tool itself is build using meson (https://mesonbuild.com), but it depends on cpp_redis (https://github.com/cpp-redis/cpp_redis) which uses plain CMake.
```
apt install g++ meson ninja-build cmake pkg-config
```
We also require openssl, which, in Ubuntu 20.04, can be installed with libssl:
```
apt install libssl-dev
```
Next, clone this repository and, recursively, clone the included submodules (cpp_redis and tacopie).
```
git clone https://github.com/dumeriz/block-parser.git
cd block-parser
git submodule update --init --recursive
```
Now we have to build and install the dependencies:
```
cd cpp_redis && mkdir build && cd build
cmake ..
make -j4
make install
cd ../..
```
We can proceed to build the block-parser code. With meson, which is an out-of-source build tool (like CMake), we create different build folders depending on the required configuration. For example, the following produces a configuration for optimized build in the `rel` folder, but you could also configure for debug build (the tool will execute a lot faster optimized and require less ressources). See [here](https://mesonbuild.com/Running-Meson.html).
```
meson setup rel --buildtype release
# meson setup debug --buildtype debug
# meson setup mixed --buildtype debugoptimized
cd rel # or debug or mixed
ninja
```
This will build the code with some warnings, which should refer to code in the zenon-folder (this is code extracted from the legacy network chain).
Finally, we require installation and boot of the redis database server. In Ubuntu, installation implies startup; if it is not started automatically, open a second terminal and start `redis-server` manually.
```
apt install redis-server
# The following should show a status of 'active (running)'
systemctl status redis-server
```
Make sure to not change the default redis-server configuration; there is currently no option to change e.g. the port where redis can be reached implemented. You can change address and port manually in redis.h, if you have to.

### Getting the block data
It is highly recommended to *not* run this tool on a productive Zenon node; instead, load the required block data to your build machine. If you absolutely have to use a machine running a node, its best to copy all blocks to a folder znnd is not looking at. Theoretically, it should also be ok to read from .Zenon/blocks directly **however** make sure that znnd is absolutely not running then.
To get the block data to your "analysis" machine, on your node:
1. Stop znnd - you run a high risk of corrupted block data else.
2. Compress every block file from .Zenon/blocks into an archive:
```
BLOCKS=/path/to/.Zenon/blocks
cd $BLOCKS
tar caf blocks.tgz blk*
cd -
mv $BLOCKS/blocks.tgz .
```
4. Restart znnd.
3. Copy the archive to your VPS, or wherever you are doing this. I'm assuming a VPS with root access:
```
scp blocks.tgz root@your-ip:~
```
4. ssh into that machine and extract the blockfiles into a folder named blocks, e.g. /root/blocks:
```
ssh root@your-ip
mkdir blocks
mv blocks.tgz blocks
cd blocks && tar xzf blocks.tgz && cd ..
```
You'll then pass the directory `/root/` to the program.

### Running the code
Finally, we're ready to run. Assuming you are in the build folder and you installed the blockfiles into /root/blocks as described earlier:
```
./block-parser /root
```
Time for some fresh air, this will take a little while. On my test machine, 4 CPUs, 8 GB physical and 4 GB virtual RAM, parsing 16 blockfiles takes only a few minutes. Storing all data into Redis takes around 20-25 minutes. However, this is assuming an *optimized* build. Debug builds will drastically increase those numbers. Physical RAM is most important here, so that Redis is not required to swap so much.

#### Expected output
If you madelist something wrong with the path of the blockfiles, the program will fail fast with an unhandled exception.
Verify that the directory you pass contains a folder named blocks which contains the blockfiles blk0....dat
In normal cases, you'll be notified about every new blockfile that is being read:

> Reading blocks from /root/blocks/blk00000.dat

During read of the initial blockfile, additional output is generated once the first regular transaction (i.e. not one of the earliest POW-transactions) has been read, for the first POS transaction and the first POS transaction with additional inputs.

When the last blockfile has been read, the program prints
> Linked ... blocks from ... available<br>
> Removed ... blocks<br>
> REDIS_STATE: 1<br>
> REDIS_STATE: 3

The first line tells you how many blocks have been found, disregarding forked chain states. The second line explains how many blocks have been removed from all read blocks, due to being on forked chain states. At line 3, the process of storing into redis was started. No further output will be generated until the program quits.

#### Tips
* While the program is running, you can log into a different terminal and start the htop-program (exit with 'q'). You can monitor your system ressources here. E.g., you should see how the amount of memory used by the block-parser program increases constantly until the transfer to Redis starts; you'll then see it slowly drop while the memory usage of redis-server and its background-saving tool grows.
* You can also drop into redis-cli from a different terminal to monitor progress.
```
redis-cli
127.0.0.1:6379> monitor
# or
127.0.0.1:6379> get znn:blocks:top
```
The monitor command will print out every command that is sent from the program to the running redis-server. That will slow things down. Quit with Ctrl-C. You can also type the other command shown periodically, to check how far insertion proceeded. It will print the height of the most recently inserted block.
* If, during execution, you should run into ressource bottlenecks, the program might get killed. In that case, log into redis and flush the database before trying again, with more swapspace or less active background processes. Do this also if you have to stop the program during execution for whatever reason. We want a clean database.

```
redis-cli
127.0.0.1:6379> flushdb
OK
127.0.0.1:6379> exit
```

### Data extraction
Redis has clients in most major languages. In the cl-folder, you can find some functions in Common Lisp. You can also use redis-cli. What's needed is an idea of the existing keys. These are currently as follows:
- `znn:block:hash:<n>` contains the hash of the block at height <n>.
- `znn:block:txns:<n>` is the set of tx-hashes contained in the block at height <n>.
- `znn:block:top` contains the height of the last available block.
- `znn:tx:<hash>:n:<n>` contains the UTXO for index <n> of transaction <hash>.
- `znn:tx:<hash>:amount:<n>` contains the amount that was spent with UTXO <n> of transaction <hash>.
- `znn:utxos` is the set of all UTXOs that have been seen over all blocks.
- `znn:blocks:<key>` is the set of all block heights for which the balance of address <key> changed (in or out).
- `znn:change:<key>:<height>` contains the balance change for address <key> at blockheigt <height> (positive or negative total during that block).

For example, in redis-cli, get all blocks that changed the balance of the UTXO that received output 0 of the genesis transaction:

```
127.0.0.1:6379> get znn:tx:d6104e80f273648af61d496ad0e353d78317504adbb20720155164a5c23ff235:n:0
127.0.0.1:6379> smembers znn:blocks:ZYvKn3nggB3ZXZdpG7LfZge9GpT81fc4uj
1) "1"
2) "106"
```

It is advisable to **not** use the `keys` command. It will take a long time to finish. Refer to the available key set, as defined above.

#### Scripts
* There's a Common Lisp file with several useful functions in folder cl. Use that interactively. It works in SBCL if you have quicklisp installed.
* There's a Python script in folder python, which produces a file of the following format, where the lines are ordered according to `sorted(list-of-addresses)`.

> znn_address1:balance<br>
> znn_address2:balance<br>
> ...

This script expects a block height as argument. Printed balances correspond to that block.
```
python3 python/list-of-balances-at-block.py 12723
```
* There are some lua scripts in redis-scripts. These are intended to be stored in redis and executed via `evalsha`. Check out the beginning of the main function
  in src/main.cpp. There's a section commented which stores these scripts in redis. When used, a hash is printed which can then be used to trigger the corresponding script.
