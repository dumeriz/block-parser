#pragma once

#include "exception.hpp"
#include "types.hpp"
#include "znn_constants.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <zenon/base58.h>
#include <zenon/hash.h>

namespace blockparser
{
    enum class script_t
    {
        PKH,
        PK,
        P2SH,
        DATA,
        PUZZLE,
        EMPTY,
        NONSTANDARD
    };

    struct TxOutput
    {
        int64_t amount{};
        PubKey script_pubkey{}; // required conditions to spend this output
        std::string address{};
        script_t type{script_t::EMPTY};
    };

    inline auto operator<<(std::ostream& os, script_t type) -> std::ostream&
    {
        static const std::unordered_map<script_t, std::string> names = {
            {script_t::PKH, "Pay-to-pubkey-hash"},  {script_t::PK, "Pay-to-pubkey"},
            {script_t::P2SH, "Pay-to-script-hash"}, {script_t::DATA, "Data"},
            {script_t::PUZZLE, "Puzzle"},           {script_t::EMPTY, "Empty"},
            {script_t::NONSTANDARD, "Non-Standard"}};

        os << names.at(type);
        return os;
    }

    inline auto operator<<(std::ostream& os, TxOutput const& tx) -> std::ostream&
    {
        os << std::setw(20) << "[Type " << tx.type << ", Amount " << tx.amount << "]" << tx.address << std::endl
           << std::setw(20) << "PK " << tx.script_pubkey;
        return os;
    }

    TxOutput read_tx_output(std::ifstream& stream);

    inline bool empty(TxOutput const& vout) { return !vout.amount && empty(vout.script_pubkey); }

    // references: src/script/{script.h,script.cpp,standard.cpp}. ExtractDestination.

    // pay-to-scripthash
    inline bool is_p2sh(PubKey const& pubkey)
    {
        static unsigned char const keylength{0x14};

        return pubkey.data.size() == 23 && pubkey.data.front() == OP_HASH160 && pubkey.data[1] == keylength &&
               pubkey.data.back() == OP_EQUAL;
    }

    // pay-to-pubkey
    // https://en.bitcoin.it/wiki/Script#Obsolete_pay-to-pubkey_transaction
    inline bool is_pk(PubKey const& pubkey)
    {
        return !pubkey.data.empty() && pubkey.data.front() < OP_PUSHDATA1 && // first byte signals size
               pubkey.data.back() == OP_CHECKSIG;
    }

    // pay-to-pubkey-hash
    // https://en.bitcoin.it/wiki/Script#Standard_Transaction_to_Bitcoin_address_.28pay-to-pubkey-hash.29
    inline bool is_pkh(PubKey const& pubkey)
    {
        static unsigned char const keylength{0x14};

        return pubkey.data.size() > 2 && pubkey.data[0] == OP_DUP && pubkey.data[1] == OP_HASH160 &&
               pubkey.data[2] == keylength && pubkey.data[pubkey.data.size() - 2] == OP_EQUALVERIFY &&
               pubkey.data.back() == OP_CHECKSIG;
    }

    // Unspendable outputs
    // https://en.bitcoin.it/wiki/Script#Provably_Unspendable.2FPrunable_Outputs
    inline bool is_unspendable(PubKey const& pubkey) { return pubkey.data.size() > 0 && pubkey.data[0] == OP_RETURN; }

    // Spendable by solving a puzzle.
    // https://en.bitcoin.it/wiki/Script#Transaction_puzzle
    inline bool is_puzzle(PubKey const& pubkey)
    {
        return pubkey.data.size() && pubkey.data.front() == OP_HASH256 && pubkey.data.back() == OP_EQUAL;
    }

    inline bool is_null_data(PubKey const& pubkey)
    {
        // std::cout << pubkey << std::endl;

        if (pubkey.data[0] != OP_RETURN) return false;

        bool rest_is_opcode{true};
        for (size_t i{1}; rest_is_opcode && i < pubkey.data.size(); ++i)
        {
            auto c{pubkey.data[i]};
            auto left{pubkey.data.size() - i};
            auto in_range{c < OP_PUSHDATA1 || c > OP_PUSHDATA4};
            auto valid_push1{c == OP_PUSHDATA1 && left > 1};
            auto valid_push2{c == OP_PUSHDATA2 && left > 2};
            auto valid_push4{c == OP_PUSHDATA4 && left > 4};

            rest_is_opcode = in_range || valid_push1 || valid_push2 || valid_push4;

            if (!rest_is_opcode)
            {
                std::cout << "Warning: " << std::hex << pubkey.data[i] << std::endl;
            }
        }

        return rest_is_opcode;
    }

