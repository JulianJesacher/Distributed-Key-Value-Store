#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <fstream>

#include "node.hpp"

using std::cout;
using Node = node::Node;
namespace po = boost::program_options;

uint16_t default_client_port{ 5000 };
uint16_t default_cluster_port{ 15000 };
bool default_serve_all_slots{ false };

std::string name;
std::string ip;
uint16_t client_port;
uint16_t cluster_port;
bool serve_all_slots;

int main(int argc, char** argv) {
    po::options_description generic_options("Generic options");
    generic_options.add_options()
        ("help,h", "Print help message")
        ("config,c", po::value<std::string>(), "Optional path for a config file.\nDeclare options in this file like '<option_name>=<option_value>'");

    po::options_description config_options("Configuration options");
    config_options.add_options()
        ("name", po::value<std::string>(&name), "Name of the node.")
        ("ip", po::value<std::string>(&ip), "IP of the node.")
        ("client_port", po::value<uint16_t>(&client_port)->default_value(default_client_port), "Port for the client")
        ("cluster_port", po::value<uint16_t>(&cluster_port)->default_value(default_cluster_port), "Port for the cluster")
        ("serve_all_slots", po::value<bool>(&serve_all_slots)->default_value(default_serve_all_slots), "Specifies if the created node serves all slots (used for the first node of a cluster)");

    po::options_description cmd_line_options("Allowed options");
    cmd_line_options.add(generic_options).add(config_options);

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, cmd_line_options), vm);
    }
    catch (boost::wrapexcept<boost::program_options::unknown_option> e) {
        cout << cmd_line_options << std::endl; //Print help
        return 1;
    }

    if (vm.count("config")) {
        std::ifstream ifs{vm["config"].as<std::string>().c_str()};
        if (ifs) {
            store(parse_config_file(ifs, config_options), vm);
        }
        else {
            cout << "Could not find/open config file" << std::endl;
            return 1;
        }
    }
    po::notify(vm);

    if (vm.count("help")) {
        cout << cmd_line_options << std::endl;
        return 1;
    }

    if (vm.count("name")) {
        cout << "Node name was set to " << vm["name"].as<std::string>() << "." << std::endl;
    }
    else {
        cout << "No name was set." << std::endl;
        return 1;
    }
    if (vm.count("ip")) {
        cout << "Ip was set to " << vm["ip"].as<std::string>() << "." << std::endl;
    }
    else {
        cout << "No ip was set." << std::endl;
        return 1;
    }
    if (vm.count("client_port")) {
        cout << "Using port '" << vm["client_port"].as<uint16_t>() << "' as client port." << std::endl;
    }
    if (vm.count("cluster_port")) {
        cout << "Using port '" << vm["cluster_port"].as<uint16_t>() << "' as cluster port." << std::endl;
    }
    if (vm.count("serve_all_slots")) {
        std::string value_string = vm["serve_all_slots"].as<bool>() ? "true" : "false";
        cout << "Option to serve all slots set to '" << value_string << "'." << std::endl;
    }

    cout << std::endl << "Starting node..." << std::endl;
    auto node = Node::new_in_memory_node(name, client_port, cluster_port, ip, serve_all_slots);
    node.start();
}