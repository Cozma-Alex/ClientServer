#include "server.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <p_r> <p_w> <delta_t>" << std::endl;
        return 1;
    }

    try {
        int p_r = std::stoi(argv[1]);
        int p_w = std::stoi(argv[2]);
        int delta_t = std::stoi(argv[3]);

        std::cout << "Starting server with p_r=" << p_r << " p_w=" << p_w 
                  << " delta_t=" << delta_t << std::endl;
        
        boost::asio::io_context io_context;
        CompetitionServer server(io_context, 12345, p_r, p_w, delta_t);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}