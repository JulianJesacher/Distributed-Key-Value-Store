#include "ByteArray.hpp"

#include <cstring>


//AllocatedByteArrayResource
AllocatedByteArrayResource::AllocatedByteArrayResource(uint64_t size) {
    data_ = new char[size];
    size_ = size;
}

AllocatedByteArrayResource::AllocatedByteArrayResource(const char* data, uint64_t size): AllocatedByteArrayResource(size) {
    memcpy(data_, data, size);
}

AllocatedByteArrayResource::AllocatedByteArrayResource(const AllocatedByteArrayResource& other): AllocatedByteArrayResource(other.data(), other.size()) {}

AllocatedByteArrayResource::AllocatedByteArrayResource(AllocatedByteArrayResource&& other) noexcept {
    data_ = std::move(other.data_);
    size_ = std::move(other.size_);
    other.data_ = nullptr;
}

AllocatedByteArrayResource& AllocatedByteArrayResource::operator=(const AllocatedByteArrayResource& other) {
    if (&other == this) {
        return *this;
    }

    data_ = new char[other.size_];
    size_ = other.size_;
    memcpy(data_, other.data_, other.size_);
    return *this;
}

AllocatedByteArrayResource& AllocatedByteArrayResource::operator=(AllocatedByteArrayResource&& other) noexcept {
    if (&other == this) {
        return *this;
    }

    data_ = std::move(other.data_);
    size_ = std::move(other.size_);
    other.data_ = nullptr;
    return *this;
}

AllocatedByteArrayResource::~AllocatedByteArrayResource() {
    delete[] data_;
}


//ByteArray
ByteArray::ByteArray(ByteArray&& other) noexcept {
    resource_ = std::move(other.resource_);
    other.resource_ = nullptr;
}

ByteArray& ByteArray::operator=(ByteArray&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    resource_ = std::move(other.resource_);
    other.resource_ = nullptr;
    return *this;
}

ByteArray ByteArray::new_allocated_byte_array(char* data, uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(data, size);
    return ba;
}

ByteArray ByteArray::new_allocated_byte_array(uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(size);
    return ba;
}

ByteArray ByteArray::new_allocated_byte_array(std::string& data) {
    return ByteArray::new_allocated_byte_array(const_cast<char*>(data.c_str()), data.size());
}

ByteArray ByteArray::new_allocated_byte_array(std::string&& data) {
    return ByteArray::new_allocated_byte_array(const_cast<char*>(data.c_str()), data.size());
}
