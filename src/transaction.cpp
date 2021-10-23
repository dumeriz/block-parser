#include "transaction.hpp"

#include "exception.hpp"

#include <types.hpp>
#include <util.hpp>
#include <zenon/hash.h>

void
check_for_coinbase(blockparser::TxInput const& input)
{
    assert(input.tx_hash == 0x0);      // no previous outpoint
    assert(input.index == 0xffffffff); // no previous outpoint
}

/// Validates that the interpretation of transaction types is correct
auto
assert_schema_matches_assumption(blockparser::Transaction const& tx)
{
    static int current_phase = 0; // pow
    static bool seen_regular_tx{};

    if (blockparser::is_pow_coinbase(tx))
    {
        if (current_phase != 0)
        {
            if (!blockparser::is_empty_pow(tx))
            {
                std::cout << "Unexpected POW:" << std::endl;
                std::cout << tx << std::endl;
                assert(false);
            }
        }
    }
    else if (blockparser::is_pos_coinbase(tx))
    {
        if (current_phase == 0)
        {
            std::cout << "Switching to POS: " << std::endl;
            std::cout << tx << std::endl;
            current_phase = 1;
        }
        else if (current_phase != 1)
        {
            std::cout << "Unexpected POS:" << std::endl;
            std::cout << tx << std::endl;
            assert(false);
        }
    }
    else if (blockparser::is_pos_coinbase_ext(tx))
    {
        if (current_phase == 1)
        {
            std::cout << "Switching to POS_EXT: " << std::endl;
            std::cout << tx << std::endl;
            current_phase = 2;
        }
        else if (current_phase != 2)
        {
            std::cout << "Unexpected POS_EXT:" << std::endl;
            std::cout << tx << std::endl;
            assert(false);
        }
    }
    else if (!seen_regular_tx)
    {
        std::cout << "First regular TX:" << std::endl;
        std::cout << tx << std::endl;
        seen_regular_tx = true;
    }
}

blockparser::Transaction
blockparser::read_transaction(std::ifstream& stream)
{
    // Remember the stream position, because we need to roll back after extraction of
    // the tx contents, to pass the serialized tx to the hasher.
    auto const tx_begin{stream.tellg()};

    Transaction tx;

    util::read(stream, tx.version);

    tx.vin.resize(util::read_vectorsize(stream));
    // std::cout << tx.vin.size() << " inputs" << std::endl;

    for (size_t i{}; i < tx.vin.size(); ++i)
    {
        tx.vin[i] = read_tx_input(stream);
        // std::cout << tx.vin[i].tx_hash.GetHex() << "," << tx.vin[i].index << std::endl;
        // std::cout << tx.vin[i].script_sig << std::endl;
    }

    // check_for_coinbase(tx.vin[0]);

    tx.vout.resize(util::read_vectorsize(stream));
    // std::cout << tx.vout.size() << " outputs" << std::endl;

    for (size_t i{}; i < tx.vout.size(); ++i)
    {
        tx.vout[i] = read_tx_output(stream);

        auto [type, script_sig]{script_sig_hash(tx.vout[i])};
        tx.vout[i].type = type;

        if (type == script_t::PK || type == script_t::PKH || type == script_t::P2SH)
        {
            // chainparams.cpp / base58Prefixes
            auto const prefix = static_cast<unsigned char>(type == script_t::P2SH ? 15 : 80);

            // base58.h:BitcoinAddress
            // base58.cpp:CBitcoinAddressVisitor
            // CBitcoinAddress(addr).ToString(), Set, SetData
            std::vector<unsigned char> reversed_endianness(script_sig.size());
            std::memcpy(reversed_endianness.data(), script_sig.begin(), script_sig.size());

            auto vch{std::vector<unsigned char>(1, prefix)};
            vch.insert(vch.end(), reversed_endianness.begin(), reversed_endianness.end()); // + 20);
            auto hash{Hash(vch.begin(), vch.end())};
            vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);

            tx.vout[i].address = EncodeBase58(vch);
        }

        else
        {
            assert((type == script_t::EMPTY && i == 0) || type != script_t::EMPTY);
        }
    }

    util::read(stream, tx.locktime);
    // std::cout << "Locktime " << tx.locktime << std::endl;

    // Remember this position, to jump back here after the hash has been constructed
    // from the just read stream data.
    auto const tx_end{stream.tellg()};
    auto const tx_size{tx_end - tx_begin};
    // std::cout << "TX-Size: " << tx_size << std::endl;

    stream.seekg(tx_begin, stream.beg);

    unsigned char buffer[tx_size];
    stream.read(reinterpret_cast<char*>(buffer), tx_size);

    assert(stream.tellg() == tx_end);

    CHash256 hasher;
    hasher.Write(buffer, tx_size);

    hasher.Finalize(reinterpret_cast<unsigned char*>(&tx.hash));
    // std::cout << "Hash " << tx.hash << std::endl;

    assert_schema_matches_assumption(tx);

    return tx;
}
