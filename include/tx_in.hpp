#pragma once

#include "types.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

namespace blockparser
{
    struct TxInput
    {
        // COutPoint
        uint256 tx_hash{}; // txid, referencing the claimed output tx hash
        uint32_t index{};  // vout, referencing the claimed output tx index

        PubKey script_sig{}; // tx script satisfying spent output's script condition
        uint32_t sequence{}; //
    };

    TxInput read_tx_input(std::ifstream& stream);

    inline bool claims_output(TxInput const& vin)
    {
        return !(vin.tx_hash.IsNull() && vin.index == std::numeric_limits<uint32_t>::max());
    }

    inline auto operator<<(std::ostream& os, TxInput const& tx) -> std::ostream&
    {
        os << std::setw(20) << "[Idx " << tx.index << " Seq " << tx.sequence << "] " << tx.tx_hash.ToString()
           << std::endl
           << std::setw(20) << "PK " << tx.script_sig;
        return os;
    }

} // namespace blockparser
