#pragma once

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind.hpp>
#include <deque>
#include <future>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>
#include <condition_variable>
#include <sstream>
#include <iostream>

using boost::asio::ip::tcp;

struct Competitor {
    int country_id;
    int competitor_id;
    int score;
    
    bool operator<(const Competitor& other) const {
        return score < other.score;
    }
};

struct RankingCache {
    std::chrono::steady_clock::time_point timestamp;
    std::string ranking_data;
};

class ThreadSafeQueue {
private:
    std::queue<Competitor> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;

public:
    void push(Competitor item) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(item);
        not_empty_.notify_one();
    }

    bool try_pop(Competitor& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = queue_.front();
        queue_.pop();
        return true;
    }

    void wait_and_pop(Competitor& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty()) {
            not_empty_.wait(lock);
        }
        item = queue_.front();
        queue_.pop();
    }
};

class CompetitionServer {
private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    boost::asio::thread_pool reader_pool_;
    boost::asio::thread_pool writer_pool_;
    ThreadSafeQueue competitor_queue_;
    std::vector<Competitor> final_ranking_;
    std::mutex ranking_mutex_;
    std::ofstream log_file_;
    int delta_t_;
    RankingCache ranking_cache_;
    std::unordered_map<int, int> country_scores_;
    std::vector<std::shared_ptr<std::promise<std::string>>> ranking_promises_;
    std::mutex promises_mutex_;
    std::atomic<bool> is_running_{true};

    void start_accept();
    void handle_connection(std::shared_ptr<tcp::socket> socket);
    void handle_client_data(std::shared_ptr<tcp::socket> socket);
    void process_competitor_data(const std::string& data, int country_id);
    std::shared_future<std::string> request_ranking(int country_id);
    void calculate_rankings();
    void send_final_results(std::shared_ptr<tcp::socket> socket);
    void save_final_rankings();
    void process_queue();

public:
    CompetitionServer(boost::asio::io_context& io_context, short port,
                     int p_r, int p_w, int delta_t);
    ~CompetitionServer();
};