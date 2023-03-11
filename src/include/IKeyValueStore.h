#pragma once

#include "Status.h"
#include "ByteArray.h"
#include "Options.h"

#include <string>

class IKeyValueStore
{
public:
    IKeyValueStore() = default;
    ~IKeyValueStore() = default;

    IKeyValueStore(const IKeyValueStore &) = delete;
    IKeyValueStore &operator=(const IKeyValueStore &) = delete;

    virtual Status Put(const std::string &key, const IByteArrayResource &value, const WriteOptions &options) = 0;
    virtual Status Get(const std::string &key, IByteArrayResource &value, const ReadOptions &options) = 0;
    virtual Status Delete(const std::string &key, const ReadOptions &options) = 0;
};