#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "utils/ByteArray.hpp"

std::string test_string = "ABCDEFGHI";
int test_string_length = 1 + test_string.size();

TEST_CASE("Test AllocatedByteArrayResource") {
    SUBCASE("Create Resource") {
        char* buf = new char[10];
        {
            AllocatedByteArrayResource resource(buf, 10, IByteArrayResource::DeepCopyTag{});
            CHECK_EQ(resource.size(), 10);
            CHECK(memcmp(buf, resource.data(), 10) == 0);
        }
        delete[] buf;
    }

    SUBCASE("Copy Resource") {
        char* buf = new char[test_string_length];
        memcpy(buf, test_string.c_str(), test_string_length);

        AllocatedByteArrayResource a(buf, test_string_length, IByteArrayResource::DeepCopyTag{});
        AllocatedByteArrayResource b(a);

        CHECK(a.data() != b.data());
        CHECK(memcmp(test_string.c_str(), b.data(), test_string_length) == 0);

        delete[] buf;
    }

    SUBCASE("Move Resource") {
        char* buf = new char[test_string_length];
        memcpy(buf, test_string.c_str(), test_string_length);

        AllocatedByteArrayResource a(buf, test_string_length, IByteArrayResource::DeepCopyTag{});
        AllocatedByteArrayResource b = std::move(a);

        CHECK(a.data() == nullptr); // NOLINT
        CHECK(memcmp(test_string.c_str(), b.data(), test_string_length) == 0);
        CHECK_EQ(b.size(), test_string_length);

        delete[] buf;
    }
}

TEST_CASE("Test ByteArray")
{
    SUBCASE("given buffer")
    {
        char* buf = new char[10];
        {
            ByteArray ba = ByteArray::new_allocated_byte_array(buf, 10);
            CHECK_EQ(ba.size(), 10);
            CHECK(memcmp(buf, ba.data(), 10) == 0);
        }
        delete[] buf;
    }

    SUBCASE("create buffer")
    {
        ByteArray ba = ByteArray::new_allocated_byte_array(test_string_length);
        CHECK_EQ(ba.size(), test_string_length);
        memcpy(ba.data(), test_string.c_str(), test_string_length);
        CHECK(memcmp(ba.data(), test_string.c_str(), test_string_length) == 0);
    }

    SUBCASE("handle string lvalue")
    {
        ByteArray ba;
        std::string data_cpy;
        {
            std::string data{"test"};
            data_cpy = data;
            ba = ByteArray::new_allocated_byte_array(data);
        }

        CHECK_EQ(ba.size(), data_cpy.size());
        CHECK(memcmp(ba.data(), data_cpy.c_str(), data_cpy.size()) == 0);
    }

    SUBCASE("handle string rvalue")
    {
        std::string data{"test"};
        ByteArray ba = ByteArray::new_allocated_byte_array(std::string{"test"});
        CHECK_EQ(ba.size(), data.size());
        CHECK(memcmp(ba.data(), data.c_str(), data.size()) == 0);
    }
}