#include <header.hpp>
#include <util.hpp>
#include <zenon/hash.h>
#include <znn_constants.hpp>

blockparser::Header blockparser::read_header(std::ifstream& stream)
{
    Header header;

    util::read(stream, header.version_, header.hash_previous_block_, header.hash_merkle_root_, header.time_,
               header.bits_, header.nonce_);

    if (header.version_ > 3)
    {
        util::read(stream, header.accumulator_checkpoint_);
    }

    return header;
}

// utilstrencodings.h:
#define END(a) ((char const*)&((&(a))[1]))

uint256 blockparser::hash(blockparser::Header const& header)
{
    auto const begin{reinterpret_cast<char const*>(&header.version_)};

    auto const tmp{END(header.nonce_)};
    auto const tmp2{END(header.accumulator_checkpoint_)};
    auto const nonce_addr{reinterpret_cast<char const*>(&(&header.nonce_)[1])};
    auto const accum_addr{reinterpret_cast<char const*>(&(&header.accumulator_checkpoint_)[1])};

    // Validate that I'm reading the macro above correctly
    auto a{HashQuark(begin, nonce_addr)};
    auto b{HashQuark(begin, tmp)};
    auto c{Hash(begin, accum_addr)};
    auto d{Hash(begin, tmp2)};
    assert(header.version_ >= 4 || (a == b));
    assert(header.version_ < 4 || (c == d));

    return header.version_ < 4 ? HashQuark(begin, nonce_addr) : Hash(begin, accum_addr);
}
