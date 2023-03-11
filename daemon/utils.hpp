//
//  utils.hpp
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

#ifndef _UTILS_HPP_
#define _UTILS_HPP_

#include <httplib.h>

#include <cstddef>
#include <iostream>

uint16_t crc16(const uint8_t* p, size_t len);

std::tuple<bool /* res */,
           std::string /* protocol */,
           std::string /* host */,
           std::string /* port */,
           std::string /* path */>
parse_url(const std::string& _url);

std::string get_host_node_id(uint32_t ip_addr);

std::string sdp_get_subject(const std::string& sdp);

struct SDPOrigin {
  std::string username;
  std::string session_id;
  uint64_t session_version{0};
  std::string network_type;
  std::string address_type;
  std::string unicast_address;

  bool operator==(const SDPOrigin& rhs) const {
    // session_version is not part of comparison, see RFC 4566
    return username == rhs.username && session_id == rhs.session_id &&
           network_type == rhs.network_type &&
           address_type == rhs.address_type &&
           unicast_address == rhs.unicast_address;
  }
};

SDPOrigin sdp_get_origin(const std::string& sdp);

#endif
