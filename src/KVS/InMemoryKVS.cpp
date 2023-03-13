#include "InMemoryKVS.hpp"

#include <cstring>

// NOLINTNEXTLINE
Status InMemoryKVS::put(const std::string& key, const ByteArray& value, const WriteOptions& options) {
    mapping_[key] = ByteArray(std::move(value));
    return Status::new_ok();
}

// NOLINTNEXTLINE
Status InMemoryKVS::get(const std::string& key, ByteArray& value, const ReadOptions& options) const {
    if (!mapping_.contains(key)) {
        std::string error_msg{"The given key was not found"};
        return Status::new_not_found(ByteArray::new_allocated_byte_array(error_msg));
    }

    value = mapping_.at(key);
    return Status::new_ok();
}

// NOLINTNEXTLINE
Status InMemoryKVS::erase(const std::string& key, const WriteOptions& options) {
    if (!mapping_.contains(key)) {
        std::string error_msg{"The given key was not found"};
        return Status::new_not_found(ByteArray::new_allocated_byte_array(error_msg));
    }

    mapping_.erase(key);
    return Status::new_ok();
}
