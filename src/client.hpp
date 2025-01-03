#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>

using boost::asio::ip::tcp;

class CompetitionClient {
private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    int country_id_;
    int delta_x_;
    std::vector<std::pair<int, int>> competitors_;
    boost::asio::steady_timer deadline_;

    void load_competitors(const std::string& filename);
    void send_competitor_batch(const std::vector<std::pair<int, int>>& batch);
    void send_competitor_data();
    std::string request_ranking();
    void request_final_results();
    void check_deadline();

public:
    CompetitionClient(boost::asio::io_context& io_context,
                     const std::string& host, const std::string& port,
                     int country_id, int delta_x,
                     const std::string& competitors_file);
    void start_competition();
};