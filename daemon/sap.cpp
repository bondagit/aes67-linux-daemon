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

#include <boost/bind.hpp>
#include "sap.hpp"


using namespace boost::asio;
using namespace boost::asio::ip;


SAP::SAP(const std::string& sap_mcast_addr) : 
  addr_(sap_mcast_addr)
//  remote_endpoint_(ip::address::from_string(addr_), port)
{ 
  socket_.open(boost::asio::ip::udp::v4());
  socket_.set_option(udp::socket::reuse_address(true));
  socket_.bind(listen_endpoint_);
  check_deadline();
}

bool SAP::set_multicast_interface(const std::string& interface_ip) {
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
    BOOST_LOG_TRIVIAL(error) << "sap::enable_loopback option " << ec.message();
    return false;
  }
  return true;
}

bool SAP::announcement(uint16_t msg_id_hash,
                       uint32_t addr,
                       const std::string& sdp) {
  BOOST_LOG_TRIVIAL(info) << "sap::announcement " << std::hex << msg_id_hash;
  return send(true, msg_id_hash, htonl(addr), sdp);
}

bool SAP::deletion(uint16_t msg_id_hash,
                   uint32_t addr,
                   const std::string& sdp) {
  BOOST_LOG_TRIVIAL(info) << "sap::deletion " << std::hex << msg_id_hash;
  return send(false, msg_id_hash, htonl(addr), sdp);
}

bool SAP::receive(bool& is_announce,
                  uint16_t& msg_id_hash,
                  uint32_t& addr,
                  std::string& sdp,
                  int tout_secs) {
  // Set a deadline for the asynchronous operation
  deadline_.expires_from_now(boost::posix_time::seconds(tout_secs));

  boost::system::error_code ec = boost::asio::error::would_block;
  ip::udp::endpoint endpoint;
  uint8_t buffer[max_length];
  std::size_t length = 0;
  // Start the asynchronous operation itself. The handle_receive function
  // used as a callback will update the ec and length variables.
  socket_.async_receive_from(
      boost::asio::buffer(buffer), endpoint,
      boost::bind(&SAP::handle_receive, _1, _2, &ec, &length));

  // Block until the asynchronous operation has completed.
  do {
    io_service_.run_one();
  } while (ec == boost::asio::error::would_block);


  if (!ec && length > 4 && (buffer[0] == 0x20 || buffer[0] == 0x24)) {
    // only accept SAP announce or delete v2 with IPv4 
    // no reserved, no compress, no encryption
    // and content/type = application/sdp
    is_announce = (buffer[0] == 0x20);
    memcpy(&msg_id_hash, buffer + 2, sizeof(msg_id_hash));
    memcpy(&addr, buffer + 4, sizeof(addr));
    for (int i = 8; buffer[i] != 0 && i < length; i++) {
      buffer[i] = std::tolower(buffer[i]);
    }
    if (!memcmp(buffer + 8, "application/sdp", 16)) {
      sdp.assign(buffer + SAP::sap_header_len, buffer + length);
      return true;
    }
  }

  return false;
}

void SAP::handle_receive(const boost::system::error_code& ec,
                         std::size_t length,
                         boost::system::error_code* out_ec,
                         std::size_t* out_length) {
  *out_ec = ec;
  *out_length = length;
}

void SAP::check_deadline() {
  if (deadline_.expires_at() <= deadline_timer::traits_type::now()) {
    // cancel receive operation
    socket_.cancel();
    deadline_.expires_at(boost::posix_time::pos_infin);
    //BOOST_LOG_TRIVIAL(debug) << "SAP:: timeout expired when receiving";
  }

  deadline_.async_wait(boost::bind(&SAP::check_deadline, this));
}

bool SAP::send(bool is_announce,
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
    socket_.send_to(boost::asio::buffer(buffer, sap_header_len + sdp.length()),
                    remote_endpoint_);
  } catch (boost::system::error_code& ec) {
    BOOST_LOG_TRIVIAL(error) << "sap::send_to " << ec.message();
    return false;
  }
  return true;
}
