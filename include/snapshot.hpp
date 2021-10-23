#pragma once

#include "block.hpp"
#include "exception.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>

namespace snapshot
{
    class utxos
    {
        std::map<std::string, int64_t> utxo_balance_map_;

    public:
        auto update(std::string const& utxo, int64_t balance_change) { utxo_balance_map_[utxo] += balance_change; }

        auto all_utxos()
        {
            std::vector<std::string> keys;
            std::transform(utxo_balance_map_.begin(), utxo_balance_map_.end(), std::back_inserter(keys),
                           [](auto const& pair) { return pair.first; });
        }

        auto map() const -> std::map<std::string, int64_t> const& { return utxo_balance_map_; }
    };

    // common borrow from boosts hashing procedure as variadic; found on stackoverflow
    inline auto hash_combine(std::size_t& seed) {}

    template <typename T, typename... Rest> inline auto hash_combine(std::size_t& seed, const T& v, Rest... rest)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        hash_combine(seed, rest...);
    }

    // needing this for txns::ref_t (used as key of unordered_map)
    struct ref_t_hash
    {
        std::size_t operator()(std::pair<std::string, size_t> const& pair) const
        {
            size_t hash{};
            hash_combine(hash, pair.first, pair.second);
            return hash;
        }
    };
    class txns
    {
        using ref_t       = std::pair<std::string, size_t>;                  // tx-hash, tx-index
        using out_point_t = std::pair<std::string, int64_t>;                 // utxo, amount
        std::unordered_map<ref_t, out_point_t, ref_t_hash> referenced_utxo_; // backlog of utxos and amounts
        std::unordered_map<ref_t, int64_t, ref_t_hash> spent_amount_;        // backlog of spent balance
    public:
        auto cache(std::string tx_hash, size_t tx_index, std::string address, int64_t amount)
        {
            referenced_utxo_[std::make_pair(tx_hash, tx_index)] = std::make_pair(address, amount);
        }

        auto get(std::string const& tx_hash, size_t tx_index) const
        {
            return referenced_utxo_.at(std::make_pair(tx_hash, tx_index));
        }
    };

    class generator
    {
        utxos result_;
        std::unique_ptr<txns> backlog_{std::make_unique<txns>()};

    public:
        void add_block(blockparser::BlockPtr const& block)
        {
            // auto const& header{block->header()};
            auto const height{std::to_string(block->height())};

            auto const& transactions{block->transactions()};
            std::vector<std::string> txs;
            std::transform(transactions.begin(), transactions.end(), std::back_inserter(txs),
                           [](auto const& tx) { return tx.hash.ToString(); });

            for (auto&& tx : transactions)
            {
                auto tx_hash{tx.hash.ToString()};

                for (size_t i{}; i < tx.vout.size(); ++i)
                {
                    auto const& vout{tx.vout[i]};

                    // If it is empty, it is a coinbase nonstandard transactions. We're only interested in 'real'
                    // outputs.
                    if (vout.address.empty())
                    {
                        continue;
                    }

                    // remember this utxo-point to refer to it again when it is used as an input
                    backlog_->cache(tx_hash, i, vout.address, vout.amount);

                    // store the balance change for the referenced address
                    result_.update(vout.address, vout.amount);
                }

                // if this is not a pow or pos_coinbase transaction, it had inputs from unspent outputs,
                // which is a decrease in balance for the spending address.
                if (!blockparser::is_pow_coinbase(tx) && !blockparser::is_pos_coinbase(tx))
                {
                    for (auto&& vin : tx.vin)
                    {
                        if (!blockparser::claims_output(vin)) continue;

                        // receive the amount spent and the address spent from, to change its balance accordingly
                        auto const& outpoint{backlog_->get(vin.tx_hash.ToString(), vin.index)};
                        result_.update(outpoint.first, -outpoint.second);
                    }
                }
            }
        }

        auto write_to(std::string filename)
        {
            std::ofstream stream{filename};

            // cache is not required anymore; free space.
            backlog_.reset();

            if (!stream.good())
            {
                std::cout << "Failed to open '" << filename << "' for writing." << std::endl;
                return;
            }

            for (auto&& entry : result_.map())
            {
                stream << entry.first << ":" << entry.second << std::endl;
            }
        }
    };
} // namespace snapshot
