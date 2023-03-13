#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "utils/ByteArray.hpp"
#include "KVS/InMemoryKVS.hpp"

std::string test_string = "ABCDEFGHI";
int test_string_length = 1 + test_string.size();

TEST_CASE("Test InMemoryKeyValueStore") {
    SUBCASE("Insert") {
    }
}