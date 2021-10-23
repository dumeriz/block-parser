#pragma once

#include "tx_in.hpp"
#include "tx_out.hpp"

#include <iomanip>
#include <iostream>
#include <memory>

namespace blockparser
{
    struct Transaction
    {
        int32_t version{};   // identifier of the valid consensus rule set
        uint32_t locktime{}; // a block height or unix time (google locktime parsing)
        uint256 hash{};

        std::vector<TxInput> vin{};
        std::vector<TxOutput> vout{};
    };

    inline auto operator<<(std::ostream& os, std::vector<TxInput> const& vin) -> std::ostream&
    {
        for (auto const& tx : vin)
        {
            os << tx << std::endl;
        }
        return os;
    }

    inline auto operator<<(std::ostream& os, std::vector<TxOutput> const& vout) -> std::ostream&
    {
        for (auto&& tx : vout) os << tx << std::endl;
        return os;
    }

    inline auto operator<<(std::ostream& os, Transaction const& tx) -> std::ostream&
    {
        os << "[V " << tx.version << ", locktime " << tx.locktime << "] " << tx.hash.ToString() << std::endl;
        os << "    Inputs:" << std::endl << tx.vin << "    Outputs:" << std::endl << tx.vout;
        return os;
    }

    inline bool operator==(Transaction const& lhs, Transaction const& rhs) { return lhs.hash == rhs.hash; }

    Transaction read_transaction(std::ifstream& stream);

    /// Transaction types:
    /// PoS Coinbase: output 0 is empty (nonstandard type); output 1 contains staking reward; output n-1 contains node
    /// reward.
    ///               input contains PoS Coinbase
    /// Coinbase: PoW: 1 input, 1 output; input does not claim an output (e.g. block 1, block 2, ...)
    ///           PoS: 1 input, 2 outputs; input does not claim an output (e.g. block 126, block 127, ...)

    /// a pow coinbase transaction has one output and one input with no matching output
    inline auto is_pow_coinbase(Transaction const& tx) -> bool
    {
        return tx.vin.size() == 1 && tx.vout.size() == 1 && !claims_output(tx.vin[0]);
    }

    // coinbase transaction with 1 in, 1 out and zero movement
    inline auto is_empty_pow(Transaction const& tx) -> bool
    {
        auto const type = tx.vout[0].type;
        return is_pow_coinbase(tx) && tx.vout[0].amount == 0 &&
               (type == script_t::NONSTANDARD || type == script_t::EMPTY);
    }

    /// a pos coinbase transaction has two outputs (stake, node) and one input with no matching output
    inline auto is_pos_coinbase(Transaction const& tx) -> bool
    {
        return tx.vin.size() == 1 && tx.vout.size() == 2 && !claims_output(tx.vin[0]) && !empty(tx.vout[0]);
    }

    /// a pos coinbase tx with an additional valid input
    inline auto is_pos_coinbase_ext(Transaction const& tx) -> bool
    {
        return tx.vin.size() == 1 && claims_output(tx.vin[0]) && empty(tx.vout[0]) && tx.vout.size() >= 3;
    }

    /*
    /// a coinstake transaction is marked with a first empty output
    inline auto is_coinstake(Transaction const& tx) -> bool
    {
        //return is_coinbase(tx) && tx.vout.size() >= 2 && empty(tx.vout[0]);
        return tx.vout.size() >= 2 && empty(tx.vout[0]);
    }
    */
    inline auto is_coinstake(Transaction const& tx, TxOutput const& out) -> bool
    {
        return is_pos_coinbase(tx) && tx.vout.size() == 2 && &tx.vout[1] == &out;
    }

    inline auto is_nodereward(Transaction const& tx, TxOutput const& out) -> bool
    {
        return is_pos_coinbase(tx) && &tx.vout.back() == &out;
    }

    /*
    /// a node reward is the initial output in a coinstake transaction
    inline auto is_nodereward(Transaction const& tx, TxOutput const& out) -> bool
    {
        return is_coinstake(tx) && &tx.vout.back() == &out;
    }
    */

    namespace detail
    {
        /// Hash functor to allow storage of uint256 as keys in a map.
        struct uint256_hash
        {
            std::size_t operator()(uint256 const& value) const noexcept
            {
                return std::hash<std::string>{}(value.ToString());
            }
        };
    } // namespace detail

    using TxMap = std::unordered_map<uint256, uint256, detail::uint256_hash>;
} // namespace blockparser
