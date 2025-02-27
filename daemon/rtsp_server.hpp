//  rtsp_server.hpp
//
//  Copyright (c) 2019 2020 Andrea Bondavalli. All rights reserved.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _RTSP_SERVER_HPP_
#define _RTSP_SERVER_HPP_

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

#include "session_manager.hpp"

using namespace std::chrono;
using boost::asio::deadline_timer;
using boost::asio::ip::tcp;
using second_t = duration<double, std::ratio<1> >;

class RtspSession : public std::enable_shared_from_this<RtspSession> {
 public:
  constexpr static uint16_t max_length = 4096;       // byte
  constexpr static uint16_t session_tout_secs = 10;  // sec

  RtspSession(std::shared_ptr<Config> config,
              std::shared_ptr<SessionManager> session_manager,
              tcp::socket socket)
      : config_(config),
        session_manager_(session_manager),
        socket_(std::move(socket)) {}

  virtual ~RtspSession() {
    BOOST_LOG_TRIVIAL(debug) << "rtsp_server:: session end";
  }

  void start();
  void stop();

  bool announce(uint8_t source_id,
                const std::string& name,
                const std::string& sdp,
                const std::string& address,
                uint16_t port);

 private:
  bool process_request();
  void build_response(const std::string& url);
  void read_request();
  void send_error(int status_code, const std::string& description);
  void send_response(const std::string& response);
  std::shared_ptr<Config> config_;
  std::shared_ptr<SessionManager> session_manager_;
  tcp::socket socket_;
  char data_[max_length + 1];
  std::string request_;
  size_t length_{0};
  int32_t cseq_{-1};
  size_t consumed_{0};
  int32_t announce_cseq_{0};
  /* set with the ids described on this session */
  std::unordered_set<uint8_t> source_ids_;
};

class RtspServer {
 public:
  constexpr static uint8_t session_num_max{(SessionManager::stream_id_max + 1) *
                                           2};

  RtspServer() = delete;
  explicit RtspServer(std::shared_ptr<SessionManager> session_manager,
                      std::shared_ptr<Config> config)
      : session_manager_(session_manager),
        config_(config),
        acceptor_(io_service_,
                  tcp::endpoint(
                      boost::asio::ip::make_address(config_->get_ip_addr_str()),
                      config_->get_rtsp_port())) {}
  bool init() {
    accept();
    /* start rtsp server on a separate thread */
    res_ = std::async([this]() { io_service_.run(); });

    session_manager_->add_source_observer(
        SessionManager::SourceObserverType::add_source,
        std::bind(&RtspServer::update_source, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));

    session_manager_->add_source_observer(
        SessionManager::SourceObserverType::update_source,
        std::bind(&RtspServer::update_source, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));

    return true;
  }

  bool terminate() {
    BOOST_LOG_TRIVIAL(info) << "rtsp_server: stopping ... ";
    io_service_.stop();
    res_.get();
    return true;
  }

 private:
  /* a source was updated */
  bool update_source(uint8_t id,
                     const std::string& name,
                     const std::string& sdp);
  void accept();

  std::mutex mutex_;
  boost::asio::io_context io_service_;
  std::shared_ptr<SessionManager> session_manager_;
  std::shared_ptr<Config> config_;
  std::vector<std::weak_ptr<RtspSession> > sessions_{session_num_max};
  std::vector<time_point<steady_clock> > sessions_start_point_{session_num_max};
  tcp::acceptor acceptor_;
  tcp::socket socket_{io_service_};
  std::future<void> res_;
};

#endif
