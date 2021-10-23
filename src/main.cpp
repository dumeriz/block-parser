#include "redis.hpp"
#include "types.hpp"

#include <chrono>
#include <datfile.hpp>
#include <thread>
#include <util.hpp>

// In the regular zenon / pivx / bitcoin code, the blocks are deserialized in main.cpp:LoadExternalBlockfile.
// The deserialization logic is implemented in the CBlock / CTransaction / CTxIn etc. classes by expansion
// of the ADD_SERIALIZE_METHODS macro, which injects the Serialize and Unserialize methods into the class body.
// Both call into the type specific SerializationOp, with a type tag to differentiate between de- and serialization.
// In LoadExternalBlockfile, the block deserialization is triggered by application of the stream operator >> to
// a CBufferedFile (wrapping a C-FILE) and a zero initialized CBlock instance.
// CBufferedFile is parameterized by a type parameter and a version parameter.  The former is, in our case, set
// to SER_DISK (defined in serialize.h), the latter to CLIENT_VERSION, defined in clientversion.h.
// Both parameters are forwarded to the actual Unserialize call in the overloaded >> operator (in CBufferedFile).
// From the different Unserialize overloads in serialize.h, the only match for a parameter of type CBlock is
// 'void Unserialize(Stream& is, T& a, long nType, int nVersion)', which simply calls into a.Unserialize, delegating
// to CBlock::SerializeOp. This then calls READWRITE for the block header, for the txns, and, conditionally, for the
// block signature (if non-coinbase txns exist and the txns were validate by POS.)
// READWRITE is a simple delegating macro, calling the respective Unserialize method of the wrapped argument.
// About the version and type parameter:
// Upon the Unserialization of a transaction, the received parameter is replaced by the deserialized version value,
// which is then forwarded to the deserialization process for the remaining transaction attributes.
// These are the types CTxIn, CTxOut, CScript, COutPoint, vector<T>, or int types. Neither of these is sensible to
// the actual value of these parameters.
// Apparently, only during the block header deserialization, the version field is relevant, but this is NOT the
// clientversion:
// - the accumulator_checkpoint is read, if version is > 3,
// - the block hash is calculated differently, when version is > 4.
// The final step in the deserialization of a Transaction is the reconstruction of the TX hash (UpdateHash.)
// This is implemented in hash.h, and depends on a (in our case, fixed to SER_GETHASH) parameter nType, and a parameter
// nVersion, which is also fixed, to PROTOCOL_VERSION, defined in version.h.

using BlockPtr = std::shared_ptr<blockparser::Block>;
using BlockMap = std::unordered_map<uint256, BlockPtr, blockparser::detail::uint256_hash>;
using TxMap    = blockparser::TxMap;

/*
void store_block(BlockMap const& blocks, TxMap const& txs, uint256 const& hash)
{
    auto const& block{blocks.at(hash)};
    //std::cout << "Storing block " << block->height() << " with " << block->transactions().size() << " txns" <<
std::endl;
    //redis::store_block(block, hash);

    for (auto&& tx : block->transactions())
    {
        // for all utxo's: increase the 'balance' of the associated pubkey
        for (auto&& utxo : tx.vout)
        {
            if (!utxo.address.empty())
            {
                redis::modify_balance(utxo.address, utxo.amount);
            }
        }

        // for all utxi's: increase the 'balance' of the associated pubkey and store various linkages
        for (auto&& txi : tx.vin)
        {
            if (claims_output(txi))  // not the coinbase
            {
                auto referenced_tx{txs.find(txi.tx_hash)};
                if (referenced_tx == txs.end())
                {
                    std::cerr << "Non-referencing txi in block " << hash.ToString() << ": "
                              << txi.tx_hash << std::endl;
                    continue;
                }

                auto referenced_block{blocks.find(referenced_tx->second)};
                if (referenced_block == blocks.end())
                {
                    std::cerr << "Missing block referenced in tx: " << hash.ToString() << std::endl;
                    continue;
                }

                bool found{};
                for (auto&& block_tx : referenced_block->second->transactions())
                {
                    if (block_tx.hash == txi.tx_hash)
                    {
                        found = true;
                        auto const& vout{block_tx.vout[txi.index]};

                        redis::modify_balance(vout.address, -vout.amount);
                        redis::link_vout_to_block(vout.address, block_tx.hash.ToString());
                        redis::link_vout_to_tx(vout.address, tx.hash.ToString());
                    }
                }
                if (!found)
                {
                    std::cerr << "Could not associate txi in block " << hash.ToString() << std::endl;
                }
            }
        }
    }
}
*/

std::string str_tolower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); } // correct
    );
    return s;
}

// naive way to enumerate the available blockdata files in directory where:
// try opening sequentially until that fails.
auto enumerate_blockfiles(std::string where)
{
    std::ifstream file{where + "/blk00000.dat"};
    size_t i{};
    for (; file.good(); i++)
    {
        auto const num{std::string{"000"} + ((i + 1) < 10 ? "0" : "") + std::to_string(i + 1)};
        auto const filename{where + "/blk" + num + ".dat"};
        file = std::ifstream{filename};
    }

    assert(i > 0); // else not even blk00000.dat was readable
    return i - 1;
}

