//  rtsp_server.cpp
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

#include "utils.hpp"
#include "rtsp_server.hpp"

using boost::asio::ip::tcp;

bool RtspServer::update_source(uint8_t id,
                               const std::string& name,
                               const std::string& sdp) {
  bool ret = false;
  BOOST_LOG_TRIVIAL(debug) << "rtsp_server:: added source " << name;
  std::lock_guard<std::mutex> lock{mutex_};
  for (unsigned int i = 0; i < sessions_.size(); i++) {
    auto session = sessions_[i].lock();
    if (session != nullptr) {
      ret |= session->announce(id, name, sdp, config_->get_ip_addr_str(),
                               config_->get_rtsp_port());
    }
  }
  return ret;
}

void RtspServer::accept() {
  acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
    if (!ec) {
      std::lock_guard<std::mutex> lock{mutex_};
      /* check for free sessions */
      unsigned int i = 0;
      for (; i < sessions_.size(); i++) {
        if (sessions_[i].use_count() == 0) {
          auto session = std::make_shared<RtspSession>(
              config_, session_manager_, std::move(socket_));
          sessions_[i] = session;
          sessions_start_point_[i] = steady_clock::now();
          session->start();
          break;
        }
      }

      if (i == sessions_.size()) {
        BOOST_LOG_TRIVIAL(warning)
            << "rtsp_server:: too many clients connected, "
            << socket_.remote_endpoint() << " closing...";
        socket_.close();
      }
    }
    accept();
  });
}

bool RtspSession::announce(uint8_t id,
                           const std::string& name,
                           const std::string& sdp,
                           const std::string& address,
                           uint16_t port) {
  /* if a describe request is currently not beeing process
   * and the specified source id has been described on this session send update
   */
  if (cseq_ < 0 && source_ids_.find(id) != source_ids_.end()) {
    std::string path(std::string("/by-name/") + config_->get_node_id() + " " +
                     name);
    std::stringstream ss;
    ss << "ANNOUNCE rtsp://" << address << ":" << std::to_string(port)
       << httplib::detail::encode_url(path) << " RTSP/1.0\r\n"
       << "User-Agent: aes67-daemon\r\n"
       << "connection: Keep-Alive"
       << "\r\n"
       << "CSeq: " << announce_cseq_++ << "\r\n"
       << "Content-Length: " << sdp.length() << "\r\n"
       << "Content-Type: application/sdp\r\n"
       << "\r\n"
       << sdp;

    BOOST_LOG_TRIVIAL(info) << "rtsp_server:: "
                            << "ANNOUNCE for source " << name << " sent to "
                            << socket_.remote_endpoint();

    send_response(ss.str());
    return true;
  }
  return false;
}

bool RtspSession::process_request() {
  /*
     DESCRIBE rtsp://127.0.0.1:8080/by-name/test RTSP/1.0
     CSeq: 312
     User-Agent: pippo
     Accept: application/sdp

  */
  data_[length_] = 0;
  std::stringstream sstream(data_);
  /* read the request */
  if (!getline(sstream, request_, '\n')) {
    return false;
  }
  consumed_ = request_.length() + 1;
  boost::trim(request_);
  std::vector<std::string> fields;
  split(fields, request_, boost::is_any_of(" "));
  if (fields.size() < 3) {
    return false;
  }
  /* read the header */
  bool is_end{false};
  std::string header;
  while (getline(sstream, header, '\n')) {
    consumed_ += header.length() + 1;
    if (header == "" || header == "\r") {
      is_end = true;
      break;
    }
    boost::to_lower(header);
    boost::trim(header);
    if (header.rfind("cseq:", 0) != std::string::npos) {
      try {
        cseq_ = stoi(header.substr(5));
      } catch (...) {
        break;
      }
    }
  }

  if (!is_end) {
    return false;
  }

  if (fields[0].substr(0, 5) == "RTSP/") {
    /* we received a response, step to next request*/
    return true;
  } else if (cseq_ < 0) {
    BOOST_LOG_TRIVIAL(error) << "rtsp_server:: CSeq not specified from "
                             << socket_.remote_endpoint();
    send_error(400, "Bad Request");
  } else if (fields[2].substr(0, 5) != "RTSP/") {
    BOOST_LOG_TRIVIAL(error)
        << "rtsp_server:: no RTSP specified from " << socket_.remote_endpoint();
    send_error(400, "Bad Request");
  } else if (fields[0] != "DESCRIBE") {
    send_error(405, "Method Not Allowed");
  } else {
    boost::trim(fields[1]);
    build_response(fields[1]);
  }
  return true;
}

