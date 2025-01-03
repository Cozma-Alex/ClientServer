#include "client.hpp"

CompetitionClient::CompetitionClient(boost::asio::io_context& io_context,
                 const std::string& host, const std::string& port,
                 int country_id, int delta_x,
                 const std::string& competitors_file)
    : io_context_(io_context),
      socket_(io_context),
      country_id_(country_id),
      delta_x_(delta_x),
      deadline_(io_context) {
    
    tcp::resolver resolver(io_context);
    boost::asio::connect(socket_, resolver.resolve(host, port));
    socket_.set_option(tcp::no_delay(true));
    load_competitors(competitors_file);
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
    check_deadline();
}

void CompetitionClient::check_deadline() {
    if (deadline_.expiry() <= boost::asio::steady_timer::clock_type::now()) {
        socket_.close();
        return;
    }
    deadline_.async_wait([this](const boost::system::error_code&) { check_deadline(); });
}

void CompetitionClient::load_competitors(const std::string& filename) {
    std::ifstream file(filename);
    int id, score;
    while (file >> id >> score) {
        competitors_.push_back({id, score});
    }
}

void CompetitionClient::send_competitor_batch(const std::vector<std::pair<int, int>>& batch) {
    std::stringstream ss;
    for (const auto& comp : batch) {
        ss << comp.first << "," << comp.second << "\n";
    }
    
    deadline_.expires_from_now(boost::asio::chrono::seconds(5));
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(ss.str()), ec);
    if (ec) {
        throw boost::system::system_error(ec);
    }
}

void CompetitionClient::send_competitor_data() {
    deadline_.expires_from_now(boost::asio::chrono::seconds(5));
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(std::to_string(country_id_) + "\n"), ec);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    for (size_t i = 0; i < competitors_.size(); i += 20) {
        size_t batch_size = std::min(size_t(20), competitors_.size() - i);
        std::vector<std::pair<int, int>> batch(
            competitors_.begin() + i,
            competitors_.begin() + i + batch_size
        );
        send_competitor_batch(batch);
        std::this_thread::sleep_for(std::chrono::seconds(delta_x_));
    }
}

std::string CompetitionClient::request_ranking() {
    deadline_.expires_from_now(boost::asio::chrono::seconds(5));
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer("REQUEST_RANKING\n"), ec);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    boost::asio::streambuf response;
    boost::asio::read_until(socket_, response, "\n", ec);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    return std::string(std::istreambuf_iterator<char>(&response),
                      std::istreambuf_iterator<char>());
}

void CompetitionClient::request_final_results() {
    deadline_.expires_from_now(boost::asio::chrono::seconds(5));
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer("FINAL_REQUEST\n"), ec);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    boost::asio::streambuf response;
    boost::asio::read_until(socket_, response, "\n\n", ec);
    if (ec) {
        throw boost::system::system_error(ec);
    }

    std::cout << "Final results for country " << country_id_ << ":\n"
              << std::string(std::istreambuf_iterator<char>(&response),
                           std::istreambuf_iterator<char>()) << std::endl;
}

void CompetitionClient::start_competition() {
    try {
        send_competitor_data();
        std::cout << "Current ranking:\n" << request_ranking();
        request_final_results();
    } catch (std::exception& e) {
        std::cerr << "Client " << country_id_ << " error: " << e.what() << std::endl;
        throw;
    }
}