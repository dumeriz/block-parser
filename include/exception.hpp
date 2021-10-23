//
//  exception.h
//  block-parser
//
//  Copyright © 2019 zenon. All rights reserved.
//

#pragma once

#include "types.hpp"

#include <exception>

namespace blockparser
{

    class exception : public std::exception
    {
        std::string what_;

    public:
        explicit exception(std::string cause) : what_{std::move(cause)} {}

        char const* what() const noexcept override { return what_.c_str(); }
    };

    class pubkey_type_exception : public exception
    {
        std::string type_;
        PubKey key_;
        std::string what_;

    public:
        pubkey_type_exception(std::string type, PubKey key)
            : exception{"Deduced type " + type + " in " + to_string(key)}, type_{std::move(type)}, key_{std::move(key)}
        {
        }

        PubKey const& key() const { return key_; }
        std::string const& type() const { return type_; }
    };

    struct ParseException : public exception
    {
        explicit ParseException(std::string error) : exception{"ParseException: " + error} {}
    };

    struct RedisException : public exception
    {
        explicit RedisException(std::string error) : exception{"RedisException: " + error} {}
    };

} // namespace blockparser
