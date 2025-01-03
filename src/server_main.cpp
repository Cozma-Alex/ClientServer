#include "server.hpp"
#include <iostream>
#include <csignal>

std::unique_ptr<competition::CompetitionServer> server_ptr;
boost::asio::io_context io_context;

void signal_handler(int signum) {
    if (server_ptr) {
        server_ptr.reset();
    }
    io_context.stop();
    exit(signum);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <p_r> <p_w> <delta_t>" << std::endl;
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        int p_r = std::stoi(argv[1]);
        int p_w = std::stoi(argv[2]);
        int delta_t = std::stoi(argv[3]);

        boost::asio::io_context::work work(io_context);
        
        std::cout << "Starting server with p_r=" << p_r << " p_w=" << p_w 
                  << " delta_t=" << delta_t << std::endl;
        
        server_ptr = std::make_unique<competition::CompetitionServer>(io_context, 12345, p_r, p_w, delta_t);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}