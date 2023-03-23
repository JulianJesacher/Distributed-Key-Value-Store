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
        ~InMemoryKVS() override = default;

        Status put(const std::string& key, const ByteArray& value, const WriteOptions& options = WriteOptions{}) noexcept override;
        Status get(const std::string& key, ByteArray& value, const ReadOptions& options = ReadOptions{}) const noexcept override;
        Status erase(const std::string& key, const WriteOptions& options = WriteOptions{}) noexcept override;
        bool contains_key(const std::string& key) const noexcept override;

        uint64_t get_size() const {
            return mapping_.size();
        }

    private:
        std::unordered_map<std::string, ByteArray> mapping_;
    };

}