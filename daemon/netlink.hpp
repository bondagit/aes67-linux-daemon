//
//  netlink.hpp
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

#ifndef _NETLINK_HPP_
#define _NETLINK_HPP_

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

template <typename Protocol>
class nl_endpoint {
 private:
  sockaddr_nl sockaddr{.nl_family = AF_NETLINK};

 public:
  using protocol_type = Protocol;
  using data_type = boost::asio::detail::socket_addr_type;

  nl_endpoint() {
    sockaddr.nl_groups = 0;
    sockaddr.nl_pid = getpid();
  }

  nl_endpoint(int group, int pid = getpid()) {
    sockaddr.nl_groups = group;
    sockaddr.nl_pid = pid;
  }

  nl_endpoint(const nl_endpoint& other) { sockaddr = other.sockaddr; }

  nl_endpoint& operator=(const nl_endpoint& other) {
    sockaddr = other.sockaddr;
    return *this;
  }

  protocol_type protocol() const { return protocol_type(); }

  data_type* data() { return reinterpret_cast<struct sockaddr*>(&sockaddr); }

  const data_type* data() const {
    return reinterpret_cast<const struct sockaddr*>(&sockaddr);
  }

  std::size_t size() const { return sizeof(sockaddr); }

  void resize(std::size_t size) { /* nothing we can do here */
  }

  std::size_t capacity() const { return sizeof(sockaddr); }
};

class nl_protocol {
 public:
  nl_protocol() { proto = 0; }

  explicit nl_protocol(int proto) { this->proto = proto; }

  int type() const { return SOCK_RAW; }

  int protocol() const { return proto; }

  int family() const { return PF_NETLINK; }

  typedef nl_endpoint<nl_protocol> endpoint;
  typedef boost::asio::basic_raw_socket<nl_protocol> socket;

 private:
  int proto;
};

#endif
