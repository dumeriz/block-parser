# Blockparser

This is a tool to read the blockfiles from Zenon Network's *legacy* blockchain. In its current iteration, it is mainly used to generate a snapshot of the UTXO set at any given blockheight.

https://zenon.network/legacy/

I am not affilitated with Zenon Network, which is an independent entity, but you can find me in the official telegram group or in the community chat. Join us:

https://t.me/zenonnetwork
https://t.me/zenon_community

### Used libraries and external code
- The zenon-folder contains copies from a subset of Zenons legacy blockchain code (https://github.com/zenonnetwork/zenon). This is used for the cryptographic primitives, which I didn't dare to reimplement.

### Background
For more info, see the readme in the main branch. This is a branch with reduced functionality. It can be used to produce a snapshot of address-balance pairs for any given block available from the provided block data. It does not populate a database or require an installation of redis.

### Requirements
This version is not as ressource hungry as the main branch. A fast CPU and 8 GB of RAM should suffice; less physical with some virtual RAM should work too.

## Usage
There are 3 steps.  If you follow these instructions, you will end up with a simple text file with `UTXO:balance`-pairs.

1. Setup a build and runtime environment and compile the code.
2. The program reads from the available block data of a Zenon legacy node; you have to provide that.
3. Run the tool. That will create the snapshot in a text file.

### Setup and build
These are instructions to setup a fresh installation of Ubuntu 20.04.
I'm assuming you are logged in to your machine with root priviledges, on a terminal.

#### Building the code
First, we need to install the required build tools. The tool itself is build using meson (https://mesonbuild.com).
```
apt install g++ meson ninja-build cmake pkg-config
```
We also require openssl, which, in Ubuntu 20.04, can be installed with libssl:
```
apt install libssl-dev
```
Next, clone this repository and checkout this branch.
```
git clone https://github.com/dumeriz/block-parser.git
cd block-parser
git checkout snapshot-without-db
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
Finally, we're ready to run. Assuming you are in the build folder, you installed the blockfiles into /root/blocks as described earlier, and you are interested in a snapshot for block 1331331:
```
./block-parser /root 1331331
```
This will take something between 5 to 15 minutes, depending on your machine. Probably not much longer.
owever, this is assuming an *optimized* build. Debug builds will increase that number.

#### Expected output
If you specified correct arguments, you'll be notified about every new blockfile that is being read:

> Reading blocks from /root/blocks/blk00000.dat

During read of the initial blockfile, additional output is generated once the first regular transaction (i.e. not one of the earliest POW-transactions) has been read, for the first POS transaction and the first POS transaction with additional inputs.

When the last blockfile has been read, the program prints
> Linked ... blocks from ... available<br>
> Removed ... blocks<br>
> Creating snapshot from ... blocks.

The first line tells you how many blocks have been found, disregarding forked chain states. The second line explains how many blocks have been removed from all read blocks, due to being on forked chain states. At line 3, the process of snapshot calculation is started. No further output will be generated until the program quits. You should then find a file named `snapshot-1331331.txt` in your current directory, containing a list of all relevant addresses associated with their balance at block 1331331. This file is sorted by address.

The content of this file should be exactly the same as generated by the python script `list-of-balances-at-block.py` from the main branch.
