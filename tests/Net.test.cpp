#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <limits>

#include "net/FileDescriptor.hpp"

TEST_CASE("Test FileDescriptor") {
    static_assert(!std::is_copy_assignable_v<net::FileDescriptor>, "FileDescriptor should not be copy assignable");
    static_assert(!std::is_copy_constructible_v<net::FileDescriptor>, "FileDescriptor should not be copy constructible");

    SUBCASE("Default construct FileDescriptor") {
        net::FileDescriptor fd{};

        CHECK_EQ(fd.unwrap(), -1);
    }

    SUBCASE("Construct FileDescriptor and move") {
        net::FileDescriptor fd{4};

        CHECK_EQ(fd.unwrap(), 4);

        auto moved{ std::move(fd) };
        CHECK_EQ(fd.unwrap(), -1); // NOLINT
        CHECK_EQ(moved.unwrap(), 4);
    }
}