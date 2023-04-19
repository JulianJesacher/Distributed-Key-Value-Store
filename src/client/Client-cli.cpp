#include <iostream>

#include "Client.hpp"

int main(int argc, char** argv) {
    client::Client client{};
    client.connect_to_node("127.0.0.1", 5000);

    auto status = client.put_value("key", "value");
    std::cout << status.is_ok() << std::endl;

    //ByteArray value;
    //status = client.get_value("key", value);
    //std::cout << status.is#ok() << std::endl;
    //std::cout << value.to_string() << std::endl;

    client.disconnect_all();
    return 0;
}