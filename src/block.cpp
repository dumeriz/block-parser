#include <block.hpp>
#include <iostream>
#include <set>
#include <tx_in.hpp>
#include <tx_out.hpp>
#include <util.hpp>
#include <znn_constants.hpp>

inline bool
is_coin_stake(blockparser::Transaction const& tx)
{
    return !tx.vin.empty() && claims_output(tx.vin[0]) && tx.vout.size() > 1 && empty(tx.vout[0]);
}

blockparser::Block
blockparser::read_block(std::ifstream& stream, uint32_t block_size, size_t block_height)
{
    auto const block_offset{stream.tellg()};

    uint8_t block_bytes[block_size];
    stream.read(reinterpret_cast<char*>(block_bytes), block_size);

    stream.seekg(block_offset);

    auto header{read_header(stream)};

    // std::cout << "Read " << stream.tellg() - pos << " bytes as header" << std::endl << header << std::endl;

    Block block{block_offset, block_height, block_size, std::move(header)};

    // Read the txns
    auto const tx_count{util::read_vectorsize(stream)};
    // std::cout << "Read a vectorsize of " << tx_count << std::endl;

    for (size_t i{}; i < tx_count; ++i)
    {
        block.transactions_.emplace_back(read_transaction(stream));
        // std::cout << "Read tx " << i << std::endl;
        // std::cout << block.transactions_.back() << std::endl;
    }

    // that's actually never the case - even in early pow blocks shown as empty in the cli
    assert(!block.transactions().empty());

    if (tx_count > 1 && is_coin_stake(block.transactions_[1]))
    {
        block.signee_.resize(util::read_vectorsize(stream));
        stream.read(reinterpret_cast<char*>(block.signee_.data()), block.signee_.size());
    }

    // std::cout << stream.tellg() - block_offset << "," << block_size << std::endl;
    assert(stream.tellg() - block_offset == block_size);
    return block;
}