    inline std::ostream& operator<<(std::ostream& os, std::vector<unsigned char> const& hex)
    {
        std::ostream hex_out{os.rdbuf()};
        hex_out << std::hex << std::setfill('0');

        for (auto c : hex)
        {
            hex_out << std::setw(2) << static_cast<unsigned int>(c);
        }

        return os;
    }

    inline uint160 from_pkh(TxOutput const& output)
    {
        std::vector<unsigned char> data;
        data.assign(output.script_pubkey.data.begin() + 3, output.script_pubkey.data.begin() + 0x17);
        return uint160(data);
    }

    inline uint160 from_pk(TxOutput const& output)
    {
        // compressed or uncompressed pubkey?
        int const tag = output.script_pubkey.data[1];
        auto const keylen{tag <= 3 ? 0x21 : 0x41}; // 33 or 65
        assert(output.script_pubkey.data[0] == keylen);

        auto const start{std::next(output.script_pubkey.data.begin())};
        auto const end{std::next(start, keylen)};

        assert(std::distance(start, end) == keylen);
        return Hash160(start, end);
    }

    inline uint160 from_p2sh(TxOutput const& output)
    {
        std::vector<unsigned char> data;
        data.assign(output.script_pubkey.data.begin() + 2, output.script_pubkey.data.begin() + 0x16);
        // std::cout << "P2SH Address " << uint160{data}.ToString() << std::endl;
        return uint160{data};
    }

    inline std::pair<script_t, uint160> script_sig_hash(TxOutput const& output)
    {
        if (is_pkh(output.script_pubkey)) return std::make_pair(script_t::PKH, from_pkh(output));
        if (is_pk(output.script_pubkey)) return std::make_pair(script_t::PK, from_pk(output));
        if (is_p2sh(output.script_pubkey)) return std::make_pair(script_t::P2SH, from_p2sh(output));

        if (output.script_pubkey.data.empty()) return std::make_pair(script_t::EMPTY, uint160{});
        if (is_puzzle(output.script_pubkey)) return std::make_pair(script_t::PUZZLE, uint160{});
        if (is_unspendable(output.script_pubkey)) return std::make_pair(script_t::DATA, uint160{});

        return std::make_pair(script_t::NONSTANDARD, uint160{});
    }

    inline uint160 address(TxOutput const& output)
    {
        if (is_pkh(output.script_pubkey))
        {
            std::vector<unsigned char> data;
            data.assign(output.script_pubkey.data.begin() + 3, output.script_pubkey.data.begin() + 0x17);
            return uint160(data);
        }
        else if (is_pk(output.script_pubkey))
        {
            // compressed or uncompressed pubkey?
            int const tag = output.script_pubkey.data[1];
            auto const keylen{tag <= 3 ? 0x21 : 0x41}; // 33 or 65
            assert(output.script_pubkey.data[0] == keylen);

            auto const start{std::next(output.script_pubkey.data.begin())};
            auto const end{std::next(start, keylen)};

            assert(std::distance(start, end) == keylen);
            return Hash160(start, end);
        }
        else if (is_p2sh(output.script_pubkey))
        {
            std::vector<unsigned char> data;
            data.assign(output.script_pubkey.data.begin() + 2, output.script_pubkey.data.begin() + 0x16);
            std::cout << "P2SH Address " << uint160{data}.ToString() << std::endl;
            return uint160{data};
        }
        else
        {
            std::string type{"nonstandard"};
            if (output.script_pubkey.data.empty()) type = "empty";
            if (is_puzzle(output.script_pubkey)) type = "puzzle";
            if (is_unspendable(output.script_pubkey)) type = "unspendable";

            throw pubkey_type_exception{type, output.script_pubkey};
        }
    }
} // namespace blockparser
