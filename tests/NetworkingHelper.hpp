#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <random>

namespace networkingHelper {
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


    ssize_t send(const std::string& addr, uint16_t port, const char* data, uint64_t size) {
        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(addr.c_str());
        server.sin_port = htons(port);
        connect(fd, reinterpret_cast<sockaddr*>(&server), sizeof(server)) == 0; // NOLINT
        return ::send(fd, data, size, 0);
    }

    ssize_t send(int16_t port, const char* data, uint64_t size) {
        return send("127.0.0.1", port, data, size);
    }

    ssize_t send(const std::string& addr, uint16_t port, const std::string& data) {
        return send(addr, port, data.c_str(), data.size());
    }

    ssize_t send(uint16_t port, const std::string& data) {
        return send("127.0.0.1", port, data);
    }

}