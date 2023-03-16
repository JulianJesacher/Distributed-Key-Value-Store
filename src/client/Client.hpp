#include "../net/Connection.hpp"
#include "../net/Socket.hpp"

namespace client {

    class Client {
    public:
        Client();
        net::Connection connect(const std::string& destination, uint16_t port);
        net::Connection connect(uint16_t port);

    private:
        std::optional<net::Socket> socket_{};
    };

}