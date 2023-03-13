#pragma once

#include "IKeyValueStore.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Status.hpp"

#include <unordered_map>
#include <memory>

class InMemoryKVS: public IKeyValueStore {
public:
    Status put(const std::string& key, const ByteArray& value, const WriteOptions& options) override;
    Status get(const std::string& key, ByteArray& value, const ReadOptions& options) override;
    Status erase(const std::string& key, const WriteOptions& options) override;

private:
    std::unordered_map<std::string, ByteArray> mapping_;
    uint64_t size_;
};
