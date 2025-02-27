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

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <utility>

#include "log.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

std::pair<uint32_t, std::string> get_interface_ip(
    const std::string& interface_name) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    BOOST_LOG_TRIVIAL(warning)
        << "Cannot retrieve IP address for interface " << interface_name;
    return {0, ""};
  }
  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
  if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
    close(fd);
    BOOST_LOG_TRIVIAL(warning)
        << "Cannot retrieve IP address for interface " << interface_name;
    return {0, ""};
  }
  close(fd);

  struct sockaddr_in* sockaddr =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  uint32_t addr = ntohl(sockaddr->sin_addr.s_addr);
  std::string str_addr(ip::address_v4(addr).to_string());
  /*BOOST_LOG_TRIVIAL(debug) << "interface " << interface_name
                             << " IP address " << str_addr;*/

  return {addr, str_addr};
}

std::pair<std::array<uint8_t, 6>, std::string> get_interface_mac(
    const std::string& interface_name) {
  std::array<uint8_t, 6> mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "Cannot retrieve MAC address for interface " << interface_name;
    return {mac, ""};
  }

  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
  if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
    close(fd);
    BOOST_LOG_TRIVIAL(error)
        << "Cannot retrieve MAC address for interface " << interface_name;
    return {mac, ""};
  }
  close(fd);

  uint8_t* sa = reinterpret_cast<uint8_t*>(ifr.ifr_hwaddr.sa_data);
  std::copy(sa, sa + 6, std::begin(mac));

  char str_mac[18];
  sprintf(str_mac, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);
  /*BOOST_LOG_TRIVIAL(debug) << "interface " << interface_name
                             << " MAC address " << str_mac;*/

  return {mac, str_mac};
}

int get_interface_index(const std::string& interface_name) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    BOOST_LOG_TRIVIAL(warning)
        << "Cannot retrieve index for interface " << interface_name;
    return -1;
  }
  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    close(fd);
    BOOST_LOG_TRIVIAL(warning)
        << "Cannot retrieve index for interface " << interface_name;
    return -1;
  }
  close(fd);
  /*BOOST_LOG_TRIVIAL(debug) << "interface " << interface_name
                             << " index " << ifr.ifr_ifindex;*/

  return ifr.ifr_ifindex;
}

std::pair<std::array<uint8_t, 6>, std::string> get_mac_from_arp_cache(
    const std::string& interface_name,
    const std::string& ip) {
  const std::string arpProcPath("/proc/net/arp");
  std::array<uint8_t, 6> mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::ifstream stream(arpProcPath);

  while (stream) {
    std::string line;
    std::vector<std::string> tokens;

    std::getline(stream, line);
    if (line.find(ip) == std::string::npos) {
      continue;
    }
    boost::split(tokens, line, boost::is_any_of(" "), boost::token_compress_on);
    /* check that IP is on the correct interface */
    if (tokens.size() >= 6 && tokens[5] == interface_name) {
      std::vector<std::string> vec;
      /* parse MAC */
      boost::split(vec, tokens[3], boost::is_any_of(":"));
      int j = 0;
      bool check = false;
      for (auto const& item : vec) {
        mac[j] = strtol(item.c_str(), nullptr, 16);
        check = check | (mac[j] != 0);
        j++;
      }
      if (check) {
        return {mac, tokens[3]};
      }
    }
  }
  BOOST_LOG_TRIVIAL(debug)
      << "get_mac_from_arp_cache:: cannot retrieve MAC for IP " << ip
      << " on interface " << interface_name;
  return {mac, ""};
}

bool ping(const std::string& ip) {
  static uint16_t sequence_number(0);
  uint16_t identifier(0xABAB);
  uint8_t buffer[10];

  // Create an ICMP header for an echo request.
  buffer[0] = 0x8;                          // echo request
  buffer[1] = 0x0;                          // code
  memcpy(buffer + 2, &identifier, 2);       // identifier
  memcpy(buffer + 4, &sequence_number, 2);  // sequence number
  memcpy(buffer + 6, "ping", 4);            // body

  // this requires root priv
  try {
    io_context io_service;
    icmp::socket socket{io_service, icmp::v4()};
    ip::icmp::endpoint destination(ip::icmp::v4(),
                                   ip::make_address(ip).to_v4().to_ulong());
    socket.send_to(boost::asio::buffer(buffer, sizeof buffer), destination);
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "ping:: send_to() failed";
    return false;
  }
  return true;
}

bool echo_try_connect(const std::string& ip) {
  ip::tcp::iostream s;
  BOOST_LOG_TRIVIAL(debug) << "echo_connect:: connecting to " << ip;
#if BOOST_VERSION < 106600
  s.expires_from_now(boost::posix_time::seconds(1));
#else
  s.expires_after(boost::asio::chrono::seconds(1));
#endif
  s.connect(ip, "7");
  if (!s || s.error()) {
    BOOST_LOG_TRIVIAL(debug) << "echo_connect:: unable to connect to " << ip;
    return false;
  }
  s.close();
  return true;
}
