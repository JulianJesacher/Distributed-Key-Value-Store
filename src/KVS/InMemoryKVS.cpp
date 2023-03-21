#include "InMemoryKVS.hpp"

using InMemoryKVS = key_value_store::InMemoryKVS;

// NOLINTNEXTLINE
Status InMemoryKVS::put(const std::string& key, const ByteArray& value, const WriteOptions& options) noexcept {
    mapping_[key] = ByteArray(std::move(value));
    return Status::new_ok();
}

// NOLINTNEXTLINE
Status InMemoryKVS::get(const std::string& key, ByteArray& value, const ReadOptions& options) const noexcept {
    if (!mapping_.contains(key)) {
        return Status::new_not_found("The given key was not found");
    }

    value = mapping_.at(key);
    return Status::new_ok();
}

// NOLINTNEXTLINE
Status InMemoryKVS::erase(const std::string& key, const WriteOptions& options) noexcept {
    if (!mapping_.contains(key)) {
        return Status::new_not_found("The given key was not found");
    }

    mapping_.erase(key);
    return Status::new_ok();
}

bool InMemoryKVS::contains_key(const std::string& key) const noexcept{
    return mapping_.contains(key);
}
