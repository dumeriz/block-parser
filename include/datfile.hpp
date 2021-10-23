#pragma once

#include "block.hpp"
#include "util.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace blockparser
{
    class Datfile
    {
    public:
        BlockVec const& blocks() const { return blocks_; }

        BlockMap const& blockhashtable() const { return hashmap_; }

        TxMap const& tx2blockhashmap() const { return tx2blockmap_; }

        explicit Datfile(std::string filepath) : filepath_{std::move(filepath)}
        {
            std::cout << "Reading blocks from " << filepath_ << std::endl;

            std::ifstream file{filepath_, std::ios::in | std::ios::binary};
            assert(file.good());

            auto const block_limits{locate_blocks(file)};
            _assert_block_limits_valid(file, block_limits);

            // std::cout << block_limits.size() << " blocks found" << std::endl;
            // std::cout << block_limits.back().first << block_limits.back().second << std::endl;

            uint256 hash;
            for (auto&& limits : block_limits)
            {
                file.seekg(limits.first);

                // std::cout << "Reading block @ " << limits.first << std::endl;

                uint32_t block_length{}; // 461
                file.read(reinterpret_cast<char*>(&block_length), sizeof(block_length));

                auto offs{file.tellg()};

                try
                {
                    blocks_.emplace_back(std::make_shared<Block>(read_block(file, block_length, blocks_.size())));
                    // std::cout << "Read block " << blocks_.size() << std::endl;
                    // std::cout << *blocks_.back() << std::endl;
                }
                catch (blockparser::exception const& blke)
                {
                    std::cout << blke.what() << " in block after " << hash.ToString() << std::endl;
                    dump(file, limits);
                    parse_ok_ = false;
                    break;
                }

                if (file.tellg() - offs != block_length)
                {
                    std::cout << "Wrong blocksize" << std::endl;
                    break;
                }
                hash = blockparser::hash(blocks_.back()->header());
                auto const hash_str{hash.ToString()};
                // std::cout << "At " << limits.first << ": " << hash.ToString() << std::endl;
                // std::cout << blocks_.back()->header() << std::endl;
                if (limits.second != -1 && block_length != limits.second - limits.first - sizeof(block_length))
                {
                    std::cout << " Hash of unmatching size: " << hash.ToString() << std::endl;
                }
                // std::cout << "Read block " << hash_str << std::endl;
                // break;
            }

            // std::cout << "Blocks read" << std::endl;
        }

        BlockVec& blocks() { return blocks_; }

        explicit operator bool() const { return parse_ok_; }

    private:
        std::string const filepath_;
        BlockVec blocks_;
        BlockMap hashmap_;
        BlockLinks block_links_;
        TxMap tx2blockmap_;

        bool parse_ok_{true};

        std::vector<std::pair<std::ifstream::pos_type, std::ifstream::pos_type>> locate_blocks(
            std::ifstream& file) const
        {
            std::vector<std::pair<std::ifstream::pos_type, std::ifstream::pos_type>> block_limits;

            file.seekg(0, file.beg);
            while (!file.eof())
            {
                block_limits.emplace_back(locate_next_block(file));
            }

            if (block_limits.back().first == -1)
            {
                block_limits.pop_back(); // last is eof/eof
            }

            file.clear(); // reset eof flag

            return block_limits;
        }

        std::pair<std::ifstream::pos_type, std::ifstream::pos_type> locate_next_block(std::ifstream& file) const
        {
            static auto constexpr block_start_size{sizeof(block_start_pattern)};

            auto const current_pos{file.tellg()};
            auto const begin{_locate_block_start_pattern(file)};

            if (current_pos != begin)
            {
                std::cout << "Expected block start at " << current_pos << ", found " << begin << std::endl;
            }

            if (file.eof())
            {
                return std::make_pair(-1, -1);
            }

            file.seekg(1, file.cur); // skip start pattern
            auto const end{_locate_block_start_pattern(file)};
            // std:: cout << "Read " << begin << " till " << end << std::endl;

            return std::make_pair(begin + static_cast<std::ifstream::pos_type>(block_start_size), end);
        }

        void _assert_block_limits_valid(
            std::ifstream& file,
            std::vector<std::pair<std::ifstream::pos_type, std::ifstream::pos_type>> const& block_limits)
        {
            file.clear();
            auto const filepos{file.tellg()};

            for (size_t i{}; i < block_limits.size(); ++i)
            {
                auto const& limits{block_limits[i]};

                file.seekg(limits.first);

                uint32_t block_length{};
                file.read(reinterpret_cast<char*>(&block_length), sizeof(block_length));

                auto const calculated_blocksize{limits.second - limits.first - sizeof(block_length)};

                auto const valid{limits.second == -1 ||
                                 (block_length >= blocksize_min && block_length <= blocksize_max &&
                                  block_length == calculated_blocksize)};

                // std::cout << limits.first << "-" << limits.second << "::" << block_length << "," <<
                // calculated_blocksize << std::endl;
                if (!valid)
                {
                    std::cout << "Warning: " << calculated_blocksize << " vs. " << block_length << std::endl;
                    std::cout << "At block " << i << ", offset=" << limits.first << std::endl;
                    auto const start_pattern_size{static_cast<std::ifstream::pos_type>(sizeof(block_start_pattern))};
                    auto const length{std::min(static_cast<uint32_t>(limits.second - limits.first), block_length) +
                                      2 * start_pattern_size};
                    char block[length];
                    file.seekg(limits.first - start_pattern_size);
                    file.read(block, length);
                    std::ofstream ofile{"wrongblock.blk", std::ios::binary};
                    ofile.write(block, length);
                }
                // assert(valid);
            }

            file.clear();
            file.seekg(filepos);
        }

        // Locate the next position of the block start pattern.
        // If no block start pattern is found, eof is returned.
        // The stream is positioned in front of the found pattern, or at eof.
        std::ifstream::pos_type _locate_block_start_pattern(std::ifstream& file) const
        {
            static auto constexpr block_start_size{sizeof(block_start_pattern)};
            uint8_t block_start_buffer[block_start_size];

            // Locate the block-start-pattern, or eof.

            while (true)
            {
                auto const pos_before{file.tellg()};
                file.ignore(std::numeric_limits<std::streamsize>::max(), block_start_pattern[0]);
                auto const pos_after{file.tellg()};
                assert(file.eof() || file.tellg() > 0);

                if (file.eof())
                {
                    return file.tellg();
                }

                else
                {
                    // first symbol from block-start-pattern has been removed from the stream by ignore
                    auto const before{file.tellg()};
                    // This is a non-modifying putback, thus should be accepted. It is not, however,
                    // at least on macos.
                    // Still, the symbol is there, apparently.
                    if (!file.putback(block_start_pattern[0]))
                    {
                        file.clear();
                        file.seekg(-1, file.cur);
                    }
                    auto const after{file.tellg()};
                    assert(before == (after + std::streamoff(1)));
                    assert(file.tellg() != -1);

                    auto const candidate{file.tellg()};

                    if (file.read(reinterpret_cast<char*>(block_start_buffer), block_start_size);
                        !std::memcmp(block_start_buffer, block_start_pattern, block_start_size))
                    {
                        // std::cout << "Found block start at " << candidate << std::endl;
                        file.seekg(-block_start_size, file.cur);
                        assert(file.tellg() != -1);

                        return candidate;
                    }
                    else // skip the wrongly identified candidate
                    {
                        file.seekg(candidate);
                        file.get();
                    }
                }
            }
        }

        void dump(std::ifstream& f, std::pair<std::ifstream::pos_type, std::ifstream::pos_type> const& limits)
        {
            f.seekg(limits.first);
            char data[limits.second - limits.first];
            f.read(data, limits.second - limits.first);
            std::ofstream ofile{"blockdump.blk", std::ios::binary};
            ofile.write(data, limits.second - limits.first);
        }
    };
} // namespace blockparser
