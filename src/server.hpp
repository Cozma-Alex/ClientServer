#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace competition {

using tcp = boost::asio::ip::tcp;

struct Competitor {
  int country_id;
  int competitor_id;
  int score;
};

struct RankingCache {
  std::chrono::steady_clock::time_point timestamp;
  std::string ranking_data;
};

template <typename T> class BoundedQueue {
private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  size_t capacity_;
  std::atomic<bool> is_active_{true};

public:
  explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}

  void shutdown() {
    is_active_ = false;
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  bool push(T item, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!not_full_.wait_for(lock, timeout, [this] {
          return queue_.size() < capacity_ || !is_active_;
        })) {
      return false;
    }
    if (!is_active_)
      return false;
    queue_.push(std::move(item));
    not_empty_.notify_one();
    return true;
  }

  bool try_pop(T &item) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!not_empty_.wait_for(lock, std::chrono::milliseconds(100), [this] {
          return !queue_.empty() || !is_active_;
        })) {
      return false;
    }
    if (queue_.empty())
      return false;
    item = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    return true;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }
};

class Connection : public std::enable_shared_from_this<Connection> {
private:
  tcp::socket socket_;
  boost::asio::any_io_executor executor_;
  boost::asio::streambuf read_buffer_;
  std::atomic<bool> is_active_{true};

public:
  Connection(tcp::socket socket)
      : socket_(std::move(socket)), executor_(socket_.get_executor()) {}

  template <typename Handler>
  void async_read_until(char delim, Handler &&handler) {
    if (!is_active_)
      return;
    boost::asio::async_read_until(
        socket_, read_buffer_, delim,
        boost::asio::bind_executor(executor_, std::forward<Handler>(handler)));
  }

  template <typename Handler>
  void async_write(const std::string &data, Handler &&handler) {
    if (!is_active_)
      return;
    boost::asio::async_write(
        socket_, boost::asio::buffer(data),
        boost::asio::bind_executor(executor_, std::forward<Handler>(handler)));
  }

  tcp::socket &socket() { return socket_; }

  std::string get_line() {
    std::istream is(&read_buffer_);
    std::string line;
    std::getline(is, line);
    return line;
  }
  
  void shutdown() {
    is_active_ = false;
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);
  }
};

class CompetitionServer {
private:
  boost::asio::io_context &io_context_;
  tcp::acceptor acceptor_;
  boost::asio::thread_pool reader_pool_;
  boost::asio::thread_pool writer_pool_;
  BoundedQueue<Competitor> competitor_queue_;
  std::vector<Competitor> final_ranking_;
  std::mutex ranking_mutex_;
  int delta_t_;
  std::ofstream log_file_;
  RankingCache ranking_cache_;
  std::unordered_map<int, int> country_scores_;
  std::vector<std::shared_ptr<std::promise<std::string>>> ranking_promises_;
  std::mutex promises_mutex_;
  std::atomic<bool> is_running_{true};
  std::mutex connections_mutex_;
  std::set<std::shared_ptr<Connection>> active_connections_;

  void start_accept();
  void handle_connection(std::shared_ptr<Connection> conn);
  void handle_client_data(std::shared_ptr<Connection> conn);
  void handle_messages(std::shared_ptr<Connection> conn, int country_id);
  void process_competitor_data(const std::string &data, int country_id);
  std::shared_future<std::string> request_ranking(int country_id);
  std::string calculate_rankings();
  void send_final_results(std::shared_ptr<Connection> conn);
  void save_final_rankings();
  void process_queue();
  void remove_connection(std::shared_ptr<Connection> conn);
  void log_message(const std::string &msg);

public:
  CompetitionServer(boost::asio::io_context &io_context, short port, int p_r,
                    int p_w, int delta_t);
  ~CompetitionServer();
};

} // namespace competition