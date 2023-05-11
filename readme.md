# Distributed Key Value Store

This project is a basic distributed key value store that works in a similar way as redis does. It is completely written in C++ and supports sharding, resharding, slot migration and the availability to make sure that different keys are stored on the same server node. It also includes a client library and a client application that can be used over the cli.

## Installation

### Prerequisites

- C++20 compiler
- Linux (tested on Ubuntu 20.04)
- Boost.Asio
- Boost.Program_options
- CMake

### Building from source

1. Clone the repository:

    `$git clone https://github.com/JulianJesacher/Distributed-Key-Value-Store.git`

2. Change into the project directory:

    `$cd Distributed-Key-Value-Store`

3. Create a build direktory:

    `$mkdir build && cd build`

4. Configure the project using CMake:

    `$cmake ..`

5. Build the project:

    `$make`

Now you find all test in the directory `tests` and the executables for the client-cli and the server nodes in the directory `src`.


## Usage

To work with the system, it's crucial to understand what a slot is. The keyspace is divided into a fixed amount of slots by the system. Each slot is served by exactly one node. The system uses the hash of the key to determine the slot of the key. You can also make sure that several keys are served by the same node. By using a key like `...{key}...`, only the part of the key within the outermost curly braces will be hashed and used to determine the slot of the key.

### Server / Node:

You can start a server / node by executing the `Node` executable with the respective arguments. You can provide the following arguments:

- name: The name of the new node in the cluster
- client_port: The port which the node uses to handle connections with clients
- cluster_port: The port which the node uses for inter-node communication
- serve_all_slots: If this flag is set, the node will serve all keys, otherwise none. For the first node of a cluster this flag should be set to true, for all other nodes it should be set to false.

You can also provide the path to a config file where you can specify the arguments. The config file should be in the following format:

```
name=node
ip=127.0.0.1
client_port=5000
cluster_port=15000
serve_all_slots=false
```

There is also a sample config file in the root directory of the project. If you specify the config file, you don't need to provide any arguments, but if you do, they will overwrite the values in the config file. If you don't specify a config file, the following default values will be used:

```
client_port=5000
cluster_port=15000
serve_all_slots=false
```

### Client:

You can import the client library into your project by linking the `Client_l` library. You can then use the `Client` class to connect to a node and send commands to it. The `Client` class has the following methods:

- `connect_to_node`: Connects to a node
- `add_node_to_cluster`: Adds a node to the cluster
- `disconnect_all`: Disconnects from all nodes
- `put_value`: Puts a value into the key value store
- `get_value`: Gets a value from the key value store
- `erase_value`: Deletes a value from the key value store
- `get_update_slot_info`: Gets and updates the information about which keys are served by which node to accelerate the get and erase operations
- `migrate_slot`: Migrates a given slot to a given node
- `import_slot`: Imports a slot to a given node

You can also use the client-cli application to interact with the system.

### Client-cli:

You can use the client-cli application to interact with the system. You can start the client-cli by executing the `Client-cli` executable without any arguments. You can then use the following commands:

- `connect <ip>`: Connects to a node
- `add_node_to_cluster <name> <ip> <client_port> <cluster_port>`: Adds a node to the cluster
- `disconnect`: Disconnects from all nodes
- `put <key> <value>`: Puts a value into the key value store
- `get <key> <size> <offset>`: Gets a value from the key value store with the given size and offset. If size and offset are not provided, the whole value will be returned.
- `erase <key>`: Deletes a value from the key value store
- `update_slot_info`: Gets and updates the information about which keys are served by which node to accelerate the get and erase operations
- `migrate_slot <slot> <other_ip> <other_client_port>`: Migrates a given slot to a given node
- `import_slot <slot> <other_ip> <other_client_port>`: Imports a slot to a given node
- `help`: Shows the help
- `exit`: Exits the program

