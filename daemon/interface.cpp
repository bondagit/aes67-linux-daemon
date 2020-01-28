//
//  interface.cpp
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
//  MIT License
//

#include <utility>
#include <boost/asio.hpp>
#include "log.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

std::pair<uint32_t, std::string> get_interface_ip(
    const std::string& interface_name) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "Cannot retrieve IP address for interface " << interface_name;
    return std::make_pair(0, "");
  }
  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
  if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
    close(fd);
    BOOST_LOG_TRIVIAL(error)
        << "Cannot retrieve IP address for interface " << interface_name;
    return std::make_pair(0, "");
  }
  close(fd);

  struct sockaddr_in* sockaddr =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  uint32_t addr = ntohl(sockaddr->sin_addr.s_addr);
  std::string str_addr(ip::address_v4(addr).to_string());
  /*BOOST_LOG_TRIVIAL(debug) << "interface " << interface_name << " IP address "
                          << str_addr;*/

  return std::make_pair(addr, str_addr);
}

std::pair<std::array<uint8_t, 6>, std::string> get_interface_mac(
    const std::string& interface_name) {
  std::array<uint8_t, 6> mac{0x01, 0x00, 0x5e, 0x01, 0x00, 0x01};
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "Cannot retrieve MAC address for interface " << interface_name;
    return std::make_pair(mac, "");
  }

  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
  if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
    close(fd);
    BOOST_LOG_TRIVIAL(error)
        << "Cannot retrieve MAC address for interface " << interface_name;
    return std::make_pair(mac, "");
  }
  close(fd);

  if (*ifr.ifr_hwaddr.sa_data != 0) {
    uint8_t* sa = reinterpret_cast<uint8_t*>(ifr.ifr_hwaddr.sa_data);
    std::copy(sa, sa + 8, std::begin(mac));
  }

  char str_mac[18];
  sprintf(str_mac, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);
  /*BOOST_LOG_TRIVIAL(debug) << "interface " << interface_name << " MAC address "
                          << str_mac;*/

  return std::make_pair(mac, str_mac);
}