void RtspSession::build_response(const std::string& url) {
  auto const res = parse_url(url);
  if (!std::get<0>(res)) {
    BOOST_LOG_TRIVIAL(error) << "rtsp_server:: cannot parse URL " << url
                             << " from " << socket_.remote_endpoint();
    send_error(400, "Bad Request");
    return;
  }
  const auto& path = std::get<4>(res);
  auto base_path = std::string("/by-name/") + config_->get_node_id() + " ";
  uint8_t id = SessionManager::stream_id_max + 1;
  if (path.rfind(base_path) != std::string::npos) {
    /* extract the source name from path and retrive the id */
    id = session_manager_->get_source_id(path.substr(base_path.length()));
  } else if (path.rfind("/by-id/") != std::string::npos) {
    try {
      id = (uint8_t)stoi(path.substr(7));
    } catch (...) {
      id = SessionManager::stream_id_max + 1;
      ;
    }
  }
  if (id != (SessionManager::stream_id_max + 1)) {
    std::string sdp;
    if (!session_manager_->get_source_sdp(id, sdp)) {
      std::stringstream ss;
      ss << "RTSP/1.0 200 OK\r\n"
         << "CSeq: " << cseq_ << "\r\n"
         << "Content-Length: " << sdp.length() << "\r\n"
         << "Content-Type: application/sdp\r\n"
         << "\r\n"
         << sdp;
      BOOST_LOG_TRIVIAL(info)
          << "rtsp_server:: " << request_ << " response 200 to "
          << socket_.remote_endpoint();
      send_response(ss.str());
      source_ids_.insert(id);
      return;
    }
  }
  send_error(404, "Not found");
}

void RtspSession::read_request() {
  auto self(shared_from_this());
  if (length_ == max_length) {
    /* request cannot be consumed and we exceeded max length */
    stop();
  } else {
    socket_.async_read_some(
        boost::asio::buffer(data_ + length_, max_length - length_),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            BOOST_LOG_TRIVIAL(debug) << "rtsp_server:: received " << length
                                     << " from " << socket_.remote_endpoint();
            length_ += length;
            while (length_ && process_request()) {
              /* step to the next request */
              std::memmove(data_, data_ + consumed_, length_ - consumed_);
              length_ -= consumed_;
              cseq_ = -1;
            }
            /* read more data */
            read_request();
          }
        });
  }
}

void RtspSession::send_error(int status_code, const std::string& description) {
  BOOST_LOG_TRIVIAL(error) << "rtsp_server:: " << request_ << " response "
                           << status_code << " to "
                           << socket_.remote_endpoint();
  std::stringstream ss;
  ss << "RTSP/1.0 " << status_code << " " << description << "\r\n";
  if (cseq_ >= 0) {
    ss << "CSeq: " << cseq_ << "\r\n";
  }
  ss << "\r\n";
  send_response(ss.str());
}

void RtspSession::send_response(const std::string& response) {
  auto self(shared_from_this());
  boost::asio::async_write(
      socket_, boost::asio::buffer(response.c_str(), response.length()),
      [self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
          // we accept multiple requests within timeout
          // stop();
        }
      });
}

void RtspSession::start() {
  BOOST_LOG_TRIVIAL(debug) << "rtsp_server:: starting session with "
                           << socket_.remote_endpoint();
  read_request();
}

void RtspSession::stop() {
  BOOST_LOG_TRIVIAL(debug) << "rtsp_server:: stopping session with "
                           << socket_.remote_endpoint();
  socket_.close();
}
