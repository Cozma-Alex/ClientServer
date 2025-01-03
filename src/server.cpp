#include "server.hpp"

CompetitionServer::CompetitionServer(boost::asio::io_context& io_context, short port,
                 int p_r, int p_w, int delta_t)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      reader_pool_(p_r),
      writer_pool_(p_w),
      delta_t_(delta_t),
      log_file_("server_log.txt") {
    start_accept();
    
    for (int i = 0; i < p_w; ++i) {
        boost::asio::post(writer_pool_, [this]() {
            process_queue();
        });
    }
}

CompetitionServer::~CompetitionServer() {
    is_running_ = false;
    reader_pool_.join();
    writer_pool_.join();
}

void CompetitionServer::start_accept() {
    auto socket = std::make_shared<tcp::socket>(io_context_);
    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            if (!error) {
                handle_connection(socket);
            }
            start_accept();
        });
}

void CompetitionServer::handle_connection(std::shared_ptr<tcp::socket> socket) {
    boost::asio::post(reader_pool_, [this, socket]() {
        handle_client_data(socket);
    });
}

void CompetitionServer::process_competitor_data(const std::string& data, int country_id) {
    std::vector<std::pair<int, int>> competitors;
    std::stringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        int id, score;
        if (sscanf(line.c_str(), "%d,%d", &id, &score) == 2) {
            competitors.push_back({id, score});
        }
    }

    boost::asio::post(writer_pool_, [this, competitors, country_id]() {
        for (const auto& comp : competitors) {
            competitor_queue_.push({country_id, comp.first, comp.second});
        }
        log_file_ << "Added " << competitors.size() << " competitors from country " << country_id << std::endl;
    });
}

std::shared_future<std::string> CompetitionServer::request_ranking(int country_id) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - ranking_cache_.timestamp).count() < delta_t_) {
        std::promise<std::string> promise;
        promise.set_value(ranking_cache_.ranking_data);
        return promise.get_future();
    }

    auto promise = std::make_shared<std::promise<std::string>>();
    auto future = promise->get_future();

    {
        std::lock_guard<std::mutex> lock(promises_mutex_);
        ranking_promises_.push_back(promise);
    }

    boost::asio::post(writer_pool_, [this, country_id]() {
        calculate_rankings();
    });

    return future;
}

void CompetitionServer::calculate_rankings() {
    std::string ranking;
    {
        std::lock_guard<std::mutex> lock(ranking_mutex_);
        country_scores_.clear();
        
        for (const auto& competitor : final_ranking_) {
            country_scores_[competitor.country_id] += competitor.score;
        }

        for (const auto& score : country_scores_) {
            ranking += std::to_string(score.first) + "," + 
                      std::to_string(score.second) + "\n";
        }
    }

    ranking_cache_.timestamp = std::chrono::steady_clock::now();
    ranking_cache_.ranking_data = ranking;

    std::vector<std::shared_ptr<std::promise<std::string>>> promises;
    {
        std::lock_guard<std::mutex> lock(promises_mutex_);
        promises = std::move(ranking_promises_);
        ranking_promises_.clear();
    }

    for (auto& promise : promises) {
        promise->set_value(ranking);
    }
}

void CompetitionServer::handle_client_data(std::shared_ptr<tcp::socket> socket) {
    int country_id = -1;
    try {
        boost::asio::streambuf buf;
        boost::asio::read_until(*socket, buf, "\n");
        std::string init_msg{std::istreambuf_iterator<char>(&buf),
                           std::istreambuf_iterator<char>()};
        country_id = std::stoi(init_msg);
        log_file_ << "Client connected: country " << country_id << std::endl;

        while (is_running_) {
            boost::asio::streambuf data_buf;
            boost::asio::read_until(*socket, data_buf, "\n");
            std::string msg{std::istreambuf_iterator<char>(&data_buf),
                          std::istreambuf_iterator<char>()};

            if (msg == "REQUEST_RANKING") {
                auto future = request_ranking(country_id);
                std::string ranking = future.get();
                boost::asio::write(*socket, boost::asio::buffer(ranking));
                log_file_ << "Sent ranking to country " << country_id << std::endl;
            } else if (msg == "FINAL_REQUEST") {
                send_final_results(socket);
                break;
            } else {
                process_competitor_data(msg, country_id);
            }
        }
    } catch (std::exception& e) {
        log_file_ << "Error handling country " << country_id << ": " << e.what() << std::endl;
    }
}

void CompetitionServer::send_final_results(std::shared_ptr<tcp::socket> socket) {
    std::lock_guard<std::mutex> lock(ranking_mutex_);
    save_final_rankings();
    
    std::ifstream competitor_file("final_competitors.txt");
    std::string competitor_data((std::istreambuf_iterator<char>(competitor_file)),
                               std::istreambuf_iterator<char>());
    
    std::ifstream country_file("final_countries.txt");
    std::string country_data((std::istreambuf_iterator<char>(country_file)),
                            std::istreambuf_iterator<char>());

    boost::asio::write(*socket, boost::asio::buffer(competitor_data + "\n" + country_data));
    log_file_ << "Sent final results" << std::endl;
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
        competitor_queue_.wait_and_pop(comp);
        std::lock_guard<std::mutex> lock(ranking_mutex_);
        final_ranking_.push_back(comp);
    }
}
