#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <limits>
#include <stdexcept>
#include <future>
#include <random>


#include "net/FileDescriptor.hpp"
#include "net/Socket.hpp"
#include "net/Connection.hpp"
#include "NetworkingHelper.hpp"

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

TEST_CASE("Test Socket") {
    net::Socket s{};
    CHECK_FALSE(net::is_listening(s.fd()));

    uint16_t port = 3000;
    s.listen(port);
    CHECK(net::is_listening(s.fd()));

    const auto send_data = [&]() {
        std::string random_string{networkingHelper::get_random_string(100)};
        networkingHelper::send(port, random_string);
        return random_string;
    };

    const auto recv_data = [&]() {
        char data[100];
        net::Connection c = s.accept();
        net::receive(c.fd(), static_cast<char*>(data), 100);
        return std::string(data, 100);
    };

    auto received = std::async(recv_data);
    auto sent = std::async(send_data);
    CHECK_EQ(received.get(), sent.get());

    //Check if we can send and receive multiple times 
    received = std::async(recv_data);
    sent = std::async(send_data);
    CHECK_EQ(received.get(), sent.get());
}
