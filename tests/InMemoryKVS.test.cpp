#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "utils/ByteArray.hpp"
#include "utils/Status.hpp"
#include "KVS/InMemoryKVS.hpp"

std::string test_string = "ABCDEFGHI";
int test_string_length = 1 + test_string.size();

TEST_CASE("Test InMemoryKeyValueStore") {
    key_value_store::InMemoryKVS kvs{};

    SUBCASE("Put") {
        ByteArray ba = ByteArray::new_allocated_byte_array(test_string);
        std::string key{"key"};
        Status status = kvs.put(key, ba);

        CHECK_EQ(kvs.get_size(), 1);
        CHECK(status.is_ok());
    }

    SUBCASE("Get") {
        ByteArray insert_ba = ByteArray::new_allocated_byte_array(test_string);
        std::string key{"key"};

        ByteArray get_ba = ByteArray::new_allocated_byte_array(100);
        kvs.put(key, insert_ba);
        Status status = kvs.get(key, get_ba);

        CHECK_EQ(kvs.get_size(), 1);
        CHECK(status.is_ok());
        CHECK(memcmp(insert_ba.data(), get_ba.data(), insert_ba.size()) == 0);

        //Get key that has not been inserted
        std::string other_key{"other_key"};
        status = kvs.get(other_key, get_ba);
        CHECK(status.is_not_found());
        CHECK(memcmp(insert_ba.data(), get_ba.data(), insert_ba.size()) == 0); //Buffer should not change
    }

    SUBCASE("Erase") {
        ByteArray insert_ba = ByteArray::new_allocated_byte_array(test_string);
        std::string key{"key"};
        kvs.put(key, insert_ba);

        CHECK_EQ(kvs.get_size(), 1);

        Status status = kvs.erase(key);

        CHECK_EQ(kvs.get_size(), 0);
        CHECK(status.is_ok());

        status = kvs.erase(key);
        CHECK_EQ(kvs.get_size(), 0);
        CHECK(status.is_not_found());
    }
}