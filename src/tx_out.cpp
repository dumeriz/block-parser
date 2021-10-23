#include "tx_out.hpp"

#include "util.hpp"
#include "znn_constants.hpp"

blockparser::TxOutput
blockparser::read_tx_output(std::ifstream& stream)
{
    TxOutput output;

    util::read(stream, output.amount);

    assert(output.amount >= 0);
    output.script_pubkey.data.resize(util::read_vectorsize(stream));
    // std::cout << "Output pubkey size: " << output.pubkey.data.size() << std::endl;

    if (output.script_pubkey.data.size())
    {
        stream.read(reinterpret_cast<char*>(output.script_pubkey.data.data()), output.script_pubkey.data.size());
    }
    // std::cout << "=> " << output.pubkey << std::endl;

    return output;
}
