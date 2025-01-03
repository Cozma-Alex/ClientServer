#include "client.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <country_id> <delta_x> <competitors_file>" << std::endl;
        return 1;
    }

    try {
        int country_id = std::stoi(argv[1]);
        int delta_x = std::stoi(argv[2]);
        std::string competitors_file = argv[3];

        std::cout << "Starting client for country " << country_id 
                  << " with delta_x=" << delta_x << std::endl;

        boost::asio::io_context io_context;
        CompetitionClient client(io_context, "localhost", "12345", 
                               country_id, delta_x, competitors_file);
        client.start_competition();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}