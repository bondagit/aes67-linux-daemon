//
//  utils.cpp
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
//

#include "utils.hpp"
#include "log.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

uint16_t crc16(const uint8_t* p, size_t len) {
  uint8_t x;
  uint16_t crc = 0xFFFF;

  while (len--) {
    x = crc >> 8 ^ *p++;
    x ^= x >> 4;
    crc = (crc << 8) ^ (static_cast<uint16_t>(x << 12)) ^
          (static_cast<uint16_t>(x << 5)) ^ (static_cast<uint16_t>(x));
  }
  return crc;
}

std::tuple<bool /* res */,
           std::string /* protocol */,
           std::string /* host */,
           std::string /* port */,
           std::string /* path */>
parse_url(const std::string& _url) {
  std::string url = httplib::detail::decode_url(_url, false);
  size_t protocol_sep_pos = url.find_first_of("://");
  if (protocol_sep_pos == std::string::npos) {
    /* no protocol, invalid URL */
    return {false, "", "", "", ""};
  }

  std::string port, host, path("/");
  std::string protocol = url.substr(0, protocol_sep_pos);
  std::string url_new = url.substr(protocol_sep_pos + 3);
  size_t path_sep_pos = url_new.find_first_of("/");
  size_t port_sep_pos = url_new.find_first_of(":");
  if (port_sep_pos != std::string::npos) {
    /* port specified */
    if (path_sep_pos != std::string::npos) {
      /* path specified */
      port = url_new.substr(port_sep_pos + 1, path_sep_pos - port_sep_pos - 1);
      path = url_new.substr(path_sep_pos);
    } else {
      /* path not specified */
      port = url_new.substr(port_sep_pos + 1);
    }
    host = url_new.substr(0, port_sep_pos);
  } else if (path_sep_pos != std::string::npos) {
    /* port not specified, path specified */
    host = url_new.substr(0, path_sep_pos);
    path = url_new.substr(path_sep_pos);
  } else {
    /* port and path not specified */
    host = url_new;
  }
  return {host.length() > 0, protocol, host, port, path};
}

std::string get_host_node_id(uint32_t ip_addr) {
  std::stringstream ss;
  ip_addr = htonl(ip_addr);
  /* we create an host ID based on the current IP */
  ss << "Daemon "
     << boost::format("%08x") % ((ip_addr << 16) | (ip_addr >> 16));
  return ss.str();
}

std::string sdp_get_subject(const std::string& sdp) {
  std::stringstream sstrem(sdp);
  std::string line;
  while (getline(sstrem, line, '\n')) {
    if (line.substr(0, 2) == "s=") {
      auto subject = line.substr(2);
      boost::trim(subject);
      return subject;
    }
  }
  return "";
}

SDPOrigin sdp_get_origin(const std::string& sdp) {
  SDPOrigin origin;
  try {
    std::stringstream sstream(sdp);
    std::string line;
    while (getline(sstream, line, '\n')) {
      boost::trim(line);
      if (line[1] != '=') {
        BOOST_LOG_TRIVIAL(error) << "session_manager:: invalid SDP file";
        break;
      }
      std::string val = line.substr(2);
      if (line[0] == 'o') {
        std::vector<std::string> fields;
        boost::split(fields, val, [line](char c) { return c == ' '; });
        if (fields.size() < 6) {
          BOOST_LOG_TRIVIAL(error) << "session_manager:: invalid origin";
          break;
        }

        origin.username = fields[0];
        origin.session_id = fields[1];
        origin.session_version = std::stoull(fields[2]);
        origin.network_type = fields[3];
        origin.address_type = fields[4];
        origin.unicast_address = fields[5];
        break;
      }
    }
  } catch (...) {
    BOOST_LOG_TRIVIAL(fatal) << "session_manager:: invalid SDP"
                             << ", cannot extract SDP identifier";
  }

  return origin;
}
