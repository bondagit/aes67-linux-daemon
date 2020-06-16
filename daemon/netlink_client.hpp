//
//  netlink_client.hpp
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
//

#ifndef _NETLINK_CLIENT_HPP_
#define _NETLINK_CLIENT_HPP_

#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <cstdlib>
#include <iostream>

#include "netlink.hpp"

using boost::asio::deadline_timer;

class NetlinkClient {
 public:
  NetlinkClient() = delete;
  NetlinkClient(const std::string& name) : name_(name) {}

  void init(const nl_endpoint<nl_protocol>& listen_endpoint,
            const nl_protocol& protocol) {
    socket_.open(protocol);
    socket_.bind(listen_endpoint);
    deadline_.expires_at(boost::posix_time::pos_infin);
    check_deadline();
  }

  void terminate() { socket_.close(); }

  std::size_t receive(const boost::asio::mutable_buffer& buffer,
                      boost::posix_time::time_duration timeout,
                      boost::system::error_code& ec) {
    // Set a deadline for the asynchronous operation.
    deadline_.expires_from_now(timeout);

    ec = boost::asio::error::would_block;
    std::size_t length = 0;
    // Start the asynchronous operation itself. The handle_receive function
    // used as a callback will update the ec and length variables.
    nl_endpoint<nl_protocol> endpoint;
    socket_.async_receive_from(
        boost::asio::buffer(buffer), endpoint,
        boost::bind(&NetlinkClient::handle_receive, _1, _2, &ec, &length));

    // Block until the asynchronous operation has completed.
    do {
      io_service_.run_one();
    } while (ec == boost::asio::error::would_block);

    return length;
  }

  boost::asio::basic_raw_socket<nl_protocol>& get_socket() { return socket_; }

 private:
  void check_deadline() {
    if (deadline_.expires_at() <= deadline_timer::traits_type::now()) {
      socket_.cancel();
      deadline_.expires_at(boost::posix_time::pos_infin);
      // BOOST_LOG_TRIVIAL(debug) << "netlink_client:: (" << name_ << ") timeout
      // expired";
    }

    deadline_.async_wait(boost::bind(&NetlinkClient::check_deadline, this));
  }

  static void handle_receive(const boost::system::error_code& ec,
                             std::size_t length,
                             boost::system::error_code* out_ec,
                             std::size_t* out_length) {
    *out_ec = ec;
    *out_length = length;
  }

 private:
  boost::asio::io_service io_service_;
  boost::asio::basic_raw_socket<nl_protocol> socket_{io_service_};
  deadline_timer deadline_{io_service_};
  std::string name_;
};

#endif
