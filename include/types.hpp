#pragma once

#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <zenon/uint256.h>

namespace blockparser
{
    struct PubKey
    {
        std::vector<unsigned char> data{};
    };

    inline bool empty(PubKey const& pubkey) { return pubkey.data.empty(); }

    inline bool operator==(PubKey const& lhs, PubKey const& rhs) { return lhs.data == rhs.data; }

    inline std::ostream& operator<<(std::ostream& os, PubKey const& pubkey)
    {
        auto const key{pubkey.data};

        std::ostream hex_out{os.rdbuf()};
        hex_out << std::hex << /*std::setw(2) <<*/ std::setfill('0'); // setw is not 'sticky' and must be set on each op

        for (size_t i{}; i < key.size(); ++i)
        {
            hex_out << std::setw(2) << static_cast<int>(key[i]);
        }

        return os;
    }

    inline std::string to_string(PubKey const& pubkey)
    {
        std::stringstream ss;
        ss << pubkey;
        return ss.str();
    }

    struct PubKeyHash
    {
        std::size_t operator()(PubKey const& pubkey) const noexcept
        {
            return std::hash<std::string>{}(to_string(pubkey));
        }
    };

    struct Transaction;

    struct PubkeyPool
    {
    public:
        using map_t = std::unordered_map<std::string, std::vector<std::string>>;

        static bool has(std::string const& address)
        {
            std::scoped_lock guard{access_mutex};

            auto it{pool->map.find(address)};
            return it != pool->map.end();
        }

        static bool insert(std::string address, typename map_t::mapped_type::value_type value)
        {
            bool exists{has(address)};
            if (!exists)
            {
                std::scoped_lock guard{access_mutex};
                pool->map[address] = {};
            }

            {
                std::scoped_lock guard{access_mutex};
                pool->map[address].emplace_back(std::move(value));
            }

            return !exists;
        }

        static void on_key(map_t::key_type const& key, std::function<void(map_t::mapped_type&)> modifier)
        {
            if (has(key))
            {
                std::scoped_lock guard{access_mutex};
                modifier(pool->map[key]);
            }
        }

        static void on_all(std::function<void(map_t const&)> reader)
        {
            std::scoped_lock guard{access_mutex};
            reader(pool->map);
        }

    private:
        PubkeyPool() = default;
        static std::unique_ptr<PubkeyPool> pool;
        static std::mutex access_mutex;

        map_t map;
    };

    inline std::unique_ptr<PubkeyPool> PubkeyPool::pool{std::unique_ptr<PubkeyPool>(new PubkeyPool{})};
    inline std::mutex PubkeyPool::access_mutex{};
} // namespace blockparser
