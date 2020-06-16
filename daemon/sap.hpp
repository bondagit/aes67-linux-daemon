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
using boost::asio::deadline_timer;

class SAP {
 public:
  constexpr static uint16_t port = 9875;
  constexpr static uint16_t max_deletions = 3;
  constexpr static uint16_t bandwidth_limit = 4000;  // bits x xsec
  constexpr static uint16_t min_interval = 300;      // secs
  constexpr static uint16_t sap_header_len = 24;
  constexpr static uint16_t max_length = 4096;

  SAP() = delete;
  SAP(const std::string& sap_mcast_addr);

  bool set_multicast_interface(const std::string& interface_ip);
  bool announcement(uint16_t msg_id_hash,
                    uint32_t addr,
                    const std::string& sdp);
  bool deletion(uint16_t msg_id_hash, uint32_t addr, const std::string& sdp);
  bool receive(bool& is_announce,
               uint16_t& msg_id_hash,
               uint32_t& addr,
               std::string& sdp,
               int tout_secs = 1);

 private:
  static void handle_receive(const boost::system::error_code& ec,
                             std::size_t length,
                             boost::system::error_code* out_ec,
                             std::size_t* out_length);
  void check_deadline();
  bool send(bool is_announce,
            uint16_t msg_id_hash,
            uint32_t addr,
            const std::string& sdp);

  std::string addr_;
  io_service io_service_;
  ip::udp::socket socket_{io_service_};
  ip::udp::endpoint remote_endpoint_{
      ip::udp::endpoint(ip::address::from_string(addr_), port)};
  ip::udp::endpoint listen_endpoint_{
      ip::udp::endpoint(ip::address::from_string("0.0.0.0"), port)};
  deadline_timer deadline_{io_service_};
};

#endif
