/*#include "../include/ByteArray.h"

#include <cstring>

//AllocatedByteArrayResource
AllocatedByteArrayResource::AllocatedByteArrayResource(const char* data, uint64_t size, DeepCopyTag): AllocatedByteArrayResource(size) {
    memcpy(data_, data, size);
}

AllocatedByteArrayResource::AllocatedByteArrayResource(char* data, uint64_t size, ShallowCopyTag) {
    data_ = std::move(data);
    size_ = size;
}

AllocatedByteArrayResource::AllocatedByteArrayResource(uint64_t size) {
    data_ = new char[size];
    size_ = size;
}

AllocatedByteArrayResource::~AllocatedByteArrayResource() {
    delete[] data_;
}

char* AllocatedByteArrayResource::data() {
    return data_;
}

const char* AllocatedByteArrayResource::cdata() {
    return data_;
}

uint64_t AllocatedByteArrayResource::size() {
    return size_;
}


//PointerByteArrayResource
PointerByteArrayResource::PointerByteArrayResource(const char* data, uint64_t size): data_{ data }, size_{ size } {}

char* PointerByteArrayResource::data() {
    return const_cast<char*>(data_);
};

const char* PointerByteArrayResource::cdata() {
    return data_;
};

uint64_t PointerByteArrayResource::size() {
    return size_;
};


//ByteArray
char* ByteArray::data() const {
    return resource_->data() + offset_;
}

const char* ByteArray::cdata() const {
    return resource_->cdata() + offset_;
}

uint64_t ByteArray::size() const {
    return size_;
}

ByteArray ByteArray::newShallowCopy(char* data, uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(data, size, IByteArrayResource::ShallowCopyTag{});
    ba.size_ = size;
    return ba;
}

ByteArray ByteArray::newDeepCopy(const char* data, uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(data, size, IByteArrayResource::DeepCopyTag{});
    ba.size_ = size;
    return ba;
}

ByteArray ByteArray::newDeepCopy(const std::string& str) {
    return ByteArray::newDeepCopy(str.c_str(), str.size());
}
*/


#include "../include/ByteArray.h"

#include <cstring>


//AllocatedByteArrayResource
AllocatedByteArrayResource::AllocatedByteArrayResource(uint64_t size) {
    data_ = new char[size];
    size_ = size;
}

AllocatedByteArrayResource::AllocatedByteArrayResource(const char* data, uint64_t size): AllocatedByteArrayResource(size) {
    memcpy(data_, data, size);
}

AllocatedByteArrayResource::AllocatedByteArrayResource(const AllocatedByteArrayResource& other): AllocatedByteArrayResource(other.cdata(), other.size()) {}

AllocatedByteArrayResource::AllocatedByteArrayResource(AllocatedByteArrayResource&& other) {
    data_ = std::move(other.data_);
    size_ = std::move(other.size_);
    other.data_ = nullptr;
}

AllocatedByteArrayResource& AllocatedByteArrayResource::operator=(AllocatedByteArrayResource& other) {
    data_ = new char[other.size_];
    size_ = other.size_;
    memcpy(data_, other.data_, other.size_);
    return *this;
}

AllocatedByteArrayResource& AllocatedByteArrayResource::operator=(AllocatedByteArrayResource&& other) {
    data_ = std::move(other.data_);
    size_ = std::move(other.size_);
    other.data_ = nullptr;
    return *this;
}

AllocatedByteArrayResource::~AllocatedByteArrayResource() {
    delete[] data_;
}


//ByteArray
ByteArray ByteArray::newAllocatedByteArray(char* data, uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(data, size);
    return ba;
}

ByteArray ByteArray::newAllocatedByteArray(uint64_t size) {
    ByteArray ba{};
    ba.resource_ = std::make_shared<AllocatedByteArrayResource>(size);
    return ba;
}