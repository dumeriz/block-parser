#pragma once

#include "header.hpp"
#include "transaction.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

namespace blockparser
{
    class Block
    {
    public:
        Block(std::ifstream::pos_type position, size_t height, uint32_t size, Header header)
            : position_{position}, height_{height}, size_{size}, header_{header}
        {
        }

        size_t offset() const { return position_; }
        size_t height() const { return height_; }
        uint32_t size() const { return size_; }
        Header const& header() const { return header_; }

        std::vector<Transaction> const& transactions() const { return transactions_; }

        void set_height(size_t height) { height_ = height; }

        void set_follower(uint256 hash) { follower_ = std::move(hash); }

        uint256 const& follower() const { return follower_; }

    private:
        std::ifstream::pos_type position_{};
        size_t height_{};
        uint32_t size_{};
        Header header_{};

        std::vector<Transaction> transactions_{};
        std::vector<unsigned char> signee_{};

        uint256 follower_{}; // next block hash

        friend Block read_block(std::ifstream&, uint32_t, size_t);
    };

    Block read_block(std::ifstream& stream, uint32_t block_size, size_t block_height);

    using BlockPtr = std::shared_ptr<Block>;
    using BlockVec = std::vector<BlockPtr>;
    // hash function defined in transaction.hpp
    using BlockMap   = std::unordered_map<uint256, BlockPtr, detail::uint256_hash>;
    using BlockLinks = std::unordered_map<uint256, std::pair<uint256, uint256>, detail::uint256_hash>;

    inline auto operator<<(std::ostream& os, std::vector<Transaction> const& transactions) -> std::ostream&
    {
        if (transactions.empty()) os << std::setw(15) << "  <empty>" << std::endl;

        for (size_t i{}; i < transactions.size(); i++)
        {
            os << transactions[i] << std::endl;
        }

        return os;
    }

    inline auto operator<<(std::ostream& os, Block const& block) -> std::ostream&
    {
        os << std::setw(10) << "Offset: " << block.offset() << std::endl
           << std::setw(10) << "Height: " << block.height() << std::endl
           << std::setw(10) << "Size: " << block.size() << std::endl
           << std::setw(10) << "Txns: " << std::endl
           << block.transactions();

        return os;
    }
} // namespace blockparser
