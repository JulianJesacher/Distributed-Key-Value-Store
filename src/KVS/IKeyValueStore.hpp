#pragma once

#include "../utils/Status.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Options.hpp"

#include <string>

class IKeyValueStore
{
public:
    IKeyValueStore(const IKeyValueStore&) = delete;
    IKeyValueStore& operator=(const IKeyValueStore&) = delete;

    virtual Status put(const std::string& key, const ByteArray& value, const WriteOptions& options) = 0;
    virtual Status get(const std::string& key, ByteArray& value, const ReadOptions& options) = 0;
    virtual Status erase(const std::string& key, const WriteOptions& options) = 0;
};
