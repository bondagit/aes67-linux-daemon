//
//  igmp.hpp
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

#ifndef _IGMP_HPP_
#define _IGMP_HPP_

#include <boost/asio.hpp>
#include <map>
#include <mutex>

#include "log.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::system;

class IGMP {
 public:
  IGMP() {
    socket_.open(listen_endpoint_.protocol());
    socket_.set_option(udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint_);
  };

  bool join(const std::string& interface_ip, const std::string& mcast_ip) {
    uint32_t mcast_ip_addr =
        ip::make_address(mcast_ip.c_str()).to_v4().to_uint();
    std::scoped_lock<std::mutex> lock{mutex};

    auto it = mcast_ref.find(mcast_ip_addr);
    if (it != mcast_ref.end() && (*it).second > 0) {
      (*it).second++;
      return true;
    }

    error_code ec;
    ip::multicast::join_group option(ip::make_address(mcast_ip).to_v4(),
                                     ip::make_address(interface_ip).to_v4());
    socket_.set_option(option, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "igmp:: failed to joined multicast group "
                               << mcast_ip << " " << ec.message();
      return false;
    }

    ip::multicast::enable_loopback el_option(true);
    socket_.set_option(el_option, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error)
          << "igmp:: enable loopback option " << ec.message();
    }

    BOOST_LOG_TRIVIAL(info) << "igmp:: joined multicast group " << mcast_ip
                            << " on " << interface_ip;
    mcast_ref[mcast_ip_addr] = 1;
    return true;
  }

  bool leave(const std::string& interface_ip, const std::string& mcast_ip) {
    uint32_t mcast_ip_addr =
        ip::make_address(mcast_ip.c_str()).to_v4().to_uint();
    std::scoped_lock<std::mutex> lock{mutex};

    auto it = mcast_ref.find(mcast_ip_addr);
    if (it == mcast_ref.end() || (*it).second == 0) {
      return false;
    }

    if (--(*it).second > 0) {
      return true;
    }

    error_code ec;
    ip::multicast::leave_group option(ip::make_address(mcast_ip).to_v4(),
                                      ip::make_address(interface_ip).to_v4());
    socket_.set_option(option, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "igmp:: failed to leave multicast group "
                               << mcast_ip << " " << ec.message();
      return false;
    }

    BOOST_LOG_TRIVIAL(info)
        << "igmp:: left multicast group " << mcast_ip << " on " << interface_ip;
    return true;
  }

 private:
  io_context io_service_;
  ip::udp::socket socket_{io_service_};
  udp::endpoint listen_endpoint_{udp::endpoint(address_v4::any(), 0)};
  std::unordered_map<uint32_t, int> mcast_ref;
  std::mutex mutex;
};

#endif
