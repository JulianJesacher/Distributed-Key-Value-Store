#pragma once

#include "IKeyValueStore.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Status.hpp"

#include <unordered_map>
#include <memory>

namespace key_value_store {

    class InMemoryKVS: public IKeyValueStore {
    public:
        InMemoryKVS() = default;
        InMemoryKVS(const InMemoryKVS&) = delete;
        InMemoryKVS& operator=(const InMemoryKVS&) = delete;

        Status put(const std::string& key, const ByteArray& value, const WriteOptions& options = WriteOptions{}) override;
        Status get(const std::string& key, ByteArray& value, const ReadOptions& options = ReadOptions{}) const override;
        ByteArray& get(const std::string& key, const ReadOptions& options = ReadOptions{}) override;
        Status erase(const std::string& key, const WriteOptions& options = WriteOptions{}) override;
        bool contains_key(const std::string& key) const override;

        uint64_t get_size() const {
            return mapping_.size();
        }

    private:
        std::unordered_map<std::string, ByteArray> mapping_;
    };

}