#include "ByteArray.hpp"

#include <cstring>


//AllocatedByteArrayResource
AllocatedByteArrayResource::AllocatedByteArrayResource(uint64_t size) {
    data_ = new char[size];
    size_ = size;
}

// NOLINTNEXTLINE
AllocatedByteArrayResource::AllocatedByteArrayResource(const char* data, uint64_t size, DeepCopyTag tag): AllocatedByteArrayResource(size) {
    std::memcpy(data_, data, size);
}

// NOLINTNEXTLINE
AllocatedByteArrayResource::AllocatedByteArrayResource(char* data, uint64_t size, ShallowCopyTag tag) {
    data_ = data;
    size_ = size;
}

AllocatedByteArrayResource::AllocatedByteArrayResource(const AllocatedByteArrayResource& other):
    AllocatedByteArrayResource(other.data(), other.size(), DeepCopyTag{}) {}

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
    std::memcpy(data_, other.data_, other.size_);
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

void AllocatedByteArrayResource::resize(uint64_t target_size) {
    if (size_ >= target_size) {
        return;
    }

    char* new_data = new char[target_size];
    std::memcpy(new_data, data_, std::min(target_size, size_));
    delete[] data_;
    data_ = new_data;
    size_ = target_size;
}

//ByteArray
ByteArray::ByteArray() {
    resource_ = std::make_shared<AllocatedByteArrayResource>(0);
}

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

void ByteArray::insert_byte_array(const ByteArray& other, uint64_t offset) {
    //If the ByteArray is too small to fit the other ByteArray, the size gets increased
    if (offset + other.size() > size()) {
        resource_ = std::make_shared<AllocatedByteArrayResource>(resource_->data(), size() + other.size(), IByteArrayResource::DeepCopyTag{});
    }
    std::memcpy(resource_->data() + offset, other.data(), other.size());
}

void ByteArray::resize(uint64_t target_size) {
    resource_->resize(target_size);
}

ByteArray ByteArray::new_allocated_byte_array(char* data, uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(data, size, IByteArrayResource::DeepCopyTag{});
    return std::move(ba);
}

ByteArray ByteArray::new_allocated_byte_array(uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(size);
    return std::move(ba);
}

ByteArray ByteArray::new_allocated_byte_array(std::string& data) {
    return ByteArray::new_allocated_byte_array(const_cast<char*>(data.c_str()), data.size());
}

ByteArray ByteArray::new_allocated_byte_array(std::string&& data) {
    return ByteArray::new_allocated_byte_array(const_cast<char*>(data.c_str()), data.size());
}
