//
//  sap.hpp
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

#ifndef _SAP_HPP_
#define _SAP_HPP_

#include <boost/asio.hpp>
#include "log.hpp"

using namespace boost::asio;

class SAP {
 public:
  constexpr static const char addr[] = "224.2.127.254";
  constexpr static uint16_t port = 9875;
  constexpr static uint16_t max_deletions = 3;
  constexpr static uint16_t bandwidth_limit = 4000;  // bits x xsec
  constexpr static uint16_t min_interval = 300;      // secs
  constexpr static uint16_t sap_header_len = 24;
  constexpr static uint16_t max_length = 1024;

  SAP() { socket_.open(boost::asio::ip::udp::v4()); };

  bool set_multicast_interface(const std::string& interface_ip) {
    ip::address_v4 local_interface = ip::address_v4::from_string(interface_ip);
    ip::multicast::outbound_interface oi_option(local_interface);
    boost::system::error_code ec;
    socket_.set_option(oi_option, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error)
          << "sap::outbound_interface option " << ec.message();
      return false;
    }
    ip::multicast::enable_loopback el_option(true);
    socket_.set_option(el_option, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error)
          << "sap::enable_loopback option " << ec.message();
      return false;
    }
    return true;
  }

  bool announcement(uint16_t msg_id_hash,
                    uint32_t addr,
                    const std::string& sdp) {
    BOOST_LOG_TRIVIAL(info) << "sap::announcement " << std::hex << msg_id_hash;
    return send(true, msg_id_hash, htonl(addr), sdp);
  }

  bool deletion(uint16_t msg_id_hash, uint32_t addr, const std::string& sdp) {
    BOOST_LOG_TRIVIAL(info) << "sap::deletetion " << std::hex << msg_id_hash;
    return send(false, msg_id_hash, htonl(addr), sdp);
  }

 private:
  bool send(bool is_announce,
            uint16_t msg_id_hash,
            uint32_t addr,
            const std::string& sdp) {
    if (sdp.length() > max_length - sap_header_len) {
      BOOST_LOG_TRIVIAL(error) << "sap:: SDP is too long";
      return false;
    }
    uint8_t buffer[max_length];

    buffer[0] = is_announce ? 0x20 : 0x24;
    buffer[1] = 0;
    memcpy(buffer + 2, &msg_id_hash, 2);
    memcpy(buffer + 4, &addr, 4);
    memcpy(buffer + 8, "application/sdp", 16); /* include trailing 0 */
    memcpy(buffer + sap_header_len, sdp.c_str(), sdp.length());

    try {
      socket_.send_to(
          boost::asio::buffer(buffer, sap_header_len + sdp.length()), remote_);
    } catch (boost::system::error_code& ec) {
      BOOST_LOG_TRIVIAL(error) << "sap::send_to " << ec.message();
      return false;
    }
    return true;
  }

  io_service io_service_;
  ip::udp::socket socket_{io_service_};
  ip::udp::endpoint remote_{
      ip::udp::endpoint(ip::address::from_string(addr), port)};
};

#endif
