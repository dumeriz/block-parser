#pragma once

#include "header.hpp"

namespace blockparser
{
    static uint8_t constexpr block_start_pattern[]{0xb1, 0x3b, 0x2d, 0xf6}; // chainparams.cpp
    static size_t constexpr blocksize_max{2000000};                         // primitives/block.h
    static size_t constexpr blocksize_min{80};                              // main.cpp:LoadExternalBlockFile

    static size_t constexpr header_size{80};
    static size_t constexpr stake_modifier_v2{260720}; // chainparams.cpp

    static const Header genesis_header{
        1,
        0,
        uint256{"0xa9292f633b9785d80c53fa23db9d2554f0ffc0235fcfb031dc54e9a09ffeff0c"}, // version, previous, merkle
        1553068993,
        0x1e0ffff0,
        176725,
        0 // time, bits, nonce, checkpoint
    };

    static uint256 const genesis_hash{"0x00000c428e1dfaf5cca80be43e445d7c6f2835d837c3d35a8243e0e0570f92ee"};

    static uint64_t constexpr cscript_max_size{0x02000000}; // serialize.h

    // used in pubkey-parsing in tx_out.hpp. zenon: script.h
    static unsigned char const OP_PUSHDATA1{0x4c};
    static unsigned char const OP_PUSHDATA2{0x4d};
    static unsigned char const OP_PUSHDATA4{0x4e};

    static unsigned char const OP_RETURN{0x6a};
    static unsigned char const OP_DUP{0x76};

    static unsigned char const OP_EQUAL{0x87};
    static unsigned char const OP_EQUALVERIFY{0x88};

    static unsigned char const OP_HASH160{0xa9};
    static unsigned char const OP_HASH256{0xaa};
    static unsigned char const OP_CHECKSIG{0xac};

} // namespace blockparser
