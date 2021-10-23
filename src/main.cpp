#include "snapshot.hpp"
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

// naive way to enumerate the available blockdata files in directory where:
// try opening sequentially until that fails.
auto
enumerate_blockfiles(std::string where)
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

int
main(int argc, char** argv)
{
    if (argc < 3)
    {
        auto const expected_call{std::string{argv[0]} + " <path> <n>"};
        std::cout << "Expected " << expected_call
                  << " with <path>=absolute path root of 'blocks' folder, <n>=requested height." << std::endl;
        return -1;
    }

    auto const blocksdir{std::string{argv[1]}};
    auto const blocknum{std::stoll(argv[2])};

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

            // If the requested block has been read, we can quit this loop.
            // We're reading a bit more though, to mitigate the risk of ending on a block from a forked state.
            // 5 hours of blocks are assumed to be safe here.
            if (blocks.size() >= (blocknum + 300)) break;
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
    last_block->set_height(count);

    size_t height{};
    for (auto* block{genesis}; !block->follower().IsNull(); block = blocks[block->follower()].get())
    {
        block->set_height(height++);
        if (block->follower().IsNull()) assert(block == last_block);
    }

    assert(last_block->height() == height);
    assert(blocks[last_block->header().hash_previous_block_]->height() == last_block->height() - 1);

    // Free some space by removing blocks from forked chains. Also remove blocks above the requested height.
    size_t removed{};
    for (auto it{blocks.begin()}; it != blocks.end();)
    {
        auto const& previous_hash{it->second->header().hash_previous_block_};
        assert(!previous_hash.IsNull() || it->second.get() == genesis);

        auto previous_is_deleted{blocks.find(previous_hash) == std::end(blocks)};

        auto const is_from_forked_state{it->second.get() != genesis &&
                                        (previous_is_deleted || blocks[previous_hash]->follower() != it->first)};
        auto const is_above_height{it->second->height() > blocknum};

        if (is_from_forked_state || is_above_height)
        {
            it = blocks.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }

    std::cout << "Removed " << removed << " blocks." << std::endl;

    // validation: Iterating through the chain now by following the follower-links, we should have
    // a height-sequence of increments by one. Initial height must be 0.
    size_t control_height{0};
    for (auto hash{blockparser::hash(genesis->header())}; control_height <= blocknum; hash = blocks[hash]->follower())
    {
        auto actual_height{blocks[hash]->height()};
        if (actual_height != control_height)
        {
            std::cout << "Expected height " << control_height << ", found " << actual_height << std::endl;
            assert(false);
        }
        control_height++;
    }

    std::cout << "Creating snapshot from " << blocks.size() << " blocks." << std::endl;

    auto snapshot_tool{snapshot::generator{}};

    height = 0;
    for (auto hash{blockparser::hash(genesis->header())}; !hash.IsNull() && height++ <= blocknum;)
    {
        auto block_ptr{blocks[hash]};
        snapshot_tool.add_block(block_ptr);
        // Free space when processed
        blocks.erase(hash);
        hash = block_ptr->follower();
    }

    snapshot_tool.write_to("snapshot-" + std::to_string(blocknum) + ".txt");
}
