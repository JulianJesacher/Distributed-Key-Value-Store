#pragma once

#include <cstdint>
#include <memory>
#include <string>

/*
class IByteArrayResource
{
public:
    //Tags to select the correct constructor
    struct DeepCopyTag {};
    struct ShallowCopyTag {};

    virtual ~IByteArrayResource()=default;
    virtual char* data() = 0;
    virtual const char* cdata() = 0;
    virtual uint64_t size() = 0;
};

class AllocatedByteArrayResource: public IByteArrayResource {
public:

    AllocatedByteArrayResource(const char* data, uint64_t size, IByteArrayResource::DeepCopyTag);
    AllocatedByteArrayResource(char* data, uint64_t size, IByteArrayResource::ShallowCopyTag);
    AllocatedByteArrayResource(uint64_t size);

    ~AllocatedByteArrayResource();

    char* data() override;
    const char* cdata() override;
    uint64_t size() override;

private:
    char* data_;
    uint64_t size_;
};

class PointerByteArrayResource: public IByteArrayResource {
public:

    PointerByteArrayResource(const char* data, uint64_t size);
    ~PointerByteArrayResource() override = default;

    char* data() override;
    const char* cdata() override;
    uint64_t size() override;

private:
    const char* data_;
    uint64_t size_;
};

class ByteArray {
public:

    ByteArray() = default;
    ~ByteArray() = default;

    char* data() const;
    const char* cdata() const;
    uint64_t size() const;
    std::string toString() const;

    static ByteArray newShallowCopy(char* data, uint64_t size);
    static ByteArray newDeepCopy(const char* data, uint64_t size);
    static ByteArray newDeepCopy(const std::string& str);

    bool operator==(const ByteArray& right) const;

private:
    uint64_t size_;
    uint64_t offset_;
    std::shared_ptr<IByteArrayResource> resource_;
};

*/

class IByteArrayResource
{
public:
    virtual ~IByteArrayResource()=default;
    virtual char* data() const = 0;
    virtual const char* cdata() const = 0;
    virtual uint64_t size() const = 0;
};

class AllocatedByteArrayResource: public IByteArrayResource {
public:

    AllocatedByteArrayResource(const char* data, uint64_t size);
    AllocatedByteArrayResource(uint64_t size);

    AllocatedByteArrayResource(const AllocatedByteArrayResource& other);
    AllocatedByteArrayResource& operator=(AllocatedByteArrayResource& other);

    AllocatedByteArrayResource(AllocatedByteArrayResource&& other);
    AllocatedByteArrayResource& operator=(AllocatedByteArrayResource&& other);

    ~AllocatedByteArrayResource() override;

    char* data() const override {
        return data_;
    };
    const char* cdata() const override {
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

    static ByteArray newAllocatedByteArray(uint64_t size);
    static ByteArray newAllocatedByteArray(char* data, uint64_t size);

    char* data() const {
        return resource_->data();
    }
    const char* cdata() const {
        return resource_->cdata();
    }
    uint64_t size() const {
        return resource_->size();
    }
    std::string toString() const {
        return std::string(resource_->data(), resource_->size());
    }

private:
    std::shared_ptr<IByteArrayResource> resource_;
};