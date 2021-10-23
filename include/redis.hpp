#pragma once

#include "block.hpp"
#include "exception.hpp"

#include <atomic>
#include <chrono>
#include <cpp_redis/cpp_redis>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>

namespace redis
{
    void commit();

    namespace detail
    {
        class redis
        {
        private:
            cpp_redis::client client_;

            static std::unique_ptr<redis> singleton_;

            redis()
            {
                cpp_redis::active_logger = std::make_unique<cpp_redis::logger>();
                client_.connect("127.0.0.1", 6379,
                                [this](const std::string&, std::size_t, cpp_redis::connect_state status)
                                {
                                    connected_.store(status == cpp_redis::connect_state::ok);
                                    std::cout << "REDIS_STATE: "
                                              << static_cast<std::underlying_type_t<cpp_redis::connect_state>>(status)
                                              << std::endl;
                                });
            }

            std::atomic_bool connected_{false};

        public:
            ~redis()
            {
                if (connected_.load())
                {
                    client_.sync_commit();
                }
            }

            static std::optional<cpp_redis::client*> client()
            {
                if (!singleton_)
                {
                    singleton_ = std::unique_ptr<redis>(new redis());
                }

                auto timeout{std::chrono::system_clock::now() + std::chrono::seconds(1)};
                while (!singleton_->connected_.load() && std::chrono::system_clock::now() < timeout)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }

                if (singleton_->connected_.load())
                {
                    return std::make_optional(&singleton_->client_);
                }

                return std::nullopt;
            }
        };

        std::unique_ptr<redis> redis::singleton_{};

