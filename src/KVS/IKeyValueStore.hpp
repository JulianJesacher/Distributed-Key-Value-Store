#pragma once

#include "../utils/Status.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Options.hpp"

#include <string>

class IKeyValueStore
{
public:
    IKeyValueStore() = default;
    virtual ~IKeyValueStore() = default;

    IKeyValueStore(const IKeyValueStore&) = delete;
    IKeyValueStore& operator=(const IKeyValueStore&) = delete;

    virtual Status put(const std::string& key, const ByteArray& value, const WriteOptions& options = WriteOptions{}) = 0;
    virtual Status get(const std::string& key, ByteArray& value, const ReadOptions& options = ReadOptions{}) const = 0;
    virtual Status erase(const std::string& key, const WriteOptions& options = WriteOptions{}) = 0;

    virtual uint64_t get_size() const = 0;
};
