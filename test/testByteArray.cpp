#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/ByteArray.h"

std::string testString = "ABCDEFGHI";
int testStringLength = 10;

TEST_CASE("Test AllocatedByteArrayResource") {
    SUBCASE("Create Resource") {
        char* buf = new char[10];
        {
            AllocatedByteArrayResource resource(buf, 10);
            CHECK_EQ(resource.size(), 10);
            CHECK(memcmp(buf, resource.cdata(), 10) == 0);
        }
        delete[] buf;
    }

    SUBCASE("Copy Resource") {
        char* buf = new char[testStringLength];
        memcpy(buf, testString.c_str(), testStringLength);

        AllocatedByteArrayResource a(buf, testStringLength);
        AllocatedByteArrayResource b(a);

        CHECK(a.data() != b.data());
        CHECK(memcmp(testString.c_str(), b.cdata(), testStringLength) == 0);

        delete[] buf;
    }

    SUBCASE("Move Resource") {
        char* buf = new char[testStringLength];
        memcpy(buf, testString.c_str(), testStringLength);

        AllocatedByteArrayResource a(buf, testStringLength);
        AllocatedByteArrayResource b = std::move(a);

        CHECK(a.data() == nullptr);
        CHECK(memcmp(testString.c_str(), b.cdata(), testStringLength) == 0);
        CHECK_EQ(b.size(), testStringLength);

        delete[] buf;
    }
}

TEST_CASE("Test ByteArray")
{
    SUBCASE("given buffer")
    {
        char* buf = new char[10];
        {
            ByteArray ba = ByteArray::newAllocatedByteArray(buf, 10);
            CHECK_EQ(ba.size(), 10);
            CHECK(memcmp(buf, ba.cdata(), 10) == 0);
        }
        delete[] buf;
    }

    SUBCASE("create buffer")
    {
        ByteArray ba = ByteArray::newAllocatedByteArray(testStringLength);
        CHECK_EQ(ba.size(), testStringLength);
        memcpy(ba.data(), testString.c_str(), testStringLength);
        CHECK(memcmp(ba.data(), testString.c_str(), testStringLength) == 0);
    }
}