        void ignore_reply(cpp_redis::reply const&) {}
        void print_reply(cpp_redis::reply const& reply) { std::cout << reply << std::endl; }

    } // namespace detail

    void set_hashes_test(std::string const& addr, std::vector<std::string> const& txs)
    {
        if (auto client{detail::redis::client()})
        {
            client.value()->lpush("tx:" + addr, txs, detail::ignore_reply);
        }
        else
        {
            std::cerr << "Could not access the redis client" << std::endl;
        }
    }

    void set_pubkey_txs(std::string const& addr, std::vector<std::string> const& txs)
    {
        if (auto client{detail::redis::client()})
        {
            client.value()->sadd("pubkey:" + addr, txs,
                                 [](cpp_redis::reply& reply) { std::cout << reply << std::endl; });
        }
        else
        {
            std::cerr << "Could not access the redis client" << std::endl;
        }
    }

    void store_block(blockparser::BlockPtr const& block, uint256 const& hash)
    {

        if (auto client{detail::redis::client()})
        {
            client.value()->set("block:height:" + std::to_string(block->height()), hash.ToString(),
                                detail::ignore_reply);
            std::vector<std::string> meta{std::to_string(block->height()),
                                          std::to_string(block->header().time_),
                                          std::to_string(block->header().version_),
                                          block->header().hash_merkle_root_.ToString(),
                                          block->header().hash_previous_block_.ToString(),
                                          block->follower().ToString()};
            client.value()->lpush("block:hash:" + hash.ToString(), meta, detail::ignore_reply);
            client.value()->set("block:time:" + std::to_string(block->header().time_), std::to_string(block->height()),
                                detail::ignore_reply);

            // store all txs in this block as set under <block-hash>:txs
            std::vector<std::string> txs;
            std::transform(block->transactions().begin(), block->transactions().end(), std::back_inserter(txs),
                           [](auto const& tx) { return tx.hash.ToString(); });

            client.value()->sadd(hash.ToString() + ":txs", txs, [](cpp_redis::reply&) {});
        }
    }

    void store_block(blockparser::BlockPtr const& block, std::string const& hash)
    {
        static size_t running_height{1}; // Just used for validation

        if (block->height() != running_height++)
        {
            throw blockparser::RedisException{"Expected block " + std::to_string(running_height - 1) + ", got " +
                                              std::to_string(block->height())};
        }

        // auto const& header{block->header()};
        auto const height{std::to_string(block->height())};

        // std::cout << "Redis: Storing block " << height << ":" << hash << std::endl;

        if (auto opt_client{detail::redis::client()})
        {
            auto& client{opt_client.value()};

            // store the max known blockheight
            client->set("znn:blocks:top", height, detail::ignore_reply);

            // This is not required for the current use case and saves us some RAM
            // store block meta in a hash
            // client->hmset("znn:block:" + hash,
            //              { {"height", height},
            //                {"time", std::to_string(header.time_)},
            //                {"version", std::to_string(header.version_)},
            //                {"bits", std::to_string(header.bits_)},
            //                {"nonce", std::to_string(header.nonce_)} },
            //              detail::ignore_reply);

            // not required currently, but inserted for reference
            // store the block hash under key block:hash:<height>
            client->set("znn:block:hash:" + height, hash, detail::ignore_reply);

            auto const& transactions{block->transactions()};
            std::vector<std::string> txs;
            std::transform(transactions.begin(), transactions.end(), std::back_inserter(txs),
                           [](auto const& tx) { return tx.hash.ToString(); });

            // not required currently, but inserted for reference
            // store all transaction hashes in a set block:txns:<height>
            client->sadd("znn:block:txns:" + height, txs, detail::ignore_reply);

            // not required currently
            // store a link to the block for each tx
            // using compatible_map = std::vector<std::pair<std::string, std::string>>;
            // compatible_map tx_hashes(txs.size());
            // std::transform(txs.begin(), txs.end(), std::back_inserter(tx_hashes),
            //               [&](auto tx) { return std::make_pair(tx, height); });

            // client->hmset("znn:tx:block", tx_hashes, detail::ignore_reply);

            // std::cout << "Redis: Storing " << transactions.size() << " txns" << std::endl;

            std::unordered_map<std::string, int64_t> balance_updates;

            for (auto&& tx : transactions)
            {
                auto tx_hash{tx.hash.ToString()};

                // store every vout with a unique id.
                for (size_t i{}; i < tx.vout.size(); ++i)
                {
                    auto const si{std::to_string(i)};
                    auto const& vout{tx.vout[i]};

                    // If it is empty, it is a coinbase nonstandard transactions.
                    if (vout.address.empty())
                    {
                        if (vout.amount)
                        {
                            // Not required currently
                            // client->sadd("znn:nonstandard", tx_hash);
                            // client->set("znn:nonstandard:" + tx_hash + ":n:" + si);
                            // client->set("znn:nonstandard:" + tx_hash + ":amount:" + std::to_string(vout.amount));
                        }
                        continue;
                    }

                    if (vout.address.length() != 34)
                    {
                        throw blockparser::RedisException{"In block " + height + ", TX=" + tx_hash + ":\n" +
                                                          "Address was " + vout.address + ", index " + si +
                                                          ", amount " + std::to_string(vout.amount)};
                    }

                    client->set("znn:tx:" + tx_hash + ":n:" + si, vout.address, detail::ignore_reply);
                    client->set("znn:tx:" + tx_hash + ":amount:" + si, std::to_string(vout.amount),
                                detail::ignore_reply);

                    // Not required currently
                    // client->hmset("znn:vout:" + sid,
                    //              { {"tx", tx_hash}, {"n", si},
                    //                {"address", vout.address}, {"amount", std::to_string(vout.amount)} },
                    //              detail::ignore_reply);

                    // client->set("znn:vout:id:" + tx_hash + ":" + si, sid, detail::ignore_reply);

                    // accumulate all balance changes for every referenced address
                    if (tx.vout[i].amount > 0)
                    {
                        balance_updates[vout.address] += vout.amount;
                    }
                }

                commit();

                // if this is not a pow or pos_coinbase transaction, it had inputs from unspent outputs,
                // which is a decrease in balance for the spending address.
                if (!blockparser::is_pow_coinbase(tx) && !blockparser::is_pos_coinbase(tx))
                {
                    for (auto&& vin : tx.vin)
                    {
                        if (!blockparser::claims_output(vin)) continue;

                        // receive the address spent from, to mark it as changed in balance in the current block
                        auto f1{client->get("znn:tx:" + vin.tx_hash.ToString() + ":n:" + std::to_string(vin.index))};

                        // receive the amount spent, to store it as (negative) change in balance
                        auto f2{
                            client->get("znn:tx:" + vin.tx_hash.ToString() + ":amount:" + std::to_string(vin.index))};

                        commit();

                        auto const consumed_address{f1.get()};
                        auto const consumed_amount{f2.get()};

                        if (!consumed_address || !consumed_amount)
                        {
                            auto addr{static_cast<bool>(consumed_address)};
                            auto amnt{static_cast<bool>(consumed_amount)};
                            throw blockparser::RedisException{
                                "In block " + height + ", TX=" + tx_hash + ":\n" + "Address was " +
                                std::to_string(addr) + ", amount " + std::to_string(amnt) + " for vin referencing " +
                                vin.tx_hash.ToString() + ", n=" + std::to_string(vin.index)};
                        }

                        auto const balance_change = std::stoll(consumed_amount.as_string());
                        balance_updates[consumed_address.as_string()] -= balance_change;
                    }
                }
            }

            // store every utxo as a member of the set of known addresses
            std::vector<std::string> utxos;
            std::transform(balance_updates.begin(), balance_updates.end(), std::back_inserter(utxos),
                           [](auto const& pair) { return pair.first; });

            if (utxos.empty())
            {
                throw blockparser::RedisException{"Block " + height + ": Empty UTXO set."};
            }

            client->sadd("znn:utxos", utxos, detail::ignore_reply);

            // store this block as a point of change for every receiving address;
            // store the amount as a positive balance change
            for (auto&& [key, balance_update] : balance_updates)
            {
                client->sadd("znn:blocks:" + key, {height}, detail::ignore_reply);
                client->set("znn:change:" + key + ":" + height, std::to_string(balance_update), detail::ignore_reply);
            }

            commit();
            // std::cout << "Stored block " << hash << std::endl;
        }
    }

    void modify_balance(std::string address, int64_t amount)
    {
        if (auto client{detail::redis::client()})
        {
            client.value()->incrbyfloat(std::move(address) + ":balance", amount, detail::ignore_reply);
        }
    }

    void link_vout_to_block(std::string address, std::string const& block_hash)
    {
        if (auto client{detail::redis::client()})
        {
            client.value()->sadd(std::move(address) + ":consumed:block", {block_hash}, detail::ignore_reply);
        }
    }

    void link_vout_to_tx(std::string address, std::string const& tx_hash)
    {
        if (auto client{detail::redis::client()})
        {
            client.value()->sadd(address + ":consumed:tx", {tx_hash}, detail::ignore_reply);
            client.value()->sadd(tx_hash + ":input:from", {std::move(address)}, detail::ignore_reply);
        }
    }

    void load_script(std::string path)
    {
        if (std::ifstream script{path, std::ios::binary | std::ios::ate})
        {
            auto size{script.tellg()};
            std::cout << size << std::endl;
            script.seekg(0);
            std::string content(size, '\0');

            if (script.read(&content[0], size))
            {
                if (auto client{detail::redis::client()})
                {
                    client.value()->script_load(content, detail::print_reply);
                }
            }
        }
    }

    void commit()
    {
        if (auto client{detail::redis::client()})
        {
            client.value()->sync_commit();
        }
    }

} // namespace redis
