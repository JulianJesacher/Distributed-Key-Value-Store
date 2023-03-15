#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <limits>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <future>
#include <random>
#include <thread>


#include "net/FileDescriptor.hpp"
#include "net/Socket.hpp"
#include "net/Connection.hpp"

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

std::string get_random_string(uint64_t length) {
    auto rand_char = []() -> char {
        constexpr std::string_view Chars =
            "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        std::mt19937 rg{std::random_device{}()};
        std::uniform_int_distribution<std::string::size_type> pick(0, Chars.size() -
            1);
        return Chars[pick(rg)];
    };

    std::string str(length, 0);
    std::generate_n(str.begin(), length, rand_char);
    return str;
}


ssize_t send_to_localhost(const std::string& data, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::string addr{"127.0.0.1"};

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(addr.c_str());
    server.sin_port = htons(port);
    connect(fd, reinterpret_cast<sockaddr*>(&server), sizeof(server)) == 0; // NOLINT
    return send(fd, data.c_str(), data.size(), 0);
}

TEST_CASE("Test Socket") {
    net::Socket s{};
    CHECK_FALSE(net::is_listening(s.fd()));

    uint16_t port = 3000;
    s.listen(port);
    CHECK(net::is_listening(s.fd()));

    const auto send_data = [&]() {
        std::string random_string{get_random_string(100)};
        send_to_localhost(random_string, port);
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
