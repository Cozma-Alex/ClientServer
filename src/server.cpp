#include "server.hpp"
#include <thread>

namespace competition {

CompetitionServer::CompetitionServer(boost::asio::io_context& io_context, short port,
                int p_r, int p_w, int delta_t)
    : io_context_(io_context)
    , acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , reader_pool_(p_r)
    , writer_pool_(p_w)
    , competitor_queue_(10000)
    , delta_t_(delta_t)
    , log_file_("server_log.txt") {
    start_accept();
    
    for (int i = 0; i < p_w; ++i) {
        boost::asio::post(writer_pool_, [this]() {
            process_queue();
        });
    }
}

CompetitionServer::~CompetitionServer() {
    is_running_ = false;
    competitor_queue_.shutdown();
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& conn : active_connections_) {
        conn->shutdown();
    }
    active_connections_.clear();
    
    reader_pool_.join();
    writer_pool_.join();
}

void CompetitionServer::start_accept() {
    std::cout << "Waiting for client connection..." << std::endl;
    auto socket = std::make_shared<tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& error) {
        if (!error) {
            std::cout << "Client connected!" << std::endl;
            auto conn = std::make_shared<Connection>(std::move(*socket));
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                active_connections_.insert(conn);
            }
            handle_connection(conn);
        } else {
            std::cout << "Accept error: " << error.message() << std::endl;
        }
        if (is_running_) {
            start_accept();
        }
    });
}

void CompetitionServer::handle_connection(std::shared_ptr<Connection> conn) {
    boost::asio::post(reader_pool_, [this, conn]() {
        handle_client_data(conn);
    });
}

void CompetitionServer::process_competitor_data(const std::string& data, int country_id) {
    std::vector<std::pair<int, int>> competitors;
    std::stringstream ss(data);
    std::string line;
    
    while (std::getline(ss, line)) {
        int id, score;
        if (sscanf(line.c_str(), "%d,%d", &id, &score) == 2) {
            if (!competitor_queue_.push({country_id, id, score}, 
                std::chrono::milliseconds(100))) {
                log_message("Queue full, dropping competitor data");
                return;
            }
        }
    }
    
    log_message("Added competitors from country " + std::to_string(country_id));
}

std::shared_future<std::string> CompetitionServer::request_ranking(int country_id) {
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(ranking_mutex_);
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - ranking_cache_.timestamp).count() < delta_t_) {
            std::promise<std::string> promise;
            promise.set_value(ranking_cache_.ranking_data);
            return promise.get_future();
        }
    }

    std::shared_ptr<std::promise<std::string>> promise = std::make_shared<std::promise<std::string>>();

    boost::asio::post(writer_pool_, [this, promise]() {
        std::string ranking = calculate_rankings();
        promise->set_value(ranking);
    });

    return promise->get_future();
}

std::string CompetitionServer::calculate_rankings() {
    std::vector<std::pair<int, int>> scores;
    {
        std::lock_guard<std::mutex> lock(ranking_mutex_);
        country_scores_.clear();
        for (const auto& competitor : final_ranking_) {
            country_scores_[competitor.country_id] += competitor.score;
        }
        scores = std::vector<std::pair<int, int>>(country_scores_.begin(), country_scores_.end());
    }
    
    std::sort(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::string ranking;
    for (const auto& score : scores) {
        ranking += std::to_string(score.first) + "," + 
                  std::to_string(score.second) + "\n";
    }

    {
        std::lock_guard<std::mutex> lock(ranking_mutex_);
        ranking_cache_.timestamp = std::chrono::steady_clock::now();
        ranking_cache_.ranking_data = ranking;
    }

    return ranking;
}

void CompetitionServer::handle_client_data(std::shared_ptr<Connection> conn) {
    std::cout << "Handling client data" << std::endl;
    try {
        conn->async_read_until('\n', [this, conn](
            const boost::system::error_code& error, std::size_t) {
            if (error) {
                std::cout << "Error reading client data: " << error.message() << std::endl;
                remove_connection(conn);
                return;
            }

            std::string init_msg = conn->get_line();
            std::cout << "Received initial message: " << init_msg << std::endl;
            int country_id = std::stoi(init_msg);
            log_message("Client connected: country " + std::to_string(country_id));
            handle_messages(conn, country_id);
        });
    } catch (std::exception& e) {
        std::cout << "Exception in handle_client_data: " << e.what() << std::endl;
        log_message("Error in client connection: " + std::string(e.what()));
        remove_connection(conn);
    }
}

void CompetitionServer::handle_messages(std::shared_ptr<Connection> conn, int country_id) {
    std::cout << "Handling messages for country " << country_id << std::endl;
    conn->async_read_until('\n', [this, conn, country_id](
        const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cout << "Error reading message: " << error.message() << std::endl;
            remove_connection(conn);
            return;
        }

        std::string msg = conn->get_line();
        std::cout << "Received message from country " << country_id << ": " << msg << std::endl;
        
        if (msg == "REQUEST_RANKING") {
            std::cout << "Processing ranking request from country " << country_id << std::endl;
            auto future = request_ranking(country_id);
            try {
                std::string ranking = future.get();
                conn->async_write(ranking, [this, conn, country_id](
                    const boost::system::error_code& error, std::size_t) {
                    if (!error) {
                        handle_messages(conn, country_id);
                    } else {
                        remove_connection(conn);
                    }
                });
            } catch (std::exception& e) {
                log_message("Error sending ranking: " + std::string(e.what()));
                remove_connection(conn);
            }
        } else if (msg == "FINAL_REQUEST") {
            std::cout << "Processing final request from country " << country_id << std::endl;
            send_final_results(conn);
            remove_connection(conn);
        } else {
            std::cout << "Processing competitor data from country " << country_id << std::endl;
            process_competitor_data(msg, country_id);
            handle_messages(conn, country_id);
        }
    });
}

void CompetitionServer::send_final_results(std::shared_ptr<Connection> conn) {
    std::lock_guard<std::mutex> lock(ranking_mutex_);
    save_final_rankings();
    
    std::ifstream competitor_file("final_competitors.txt");
    std::stringstream competitor_data;
    competitor_data << competitor_file.rdbuf();
    
    std::ifstream country_file("final_countries.txt");
    std::stringstream country_data;
    country_data << country_file.rdbuf();

    conn->async_write(competitor_data.str() + "\n" + country_data.str(),
        [](const boost::system::error_code&, std::size_t) {});
    log_message("Sent final results");
}

void CompetitionServer::save_final_rankings() {
    std::sort(final_ranking_.begin(), final_ranking_.end(),
        [](const Competitor& a, const Competitor& b) {
            return a.score > b.score;
        });

    std::ofstream competitor_file("final_competitors.txt");
    for (const auto& competitor : final_ranking_) {
        competitor_file << competitor.country_id << ","
                       << competitor.competitor_id << ","
                       << competitor.score << "\n";
    }

    std::ofstream country_file("final_countries.txt");
    for (const auto& score : country_scores_) {
        country_file << score.first << "," << score.second << "\n";
    }
}

void CompetitionServer::process_queue() {
    while (is_running_) {
        Competitor comp;
        if (competitor_queue_.try_pop(comp)) {
            std::lock_guard<std::mutex> lock(ranking_mutex_);
            final_ranking_.push_back(comp);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
void CompetitionServer::remove_connection(std::shared_ptr<Connection> conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    conn->shutdown();
    active_connections_.erase(conn);
}

void CompetitionServer::log_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(ranking_mutex_);
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    log_file_ << "[" << std::ctime(&time) << "] " << msg << std::endl;
}

} // namespace competition