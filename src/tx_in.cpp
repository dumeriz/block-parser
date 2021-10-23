#include "tx_in.hpp"

#include "util.hpp"
#include "znn_constants.hpp"

blockparser::TxInput blockparser::read_tx_input(std::ifstream& stream)
{
    // Serialized TX Inputs consist of (in that order):
    // - a COutPoint (a tx hash and an index, locating the claimed tx-out)
    // - a CScript (signature for the output's pubkey)
    // - an nSequence field (see https://bitcoin.org/en/transactions-guide#locktime-and-sequence-number)
    // CScript is an extension of vector, which is serialized with its size in prepended.
    // Extraction of that field is adapted from serialize.h:ReadCompactSize.
    // nSequence is a 32bit field.
    // The information from COutPoint is included directly into TxInput here.

    TxInput input;

    util::read(stream, input.tx_hash, input.index);

    input.script_sig.data.resize(util::read_vectorsize(stream));
    // std::cout << "Input pubkey size: " << input.pubkey.data.size() << std::endl;

    stream.read(reinterpret_cast<char*>(input.script_sig.data.data()), input.script_sig.data.size());
    // std::cout << "=> " << input.pubkey << std::endl;

    util::read(stream, input.sequence);

    return input;
}