int main(int argc, char** argv)
{
    // Not all of these scripts might work with the current iteration of the code.
    // redis::load_script("redis-scripts/getallutxos.lua");
    // redis::load_script("redis-scripts/getutxovalue.lua");
    // redis::load_script("redis-scripts/getblocks.lua");
    // redis::load_script("redis-scripts/gettxinputs.lua");
    // redis::load_script("redis-scripts/gettxsconsumingpk.lua");
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    // return 0;

    if (argc < 2)
    {
        std::cout << "Please pass the absolute path to the directory containing the 'blocks' folder" << std::endl;
        return -1;
    }

    auto const blocksdir{std::string{argv[1]}};

    if (std::ifstream{blocksdir + "/Zenon.conf"}.is_open())
    {
        std::cout << "It seems you're reading directly from Zenons config directory - are you sure? ('y' to proceed)."
                  << std::endl;
        std::string input;
        std::getline(std::cin, input);
        if (input != "y" && input != "Y")
        {
            return 0;
        }
        else
        {
            std::cout << "If Zenon is still running - now is the time to press Ctrl-c and stop it first..."
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
            std::cout << "Nevermind." << std::endl << std::endl;
        }
    }

    std::vector<std::unique_ptr<blockparser::Datfile>> datfiles;
    BlockMap blocks;
    blockparser::Block* genesis{nullptr};    // first block
    blockparser::Block* last_block{nullptr}; // for reverse iteration to build the linked list

    for (size_t i{}; i <= enumerate_blockfiles(blocksdir + "/blocks"); ++i)
    {
        try
        {
            auto num = (i < 10 ? "0" : "") + std::to_string(i);

            blockparser::Datfile datfile{blocksdir + "/blocks/blk000" + num + ".dat"};

            if (i == 0)
            {
                genesis    = datfile.blocks().front().get();
                last_block = genesis;
            }

            for (auto&& block : datfile.blocks())
            {
                blocks[blockparser::hash(block->header())] = block;
            }

            last_block = datfile.blocks().back().get();
        }

        catch (blockparser::ParseException const& pe)
        {
            std::cout << __func__ << ": " << pe.what() << " at block " << datfiles.back()->blocks().size()
                      << " of file " << i << std::endl;
        }
    }

    assert(genesis && last_block);

    // By reverse iterating and forward linking all blocks, we get rid of blocks from forked chains.
    size_t count{0};
    for (auto* block{last_block}; block != genesis; count++)
    {
        auto const block_hash{blockparser::hash(block->header())};
        assert(blocks.at(block_hash).get() == block);

        auto const& prev_hash{block->header().hash_previous_block_};
        blocks.at(prev_hash)->set_follower(block_hash);
        block = blocks[prev_hash].get();
    }

    std::cout << "Linked " << (count + 1) << " blocks from " << blocks.size() << " available." << std::endl;
    assert(last_block->follower().IsNull());

    // set the actual block height now
    last_block->set_height(count + 1);

    size_t height{1};
    for (auto* block{genesis}; !block->follower().IsNull(); block = blocks[block->follower()].get())
    {
        block->set_height(height++);
        if (block->follower().IsNull()) assert(block == last_block);
    }

    assert(last_block->height() == height);
    assert(blocks[last_block->header().hash_previous_block_]->height() == last_block->height() - 1);

    // Free some space by removing blocks from forked chains.
    size_t removed{};
    for (auto it{blocks.begin()}; it != blocks.end();)
    {
        auto const& previous_hash{it->second->header().hash_previous_block_};
        assert(!previous_hash.IsNull() || it->second.get() == genesis);

        auto previous_is_deleted{blocks.find(previous_hash) == std::end(blocks)};

        if (it->second.get() != genesis && (previous_is_deleted || blocks[previous_hash]->follower() != it->first))
        {
            // std::cout << "Erasing block " << it->second->height() << ": " << blockparser::hash(it->second->header())
            // << std::endl;
            it = blocks.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }

    std::cout << "Removed " << removed << " blocks." << std::endl;
    assert(last_block->follower().IsNull());

    // validation: Iterating through the chain now by following the follewer-links, we should have
    // a height-sequence of increments by one. Initial height must be 0, top height must be height-1 (see above).
    size_t control_height{1};
    for (auto hash{blockparser::hash(genesis->header())}; !hash.IsNull(); hash = blocks[hash]->follower())
    {
        auto actual_height{blocks[hash]->height()};
        if (actual_height != control_height)
        {
            std::cout << "Expected height " << control_height << ", found " << actual_height << std::endl;
            assert(false);
        }
        control_height++;
    }

    std::cout << "Storing blockmap of size " << blocks.size() << " in database" << std::endl;

    try
    {
        for (auto hash{blockparser::hash(genesis->header())}; !hash.IsNull();)
        {
            auto block_ptr{blocks[hash]};
            redis::store_block(block_ptr, hash.ToString());
            // Free space when stored in db
            blocks.erase(hash);
            hash = block_ptr->follower();
        }
    }

    catch (blockparser::RedisException const& re)
    {
        std::cout << __func__ << ": " << re.what() << std::endl;
    }
}

/*

auto& datfile{datfiles.back()};
auto const hashmap{datfile->blockhashtable()};
auto genesis{std::find_if(hashmap.cbegin(), hashmap.cend(), [](auto const& entry) {
    return !entry.second->header().hash_previous_block_;
})};

if (genesis != hashmap.end())
{
    std::cout << std::endl
              << "Genesis block: " << genesis->first << ": " << *genesis->second << std::endl;
}

for (auto&& block : datfile->blocks())
{
    store_block(datfile->blockhashtable(), datfile->tx2blockhashmap(),
                blockparser::hash(block->header()));
    redis::commit();
}

auto const addr{"put-some-znn-address-here"};
blockparser::PubkeyPool::on_key(addr, [&](auto const& vec) {
    redis::set_hashes_test(addr, vec);
    for (auto&& str : vec) std::cout << str << std::endl;
});
*/
/*
    TxMap tx2blockmap_;
    BlockMap blocks;

    auto genesis{datfiles.front()->blocks().front()};
    auto last{datfiles.back()->blocks().back()};

    auto const genesis_hash{blockparser::hash(genesis->header())};
    auto const last_hash{blockparser::hash(last->header())};

    std::cout << "Genesis:   " << genesis_hash.ToString() << std::endl;
    std::cout << "Last:      " << last_hash.ToString() << std::endl;

    //BlockTree::build(genesis_hash);

    //std::unordered_map<size_t, std::vector<BlockPtr>> blocks_per_height;

    // hash all blocks
    for (auto&& datfile: datfiles)
    {
        for (auto&& block: datfile->blocks())
        {
            auto hash{blockparser::hash(block->header())};
            blocks[hash] = block;

            auto height{hash == genesis_hash? 0: blocks[block->header().hash_previous_block_]->height() + 1};
            block->set_height(height);
        }
    }

    std::cout << "Hashed:    " << blocks.size() << std::endl;

    // forward link all blocks (beginning at last known, thereby ignoring non-mainnet blocks)
    for (auto current{last_hash}; !current.IsNull(); current = blocks[current]->header().hash_previous_block_)
    {
        auto const& hash_prev{blocks[current]->header().hash_previous_block_};
        if (!hash_prev.IsNull()) // current is not genesis
        {
            if (blocks.find(hash_prev) == std::cend(blocks))
            {
                std::cout << "Missing block " << hash_prev.ToString() << " before " << current.ToString() << std::endl;
                assert(false);
            }
            auto& predecessor{blocks[hash_prev]};
            predecessor->set_follower(current);
        }
        else
        {
            assert(current == genesis_hash);
        }
    }

    std::cout << "Linked" << std::endl;

    // set all heights and store the set of mainnet block hashes
    std::set<uint256> valid_blocks_;
    auto previous{genesis_hash};
    size_t height{};
    for (auto current{genesis_hash}; previous != last_hash; current = blocks[previous]->follower())
    {
        valid_blocks_.insert(current);
        blocks[current]->set_height(height++);
        previous = current;
    }

    std::cout << "Valid:     " << valid_blocks_.size() << std::endl;
    std::cout << "Blocks:    " << blocks.size() << std::endl;
    std::cout << "MaxHeight: " << height-1 << std::endl;


    for (auto current{genesis_hash}; !current.IsNull(); current = blocks[current]->follower())
    {
        auto const& block{blocks[current]};
        for (auto&& tx: block->transactions())
        {
            if (tx2blockmap_.find(tx.hash) != tx2blockmap_.end())
            {
                std::cout << "WARNING: DUPLICATE TX-HASH " << tx.hash.ToString() << std::endl;
            }
            tx2blockmap_[tx.hash] = current;
        }
    }

    std::cout << "TxCount:   " << tx2blockmap_.size() << std::endl;

#ifndef WITH_REDIS
    std::vector<std::string> prefixes = {"zm", "zmn", "zmns", "zmnsc", "zmnscp", "zmnscpx", "zmnscpxj"};
    std::vector<std::vector<string>> addresses;

    addresses.push_back({});
    blockparser::PubkeyPool::on_all([&](auto const& map) {
        for (auto&& entry: map) {
            addresses.back().push_back(entry.first);
        }
    });

    std::cout << "Found " << addresses.back().size() << " addresses" << std::endl;

    size_t i{};
    for (auto&& prefix: prefixes)
    {
        addresses.emplace_back();
        std::copy_if(addresses[i].begin(), addresses[i].end(), std::back_inserter(addresses.back()),
                     [&](auto const& address) { return str_tolower(address.substr(0, prefix.length())) == prefix; });
        std::cout << "Found " << addresses.back().size() << " addresses starting with " << prefix << std::endl;
        i++;
    }

#else

    std::cout << "Storing blocks in REDIT" << std::endl;
    for (auto current{genesis_hash}; !current.IsNull(); current = blocks[current]->follower())
    {
        auto const& block{blocks[current]};
        redis::store_block(block, current.ToString());
        // store_block(blocks, tx2blockmap_, current);
    }
#endif
    return 0;
}
*/
