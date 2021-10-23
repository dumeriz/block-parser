#pragma once

#include "exception.hpp"

#include <fstream>
#include <iomanip>
#include <zenon/uint256.h>

namespace blockparser
{
    struct Header
    {
        int32_t version_{};
        uint256 hash_previous_block_{};
        uint256 hash_merkle_root_{};
        uint32_t time_{};
        uint32_t bits_{};
        uint32_t nonce_{};
        uint256 accumulator_checkpoint_{};
    };

    /// Read the header fields from a stream.
    Header read_header(std::ifstream&);

    /// Produce the block hash.
    uint256 hash(blockparser::Header const& header);

    /*
    inline std::ofstream& operator<<(std::ofstream& os, Header const& header)
    {
        os << "version=" << header.version_ << ", hash_prev=" << header.hash_previous_block_.ToString()
           << ", hash_merkle=" << header.hash_merkle_root_.ToString() << ", time=" << header.time_
           << ", bits=" << header.bits_ << ", nonce_=" << header.nonce_;

        if (header.version_ > 3)
        {
            os << ", checkpoint=" << header.accumulator_checkpoint_.ToString();
        }

        return os;
    }
    */
} // namespace blockparser
