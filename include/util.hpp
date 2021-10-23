#pragma once

#include "block.hpp"
#include "header.hpp"
#include "znn_constants.hpp"

#include <iostream>

inline std::ostream&
operator<<(std::ostream& os, uint256 const& value)
{
    os << value.GetHex();
    return os;
}

namespace blockparser
{
    namespace util
    {
        /// Deserialize a T from the buffer at *b, and forward the buffer pointer by the size of T.
        template <typename T> inline void read(uint8_t const** b, T& t)
        {
            t = *reinterpret_cast<T const*>(*b);
            *b += sizeof(T);
        }

        /// Deserialize a sequence of of values from a buffer.
        template <typename... Args> inline void read(uint8_t const* bytes, Args&... args) { (read(&bytes, args), ...); }

        template <typename T> inline void read(std::ifstream& f, T& target)
        {
            f.read(reinterpret_cast<char*>(&target), sizeof(T));
        }

        template <typename... Args> inline void read(std::ifstream& f, Args&... args) { (read(f, args), ...); }

        // Read the size of the CScript serialized in the stream. The size field is not of fixed length;
        // see serialize.h:WriteCompactSize.
        // Note, that the possibility of size being a uint64 is excluded here; it conflicts
        // with the requirement of the size being smaller than cscript_max_size (in serialize.h: MAX_SIZE).
        inline size_t read_vectorsize(std::ifstream& stream)
        {
            size_t vector_size{};
            uint64_t flagged_minsize{}; // depending on the encoded type extracted below

            unsigned char const flag_size_is_ushort{253};
            unsigned char const flag_size_is_uint{254};

            unsigned char uchar_field; // signaling the size of the script
            read(stream, uchar_field);

            if (uchar_field < flag_size_is_ushort)
            {
                vector_size     = uchar_field;
                flagged_minsize = 0;
            }
            else if (uchar_field == flag_size_is_ushort)
            {
                unsigned short size{};
                read(stream, size);
                vector_size     = size;
                flagged_minsize = 253;
            }
            else if (uchar_field == flag_size_is_uint)
            {
                unsigned int size{};
                read(stream, size);
                vector_size     = size;
                flagged_minsize = 0x10000u;
            }
            else
            {
                throw ParseException{"Unexpected size signaled for TX-In:CScript: " + std::to_string(uchar_field)};
            }

            if (vector_size < flagged_minsize || vector_size > cscript_max_size)
            {
                throw ParseException{"Invalid size read for TX-In:CScript: " + std::to_string(vector_size)};
            }

            return vector_size;
        }

        inline void stream_advance(std::ifstream& stream, size_t bytes)
        {
            stream.seekg(static_cast<size_t>(stream.tellg()) + bytes);
        }
    } // namespace util

    namespace detail
    {
        template <size_t Width, typename T>
        inline std::ostream& stream_insert_value(std::ostream& os, std::string label, T value)
        {
            os << std::setw(Width) << label << ": " << std::setw(0) << value;
            return os;
        }

        template <size_t N> size_t constexpr str_length(char const (&)[N]) { return N - 1; }
    } // namespace detail

    inline std::ostream& operator<<(std::ostream& os, Header const& header)
    {
        static auto constexpr labelwidth{detail::str_length("Accumulator Checkpoint")};

        using namespace detail;

        stream_insert_value<labelwidth>(os, "Version", header.version_) << std::endl;
        stream_insert_value<labelwidth>(os, "Hash Previous Block", header.hash_previous_block_) << std::endl;
        stream_insert_value<labelwidth>(os, "Hash Merkle Root", header.hash_merkle_root_) << std::endl;
        stream_insert_value<labelwidth>(os, "Time", header.time_) << std::endl;
        stream_insert_value<labelwidth>(os, "Bits", header.bits_) << std::endl;
        stream_insert_value<labelwidth>(os, "Nonce", header.nonce_) << std::endl;
        stream_insert_value<labelwidth>(os, "Accumulator Checkpoint", header.accumulator_checkpoint_) << std::endl;
        return os;
    }

    /*
    inline std::ostream& operator<<(std::ostream& os, Block const& block)
    {
        os << "Block @ " << block.offset() << " of size " << block.size() << " with header:" << std::endl
           << block.header();
        return os;
    }
    */
} // namespace blockparser
