#pragma once

#include <cstdint>
#include <memory>
#include <string>

class IByteArrayResource
{
public:
    class CopyTag {};
    class ShallowCopyTag: public CopyTag {};
    class DeepCopyTag: public CopyTag {};

    virtual ~IByteArrayResource() = default;
    virtual char* data() = 0;
    virtual const char* data() const = 0;
    virtual uint64_t size() const = 0;
};

class AllocatedByteArrayResource: public IByteArrayResource {
public:
    AllocatedByteArrayResource(uint64_t size);
    AllocatedByteArrayResource(const char* data, uint64_t size, DeepCopyTag tag);
    AllocatedByteArrayResource(char* data, uint64_t size, ShallowCopyTag tag);

    AllocatedByteArrayResource(const AllocatedByteArrayResource& other);
    AllocatedByteArrayResource& operator=(const AllocatedByteArrayResource& other);

    AllocatedByteArrayResource(AllocatedByteArrayResource&& other) noexcept;
    AllocatedByteArrayResource& operator=(AllocatedByteArrayResource&& other) noexcept;

    ~AllocatedByteArrayResource() override;

    char* data() override {
        return data_;
    };
    const char* data() const override {
        return data_;
    };
    uint64_t size() const override {
        return size_;
    };

private:
    char* data_;
    uint64_t size_;
};

class ByteArray {
public:

    ByteArray() = default;
    ByteArray(const ByteArray& other) = default;
    ByteArray& operator=(const ByteArray& other) = default;
    ByteArray(ByteArray&& other) noexcept;
    ByteArray& operator=(ByteArray&& other) noexcept;

    static ByteArray new_allocated_byte_array(uint64_t size);
    static ByteArray new_allocated_byte_array(char* data, uint64_t size);
    static ByteArray new_allocated_byte_array(std::string& data);
    static ByteArray new_allocated_byte_array(std::string&& data);

    char* data() {
        return resource_->data();
    }
    const char* data() const {
        return resource_->data();
    }
    uint64_t size() const {
        return resource_->size();
    }
    std::string to_string() const {
        return std::string(resource_->data(), resource_->size());
    }

private:
    std::shared_ptr<IByteArrayResource> resource_;
};